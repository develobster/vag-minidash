"""Read-only command line entrypoint for live research, never coding or flashing."""
import argparse
import json
import sys
import tempfile
import time
import uuid
from contextlib import ExitStack
from dataclasses import asdict
from datetime import datetime, timezone
from importlib import metadata, util
from pathlib import Path

from .api import State, serve
from .dbc import load_cantools_database
from .discovery import (
    DEFAULT_IDENTITY_PROBES,
    DiscoverySnapshot,
    IdentityProbe,
    discover_vehicle,
    inventory,
    verify_discovered_identity,
)
from .display_bridge import UI_SIGNAL_NAMES, open_serial_display_bridge
from .instruments import PROFILES as INSTRUMENT_PROFILES, select_instrument_profile
from .identity import EcuIdentity, VehicleIdentity
from .knowledge import registry_from_providers
from .pipeline import (
    AcquisitionPlanner,
    RuntimeEngine,
    SourceAvailability,
    signal_catalog,
)
from .profiles import (
    decode,
    load_profile,
    match_catalog,
    match_fingerprint,
    resolve_signal,
)
from .runtime import EcuSession
from .replay import ReplayClock, replay_can
from .uds import NegativeResponse, ReadClient, pdu_log_fields
from .vag import VagKnowledgeProvider

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_VEHICLE = ROOT / "data/vehicles/tiguan-5na-czpa-dq500.json"
FORBIDDEN_CONFIG_KEYS = frozenset({
    "actuator_test", "adaptation", "basic_settings",
    "clear_diagnostic_information", "clear_dtc", "coding",
    "communication_control", "ecu_reset", "flashing",
    "input_output_control", "routine_control", "security_access",
    "write_data_by_identifier", "write_nvm",
})


def _read_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))


def _walk_keys(value):
    if isinstance(value, dict):
        for key, child in value.items():
            yield str(key).lower()
            yield from _walk_keys(child)
    elif isinstance(value, list):
        for child in value:
            yield from _walk_keys(child)


def validate_runtime_config(path, *, check_files=True):
    """Validate the live parser contract without opening a DLL, VCI or vehicle."""
    path = Path(path).resolve()
    errors = []
    try:
        config = _read_json(path)
    except (OSError, ValueError, TypeError, json.JSONDecodeError) as exc:
        return {"path": str(path), "valid": False, "read_only": False,
                "ecu_roles": [], "endpoints": [], "errors": [str(exc)]}
    if not isinstance(config, dict):
        errors.append("Configuration root must be a JSON object")
        config = {}
    forbidden = sorted(set(_walk_keys(config)) & FORBIDDEN_CONFIG_KEYS)
    if forbidden:
        errors.append("Forbidden operation keys: " + ", ".join(forbidden))
    ecus = config.get("ecus")
    if not isinstance(ecus, dict) or not ecus:
        errors.append("At least one explicit physical ECU endpoint is required")
        ecus = {}
    roles = []
    endpoints = []
    for role in ecus:
        roles.append(role)
        try:
            connection = _connection(config, role)
            if connection["kind"] == "replay":
                continue
            txid = int(str(connection["txid"]), 16)
            rxid = int(str(connection["rxid"]), 16)
            from .transports import validate_can
            validate_can(txid, rxid, connection["bitrate"])
            endpoints.append({
                "role": role,
                "transport": connection["kind"],
                "protocol": "ISO15765" if connection["kind"] == "j2534"
                            else "ISO-TP",
                "txid": f"{txid:X}",
                "rxid": f"{rxid:X}",
                "bitrate": connection["bitrate"],
                "can_id_format": connection.get("can_id_format", "11bit"),
                "isotp_addressing": connection.get("isotp_addressing", "normal"),
                "address_evidence": connection.get("address_evidence"),
            })
            if connection["kind"] == "j2534":
                dll = connection.get("dll")
                if not isinstance(dll, str) or not dll:
                    raise ValueError("J2534 transport needs an absolute FunctionLibrary DLL path")
                dll_path = Path(dll)
                if not dll_path.is_absolute():
                    raise ValueError("J2534 FunctionLibrary DLL path must be absolute")
                if check_files:
                    if not dll_path.is_file():
                        raise ValueError("J2534 FunctionLibrary DLL must be an existing absolute path")
            _identity_probes(ecus[role])
        except (KeyError, TypeError, ValueError, OSError) as exc:
            errors.append(f"ECU {role}: {exc}")
    return {"path": str(path), "valid": not errors, "read_only": not forbidden,
            "ecu_roles": roles, "endpoints": endpoints, "errors": errors}


