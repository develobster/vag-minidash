"""Read-only command line entrypoint for live research, never coding or flashing."""
import argparse
from contextlib import ExitStack
from datetime import datetime, timezone
import json
from pathlib import Path
import sys
import time
import uuid

from .api import State, serve
from .discovery import inventory
from .profiles import decode, load_profile, match_catalog, match_fingerprint, resolve_signal
from .uds import NegativeResponse, ReadClient

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_VEHICLE = ROOT / "data/vehicles/tiguan-5na-czpa-dq500.json"


def _read_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))


def _did(text):
    text = text.strip().removeprefix("$").removeprefix("0x").removeprefix("0X")
    if len(text) != 4:
        raise ValueError("DID must be exactly four hex characters")
    return f"{int(text, 16):04X}"


def _load_vehicle(path):
    path = Path(path).resolve()
    vehicle = _read_json(path)
    if vehicle.get("schema_version") != 1 or not isinstance(vehicle.get("ecu_profiles"), dict):
        raise ValueError("Invalid vehicle profile")
    return path, vehicle


def _profile_for(vehicle_path, vehicle, ecu):
    try:
        relative = vehicle["ecu_profiles"][ecu]
    except KeyError as exc:
        raise ValueError(f"Unknown ECU alias {ecu!r}; use one of {', '.join(vehicle['ecu_profiles'])}") from exc
    profile_path = (vehicle_path.parent / relative).resolve()
    profile = load_profile(profile_path)
    if profile.get("ecu_alias") != ecu:
        raise ValueError("Vehicle profile alias does not match ECU profile")
    return profile, profile_path


def _connection(config, ecu):
    base = dict(config.get("transport", {}))
    selected = config.get("ecus", {}).get(ecu, {})
    if not isinstance(base, dict) or not isinstance(selected, dict):
        raise ValueError("transport/ecus must be JSON objects")
    merged = {**base, **selected}
    kind = merged.get("kind")
    if kind == "replay":
        if not merged.get("fixture"):
            raise ValueError("Replay transport needs a fixture path")
        return merged
    if kind not in ("socketcan", "j2534", "d_pdu", "doip"):
        raise ValueError("Transport kind must be socketcan, j2534, replay, d_pdu or doip")
    if not merged.get("address_evidence"):
        raise ValueError("Record address_evidence for this ECU; no physical CAN IDs are guessed")
    for name in ("txid", "rxid", "bitrate"):
        if name not in merged:
            raise ValueError(f"Missing {name} for ECU {ecu}")
    return merged


def _open_transport(connection, config_dir):
    kind = connection["kind"]
    if kind == "replay":
        from .replay import ReplayTransport
        return ReplayTransport(config_dir / connection["fixture"])
    if kind == "d_pdu":
        raise OSError("D-PDU discovery may be used, but opening it is blocked until local VNCI MVCI module/resource/protocol evidence is captured")
    if kind == "doip":
        raise OSError("DoIP is unavailable until a VNCI endpoint, routing activation and logical addresses are evidenced")
    txid, rxid = int(str(connection["txid"]), 16), int(str(connection["rxid"]), 16)
    from .transports import validate_can
    validate_can(txid, rxid, connection["bitrate"])
    if kind == "j2534":
        from .j2534 import J2534Transport
        return J2534Transport(connection["dll"], txid, rxid, connection["bitrate"])
    from .transports import CanTransport
    return CanTransport(connection["channel"], txid, rxid, connection["bitrate"])