def _dependency_inventory():
    packages = {
        "python-can": "can", "can-isotp": "isotp", "udsoncan": "udsoncan",
        "cantools": "cantools",
    }
    result = {}
    for package, module in packages.items():
        try:
            version = metadata.version(package)
        except metadata.PackageNotFoundError:
            version = None
        result[package] = {"module": module, "installed": util.find_spec(module) is not None,
                           "version": version}
    return result


def _artifact_directory(path):
    if not path:
        return None
    directory = Path(path).resolve()
    try:
        directory.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(prefix=".minidash-write-test-", dir=directory):
            pass
        return {"path": str(directory), "writable": True}
    except OSError as exc:
        return {"path": str(directory), "writable": False, "error": str(exc)}


def doctor_report(*, dlls=(), config_path=None, artifact_dir=None):
    selected = list(dlls)
    config = validate_runtime_config(config_path) if config_path else None
    if config_path:
        try:
            raw = _read_json(config_path)
            dll = raw.get("transport", {}).get("dll")
            if isinstance(dll, str) and dll and dll not in selected:
                selected.append(dll)
        except (AttributeError, OSError, ValueError, TypeError, json.JSONDecodeError):
            pass
    result = inventory(selected)
    result["dependencies"] = _dependency_inventory()
    result["dependencies_ready"] = all(item["installed"]
                                       for item in result["dependencies"].values())
    result["config"] = config
    result["artifact_directory"] = _artifact_directory(artifact_dir)
    result["dll_load_performed"] = False
    result["backend_initialization_performed"] = False
    checks = [item["j2534_required_exports_present"] and item["python_bitness_match"]
              for item in result["dlls"] if item["explicit_candidate"]]
    result["preflight_ready"] = bool(checks and all(checks) and result["dependencies_ready"])
    if config is not None:
        result["preflight_ready"] = result["preflight_ready"] and config["valid"]
    if result["artifact_directory"] is not None:
        result["preflight_ready"] = (result["preflight_ready"] and
                                     result["artifact_directory"]["writable"])
    return result


def _did(text):
    text = text.strip().removeprefix("$").removeprefix("0x").removeprefix("0X")
    if len(text) != 4:
        raise ValueError("DID must be exactly four hex characters")
    return f"{int(text, 16):04X}"


def _redact_sensitive_reading(did, reading):
    if did != "F190":
        return reading
    result = dict(reading)
    result.update(raw=None, value=None, quality="redacted",
                  sensitive="vehicle.vin")
    return result


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
    if merged.get("can_id_format", "11bit") != "11bit":
        raise ValueError("Only 11bit CAN identifiers are supported")
    if merged.get("isotp_addressing", "normal") != "normal":
        raise ValueError("Only normal ISO-TP addressing is supported")
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
    return CanTransport(connection["channel"], txid, rxid, connection["bitrate"],
                        interface=connection.get("interface", "socketcan"))


def _identity_probes(ecu_config):
    if "identity_probes" not in ecu_config:
        return DEFAULT_IDENTITY_PROBES
    return tuple(IdentityProbe.from_mapping(item) for item in ecu_config["identity_probes"])


def _transport_factory(connection, config_dir):
    def factory():
        return _open_transport(connection, config_dir)
    return factory


def _identity_verifier(discovered, probes):
    def verify(client):
        return verify_discovered_identity(client, discovered, probes)
    return verify


def _runtime_endpoints(config_path, config, trace=lambda event: None):
    endpoints = []
    for role, ecu_config in config.get("ecus", {}).items():
        connection = _connection(config, role)
        identity_data = dict(ecu_config.get("identity", {}))
        identity_data.setdefault("logical_role", role)
        identity_data.setdefault("request_id", connection.get("txid"))
        identity_data.setdefault("response_id", connection.get("rxid"))
        identity = EcuIdentity(**identity_data)
        factory = _transport_factory(connection, config_path.parent)
        endpoints.append({"role": role, "factory": factory, "identity": identity,
                          "probes": _identity_probes(ecu_config),
                          "client_options": config.get("uds_timing", {}), "trace": trace})
    return endpoints


def _discover_runtime(config_path, config, trace=lambda event: None):
    vehicle = VehicleIdentity(**config.get("vehicle_identity", {}))
    return discover_vehicle(vehicle, _runtime_endpoints(config_path, config, trace))


def _load_discovery(path):
    return DiscoverySnapshot.from_mapping(_read_json(path))


def _write_new(path, content):
    with open(path, "x", encoding="utf-8") as output:
        output.write(content)


def _registry():
    return registry_from_providers([VagKnowledgeProvider()])


def run_discover(args):
    config_path = Path(args.config).resolve()
    config = _read_json(config_path)
    events = []
    snapshot = _discover_runtime(config_path, config,
        trace=lambda event: events.append({"timestamp": datetime.now(timezone.utc).isoformat(), **event}))
    result = snapshot.as_dict(redact_sensitive=True)
    result["metrics"] = {
        "endpoints_configured": len(config.get("ecus", {})),
        "ecus_discovered": len(snapshot.ecus),
        "endpoints_unavailable": len(snapshot.unavailable),
        "pdu_tx": sum(event.get("kind") == "pdu" and event.get("direction") == "tx"
                      for event in events),
        "pdu_rx": sum(event.get("kind") == "pdu" and event.get("direction") == "rx"
                      for event in events),
    }
    if args.debug:
        result["debug"] = events
    content = json.dumps(result, ensure_ascii=False, indent=2)
    if args.output:
        _write_new(args.output, content + "\n")
    print(content)
    return 0 if snapshot.ecus else 1


def run_signals(args):
    config_path = Path(args.config).resolve()
    config = _read_json(config_path)
    snapshot = _load_discovery(args.identity_snapshot) if args.identity_snapshot else \
        _discover_runtime(config_path, config)
    registry = _registry()
    ecus = {item.role: item.identity for item in snapshot.ecus}
    rows = signal_catalog(registry, snapshot.vehicle, ecus)
    if args.ecu:
        rows = [row for row in rows if row["ecu"] == args.ecu]
    if args.source:
        rows = [row for row in rows if row["source"] == args.source]
    safe_vehicle = snapshot.as_dict(redact_sensitive=True)["vehicle"]
    print(json.dumps({"vehicle": safe_vehicle,
                      "ecus": [{"role": item.role, "identity": asdict(item.identity),
                                "fingerprint": item.fingerprint} for item in snapshot.ecus],
                      "signals": rows, "unavailable_ecus": list(snapshot.unavailable)},
                     ensure_ascii=False, indent=2))
    return 0


def _open_passive_bus(config):
    passive = config.get("passive_can")
    if not passive:
        return None
    import can
    return can.Bus(interface=passive.get("interface", passive.get("kind", "socketcan")),
                   channel=passive["channel"], bitrate=passive.get("bitrate", 500000),
                   ignore_config=True)