def run_probe(args):
    if not 1 <= args.rounds <= 10000:
        raise ValueError("rounds must be 1..10000")
    if not args.trace and not args.log:
        raise ValueError("--trace is required")
    trace_path = Path(args.trace or args.log)
    vehicle_path, vehicle = _load_vehicle(args.vehicle)
    profile, profile_path = _profile_for(vehicle_path, vehicle, args.ecu)
    config_path = Path(args.config).resolve()
    config = _read_json(config_path)
    connection = _connection(config, args.ecu)
    observed_fingerprint = config.get("fingerprint_observation")
    if observed_fingerprint is not None and not isinstance(observed_fingerprint, dict):
        raise ValueError("fingerprint_observation must be an object when supplied")
    identity_verified = bool(observed_fingerprint and match_fingerprint(profile, observed_fingerprint))
    requested = list(dict.fromkeys(_did(value) for value in args.did))
    state, run_id = State(), str(uuid.uuid4())
    with ExitStack() as resources:
        output = resources.enter_context(open(trace_path, "x", encoding="utf-8", buffering=1))
        def emit(event):
            record = {"timestamp": datetime.now(timezone.utc).isoformat(), "monotonic": time.monotonic(),
                      "run_id": run_id, "vehicle_profile": vehicle["id"], "ecu": profile["logical_ecu"],
                      "ecu_alias": args.ecu,
                      "selected_profile_fingerprint": profile.get("fingerprint", {}),
                      "observed_fingerprint": observed_fingerprint,
                      "identity_verified": identity_verified, **event}
            output.write(json.dumps(record, ensure_ascii=False, allow_nan=False) + "\n")
            state.update(record)
            return record
        emit({"kind": "session", "mode": "read_only_probe", "config": config,
              "profile_path": str(profile_path), "profile_snapshot": profile,
              "selected_dids": requested, "identity_verified": identity_verified,
              "fingerprint_observation": observed_fingerprint,
              "note": "Exact fingerprint matched only when fingerprint_observation is complete and equal. Raw UDS identity layouts remain research data; vehicle title never authorizes a match."})
        if args.port is not None:
            server, thread = serve(state, args.port)
            resources.callback(server.server_close)
            resources.callback(thread.join, 2)
            resources.callback(server.shutdown)
            print(f"UI: http://127.0.0.1:{server.server_port}", file=sys.stderr)
        transport = _open_transport(connection, config_path.parent)
        resources.callback(transport.close)
        client = ReadClient(transport, trace=emit)
        emit({"kind": "state", "status": "reading"})
        failures = False
        for _ in range(args.rounds):
            for did in requested:
                item = resolve_signal(profile, did)
                try:
                    exchange = client.read_did_exchange(int(did, 16))
                    reading = decode(item, exchange.payload, identity_verified=identity_verified)
                    record = emit({"kind": "uds_exchange", "service": "0x22", "did": did,
                                   "request": exchange.request.hex(), "response": exchange.response.hex(),
                                   "pending_count": exchange.pending_count, "decoded": reading})
                    emit({"kind": "signal", **reading})
                    if args.raw:
                        print(json.dumps(record, ensure_ascii=False), flush=True)
                except NegativeResponse as exc:
                    failures = True
                    emit({"kind": "error", "service": "0x22", "did": did, "nrc": exc.code, "message": str(exc)})
                except Exception as exc:
                    failures = True
                    emit({"kind": "error", "service": "0x22", "did": did, "message": str(exc), "type": type(exc).__name__})
                    if client.tainted:
                        break
            if client.tainted:
                break
        emit({"kind": "state", "status": "finished_with_errors" if failures else "finished"})
        return 1 if failures else 0


def main(argv=None):
    parser = argparse.ArgumentParser(description="VAG MiniDash read-only PC diagnostic reader")
    commands = parser.add_subparsers(dest="command", required=True)
    doctor = commands.add_parser("doctor", help="Inventory PC API evidence only; never opens a VCI")
    doctor.add_argument("--dll", action="append", default=[])
    doctor.add_argument("--output")
    listing = commands.add_parser("profile", help="Validate and print an ECU profile")
    listing.add_argument("path")
    matcher = commands.add_parser("match", help="Exact fingerprint matching; accepts only captured/evidenced values")
    matcher.add_argument("--fingerprint", required=True, help="JSON object with fingerprint fields")
    matcher.add_argument("--profile-dir", default=str(ROOT / "data/ecu_profiles"))
    probe = commands.add_parser("probe", aliases=["read"], help="Bounded read-only UDS ReadDataByIdentifier probe")
    probe.add_argument("--config", required=True, help="Connection JSON: transport plus verified ECU addresses")
    probe.add_argument("--vehicle", default=str(DEFAULT_VEHICLE))
    probe.add_argument("--ecu", required=True, help="Vehicle ECU alias: engine, transmission, abs, awd")
    probe.add_argument("--did", action="append", required=True, help="Four-digit hex DID; repeatable; unknown DID remains raw")
    probe.add_argument("--rounds", type=int, default=1)
    probe.add_argument("--trace", help="New JSONL trace path; existing files are never overwritten")
    probe.add_argument("--log", help=argparse.SUPPRESS)
    probe.add_argument("--raw", action="store_true", help="Also print each complete UDS exchange to stdout")
    probe.add_argument("--port", type=int, help="Optional loopback read-only signal API/UI")
    args = parser.parse_args(argv)
    try:
        if args.command == "doctor":
            content = json.dumps(inventory(args.dll), ensure_ascii=False, indent=2)
            if args.output:
                Path(args.output).write_text(content, encoding="utf-8")
            print(content)
            return 0
        if args.command == "profile":
            print(json.dumps(load_profile(args.path), ensure_ascii=False, indent=2))
            return 0
        if args.command == "match":
            observed = _read_json(args.fingerprint)
            paths = sorted(Path(args.profile_dir).glob("*.json"))
            print(json.dumps({"matches": [p["id"] for p in match_catalog(paths, observed)]}, ensure_ascii=False))
            return 0
        return run_probe(args)
    except (ValueError, KeyError, TypeError, OSError, json.JSONDecodeError) as exc:
        parser.error(str(exc))


if __name__ == "__main__":
    raise SystemExit(main())