def run_watch(args):
    config_path = Path(args.config).resolve()
    config = _read_json(config_path)
    with ExitStack() as resources:
        trace_file = resources.enter_context(
            open(args.trace, "x", encoding="utf-8", buffering=1)) if args.trace else None
        def emit(event):
            record = {"timestamp": datetime.now(timezone.utc).isoformat(),
                      "monotonic": time.monotonic(), **event}
            if trace_file:
                trace_file.write(json.dumps(record, ensure_ascii=False, allow_nan=False) + "\n")
        snapshot = _load_discovery(args.identity_snapshot) if args.identity_snapshot else \
            _discover_runtime(config_path, config, emit)
        registry = _registry()
        ecus = {item.role: item.identity for item in snapshot.ecus}
        availability = SourceAvailability(registry.definitions)
        planner = AcquisitionPlanner(registry, snapshot.vehicle, ecus, availability,
                                     poll_periods=config.get("poll_periods", {}),
                                     passive_grace=config.get("passive_can_grace", 0.5)
                                     if config.get("passive_can") else 0)
        plan = planner.build(args.signal)
        endpoint_configs = {
            item["role"]: item for item in _runtime_endpoints(config_path, config, emit)}
        discovered = {item.role: item for item in snapshot.ecus}
        sessions = {}
        for role in {job.ecu for job in plan.jobs}:
            endpoint = endpoint_configs[role]
            probes = endpoint["probes"]
            verify = _identity_verifier(discovered[role], probes)
            session = EcuSession(endpoint["factory"], verify=verify, trace=emit,
                                 client_options=config.get("uds_timing", {}))
            sessions[role] = session
            resources.callback(session.close)
        databases = {name: load_cantools_database(
                        (config_path.parent / path).resolve()
                        if not Path(path).is_absolute() else path)
                     for name, path in config.get("dbc_databases", {}).items()}
        engine = RuntimeEngine(plan, planner, sessions, databases,
                               cooldown=config.get("poll_cooldown", .05), trace=emit)
        web_state = None
        if getattr(args, 'web_port', None) is not None:
            web_state = State()
            web_state.mode = 'runtime'
            server, web_thread = serve(web_state, args.web_port,
                                      host=args.web_host, data_dir=args.web_data_dir)
            resources.callback(server.server_close)
            resources.callback(server.shutdown)
        bus = _open_passive_bus(config)
        if bus is not None:
            resources.callback(bus.shutdown)
        else:
            for definition in plan.passive:
                availability.set(definition.id, "unavailable", "passive_bus_not_configured")
        display_bridge = open_serial_display_bridge(args.display_port) \
            if args.display_port else None
        if display_bridge is not None:
            resources.callback(display_bridge.close)
        instrument_profile = getattr(args, 'display_instrument_profile', 'auto')
        if instrument_profile == 'auto':
            instrument_profile = select_instrument_profile(snapshot,
                market=getattr(args, 'display_market', 'eu'))
        next_print, next_display = 0, 0
        display_error_reported = False
        next_web = 0
        emit({"kind": "discovery", **snapshot.as_dict(redact_sensitive=True)})
        emit({"kind": "plan", "requested": list(plan.requested),
              "definitions": [item.id for item in plan.definitions],
              "catalog": [row for row in signal_catalog(registry, snapshot.vehicle, ecus,
                                                         availability)
                          if row["canonical_name"] in plan.requested],
              "jobs": [{"definition_id": job.definition_id, "did": f"{job.did:04X}",
                        "period": job.period} for job in plan.jobs],
              "skipped": list(plan.skipped)})
        # Setup/catalog tracing is not part of the requested acquisition duration.
        deadline = time.monotonic() + args.duration
        while time.monotonic() < deadline:
            if bus:
                message = bus.recv(.01)
                if message is not None:
                    engine.feed_message(message)
            engine.step()
            now = time.monotonic()
            if display_bridge is not None and now >= next_display:
                display_bridge.publish(engine.snapshot(), args.signal,
                    instrument_profile=instrument_profile,
                    profile_ttl_ms=min(60000,max(3000,int(args.display_interval*3000))))
                if display_bridge.error is not None and not display_error_reported:
                    print(f"Display serial bridge stopped: {display_bridge.error}", file=sys.stderr)
                    display_error_reported = True
                next_display = now + args.display_interval
            if web_state is not None and now >= next_web:
                web_state.update_runtime(engine.snapshot(), ecus=[
                    {'role':role, 'state':session.state, 'identity_verified':session.identity_verified}
                    for role, session in sessions.items()],
                    can_state='listening' if bus else 'unavailable',
                    vehicle_state='verified' if any(s.identity_verified for s in sessions.values()) else 'unavailable')
                next_web = now + .5
            if now >= next_print:
                current = engine.snapshot()
                if args.json:
                    print(json.dumps(current, ensure_ascii=False, allow_nan=False), flush=True)
                else:
                    for signal, decision in current["decisions"].items():
                        row = decision["selected"]
                        if row:
                            print(f"{signal}: {row['value']} {row.get('unit') or ''} "
                                  f"[{row['source']} {row['quality']} age={row['age']:.3f}s]",
                                  flush=True)
                        else:
                            print(f"{signal}: unavailable", flush=True)
                next_print = now + args.interval
            time.sleep(.005)
        emit({"kind": "summary", **engine.snapshot()})
        return 0


def run_can_replay(args):
    """Offline passive decoding. Connection fields are never opened or queried."""
    from .registry import SignalRegistry
    config_path = Path(args.config).resolve()
    config = _read_json(config_path)
    snapshot = _load_discovery(args.identity_snapshot)  # Required; no discovery fallback.
    registry = SignalRegistry(item for item in _registry().definitions
                              if item.source.kind == 'can')
    clock = ReplayClock()
    availability = SourceAvailability(registry.definitions, clock=clock)
    ecus = {item.role: item.identity for item in snapshot.ecus}
    planner = AcquisitionPlanner(registry, snapshot.vehicle, ecus, availability)
    plan = planner.build(args.signal)
    databases = {name: load_cantools_database(
                    (config_path.parent / path).resolve()
                    if not Path(path).is_absolute() else path)
                 for name, path in config.get('dbc_databases', {}).items()}
    engine = RuntimeEngine(plan, planner, {}, databases, clock=clock)
    with ExitStack() as resources:
        output = resources.enter_context(open(args.output, 'x', encoding='utf-8')) \
            if args.output else None
        def emit(record):
            if output:
                output.write(json.dumps(record, ensure_ascii=False, allow_nan=False) + '\n')
        emit({'kind': 'replay_metadata', 'schema': 1, 'offline': True,
              'vehicle_validation': False, 'requested': list(plan.requested)})
        try:
            result = replay_can(args.trace, engine, clock, max_frames=args.max_frames,
                                final_age=args.final_age, on_snapshot=emit)
        except (ValueError, OSError) as exc:
            emit({'kind': 'replay_error', 'offline': True, 'vehicle_validation': False,
                  'message': str(exc)})
            raise
        emit(result)
        print(json.dumps(result, ensure_ascii=False, allow_nan=False))
    return 0


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
                    reading = _redact_sensitive_reading(
                        did, decode(item, exchange.payload,
                                    identity_verified=identity_verified))
                    response_log = pdu_log_fields(
                        exchange.response, request=exchange.request)
                    record = emit({"kind": "uds_exchange", "service": "0x22", "did": did,
                                   "request": exchange.request.hex(),
                                   "response": response_log["pdu"],
                                   **{key: value for key, value in response_log.items()
                                      if key != "pdu"},
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
    doctor = commands.add_parser("doctor", help="Offline PC/config preflight; never opens a VCI")
    doctor.add_argument("--dll", action="append", default=[])
    doctor.add_argument("--config", help="Validate a runtime config without opening its transport")
    doctor.add_argument("--artifact-dir", help="Create/check a writable artifact directory")
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
    discover = commands.add_parser("discover", help="Read configured ECU identity endpoints")
    discover.add_argument("--config", required=True)
    discover.add_argument("--output", help="New identity snapshot JSON; never overwritten")
    discover.add_argument("--debug", action="store_true")
    signals = commands.add_parser("signals", help="List knowledge applicable to discovered identity")
    signals.add_argument("--config", required=True)
    signals.add_argument("--identity-snapshot")
    signals.add_argument("--all-implementations", action="store_true")
    signals.add_argument("--ecu")
    signals.add_argument("--source", choices=("can", "uds"))
    watch = commands.add_parser("watch", help="Run canonical passive CAN/UDS acquisition")
    watch.add_argument("signal", nargs="+", help="Canonical signal name(s), never DID/CAN ID")
    watch.add_argument("--config", required=True)
    watch.add_argument("--identity-snapshot")
    watch.add_argument("--duration", type=float, default=10.0)
    watch.add_argument("--interval", type=float, default=1.0)
    watch.add_argument("--trace", help="New structured JSONL trace; never overwritten")
    watch.add_argument("--json", action="store_true")
    watch.add_argument("--display-port", help="Optional USB Serial/JTAG port for the embedded display")
    watch.add_argument('--display-market', choices=['eu','other'], default='eu',
                       help='Instrument catalogue market; other disables automatic matching')
    watch.add_argument('--display-instrument-profile', default='auto',
                       choices=['auto','default',*INSTRUMENT_PROFILES],
                       help='Presentation scale override; auto uses discovered engine plus model generation')
    watch.add_argument("--display-interval", type=float, default=.1,
                       help="Embedded display update interval in seconds (default: 0.1)")
    watch.add_argument("--web-port", type=int, help="Serve the existing Web Configurator during watch")
    watch.add_argument("--web-host", default="127.0.0.1")
    watch.add_argument("--web-data-dir", default=".minidash")
    replay = commands.add_parser('replay-can',
        help='Decode a recorded CAN log offline; never opens buses/ECUs or sends PDUs')
    replay.add_argument('signal', nargs='+', help='Canonical signal name(s)')
    replay.add_argument('--config', required=True, help='Config containing external DBC paths')
    replay.add_argument('--identity-snapshot', required=True,
                        help='Existing identity context, not new vehicle validation')
    replay.add_argument('--trace', required=True, help='can_capture schema-1 JSONL input')
    replay.add_argument('--output', help='New offline decoded JSONL file; never overwritten')
    replay.add_argument('--max-frames', type=int, default=100000)
    replay.add_argument('--final-age', type=float, default=0.0,
                        help='Advance virtual time after last frame to test TTL expiry')
    args = parser.parse_args(argv)
    try:
        if args.command == "doctor":
            report = doctor_report(dlls=args.dll, config_path=args.config,
                                   artifact_dir=args.artifact_dir)
            content = json.dumps(report, ensure_ascii=False, indent=2)
            if args.output:
                _write_new(args.output, content + "\n")
            print(content)
            return 0 if (not args.config and not args.dll and not args.artifact_dir) \
                or report["preflight_ready"] else 1
        if args.command == "profile":
            print(json.dumps(load_profile(args.path), ensure_ascii=False, indent=2))
            return 0
        if args.command == "match":
            observed = _read_json(args.fingerprint)
            paths = sorted(Path(args.profile_dir).glob("*.json"))
            print(json.dumps({"matches": [p["id"] for p in match_catalog(paths, observed)]}, ensure_ascii=False))
            return 0
        if args.command == "discover":
            return run_discover(args)
        if args.command == "signals":
            return run_signals(args)
        if args.command == "watch":
            if args.duration <= 0 or args.interval <= 0 or args.display_interval <= 0:
                raise ValueError("duration and output intervals must be positive")
            if args.display_port:
                unsupported = sorted(set(args.signal) - UI_SIGNAL_NAMES)
                if unsupported:
                    raise ValueError("Signals are not supported by embedded UI: " +
                                     ", ".join(unsupported))
            return run_watch(args)
        if args.command == 'replay-can':
            return run_can_replay(args)
        return run_probe(args)
    except (ValueError, KeyError, TypeError, OSError, RuntimeError, json.JSONDecodeError) as exc:
        parser.error(str(exc))


if __name__ == "__main__":
    raise SystemExit(main())
