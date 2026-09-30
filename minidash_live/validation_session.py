"""Finite first-car acquisition through production discovery/planner/engine.

The capture is private. A replay runs this same workflow with recorded Transport.
There are no loops/reconnect retries after a fault, no guessed probes or decoders.
"""
import time
from dataclasses import asdict
from pathlib import Path

from .discovery import discover_endpoint, DiscoverySnapshot, verify_discovered_identity
from .identity import VehicleIdentity
from .pipeline import AcquisitionPlanner, SourceAvailability, RuntimeEngine
from .runtime import EcuSession
from .replay import RecordedUdsTransport, ReplayClock
from .validation import EvidenceWriter, RecordingTransport, StopGate, strict_json, preflight

TARGETS = ('engine.coolant_temperature', 'engine.ambient_pressure',
           'engine.rpm', 'vehicle.speed')


def run_session(config_path, output, *, signals=(), replay=None):
    from .__main__ import _runtime_endpoints, _registry, validate_runtime_config
    path = Path(config_path).resolve()
    config = strict_json(path.read_text(encoding='utf-8'))
    check = validate_runtime_config(path, check_files=replay is None)
    if not check['valid'] or set(config['ecus']) != {'engine'}:
        raise ValueError('Invalid first-session engine configuration: ' + str(check['errors']))
    signals = tuple(signals)
    if len(set(signals)) != len(signals) or any(name not in TARGETS for name in signals):
        raise ValueError('Only the four evidenced engine targets are permitted once each')
    if replay is None:
        checks = preflight(path, Path(output).parent, require_passive=False)
        if checks['blockers']:
            raise ValueError('Preflight BLOCKED: ' + '; '.join(checks['blockers']))
        clock, recorded = time.monotonic, None
    else:
        clock = ReplayClock()
        recorded = RecordedUdsTransport(replay, clock=clock)
        if recorded.metadata.get('config') != config or recorded.metadata.get('signals') != list(signals):
            raise ValueError('Replay requires the exact captured config and signal order')
    endpoint = _runtime_endpoints(path, config)[0]
    gate = StopGate()
    writer = EvidenceWriter(output, dict(config=config, signals=list(signals),
        transport=check['endpoints'], offline=replay is not None, vehicle_validation=False))
    sessions = {}
    def emit(row):
        # PDUs are captured at the Transport boundary, including identity payloads.
        if row.get('kind') != 'pdu':
            writer.write(row)
    def factory():
        gate.check()
        transport = recorded if recorded else endpoint['factory']()
        return RecordingTransport(transport, writer, gate)
    result = dict(status='STOP', offline=replay is not None, vehicle_validation=False)
    try:
        vehicle, ecu = discover_endpoint('engine', factory,
            vehicle=VehicleIdentity(**config.get('vehicle_identity', {})),
            identity=endpoint['identity'], probes=endpoint['probes'],
            client_options=config.get('uds_timing', {}), stop_on_error=True)
        discovery = DiscoverySnapshot(vehicle, (ecu,))
        writer.write(dict(kind='discovery', **discovery.as_dict()))
        if signals and not any(item['target'].startswith('ecu.') for item in ecu.fields):
            raise ValueError('ECU identity has not been observed; F190 alone cannot unlock telemetry')
        registry = _registry()
        availability = SourceAvailability(registry.definitions, clock=clock)
        planner = AcquisitionPlanner(registry, vehicle, {'engine': ecu.identity}, availability,
            passive_grace=0, poll_periods={name: 2.0 for name in signals})
        plan = planner.build(signals)
        # First session is UDS-only. Passive evidence must be recorded independently.
        if len(plan.jobs) != len(signals) or plan.skipped or any(j.ecu != 'engine' for j in plan.jobs):
            raise ValueError('Targets not uniquely applicable to observed ECU; no speculative reads')
        writer.write(dict(kind='plan', requested=list(signals),
            definitions=[item.id for item in plan.definitions], skipped=list(plan.skipped)))
        def verify(client):
            matched = verify_discovered_identity(client, ecu, endpoint['probes'])
            if not matched:
                gate.reason = 'ECU identity mismatch/unverified'
                raise ValueError(gate.reason)
            return True
        if plan.jobs:
            session = EcuSession(factory, verify=verify, clock=clock,
                client_options=config.get('uds_timing', {}))
            sessions['engine'] = session
        engine = RuntimeEngine(plan, planner, sessions, {}, clock=clock, trace=emit)
        callback = engine.scheduler.on_result
        def on_result(job, status, latency, error=None):
            callback(job, status, latency, error)
            # A completed first-session job never repeats, even after planner reconciliation.
            job.blocked = True
            if status != 'success':
                gate.reason = status
            row = next((row for row in engine.store.snapshot()
                        if row['definition_id'] == job.definition_id), None)
            definition = next(item for item in plan.definitions if item.id == job.definition_id)
            quality = row['quality'] if row else 'unavailable'
            writer.write(dict(kind='evidence', signal=job.signal, definition_id=job.definition_id,
                did=f'{job.did:04X}', service='22', ecu_identity=asdict(ecu.identity),
                vehicle_identity=asdict(vehicle), transport=check['endpoints'],
                status='decoded' if quality in ('valid', 'candidate') else
                       'observed' if quality == 'raw_only' else 'rejected',
                decoder=dict(definition.decoder) if definition.decoder else None,
                provenance=asdict(definition.provenance),
                raw_exchange=writer.last_exchange,
                reference=None, reading=row, offline=replay is not None,
                vehicle_validation=False, error=error))
        engine.scheduler.on_result = on_result
        for job in plan.jobs:
            job.blocked = True
        for job in plan.jobs:
            gate.check()
            # Existing scheduler and decoder own each acquisition; no replay-only decoding.
            job.blocked = False
            job.due = clock()
            engine.scheduler.next_tx.clear()
            if not engine.step():
                raise ValueError('Requested acquisition did not run')
            gate.check()
        if recorded and not recorded.exhausted:
            raise ValueError('Replay has unconsumed exchanges')
        result.update(status='CAPTURED' if signals else 'IDENTITY_OBSERVED',
                      **engine.snapshot())
    except Exception as exc:
        gate.reason = gate.reason or type(exc).__name__
        result['error'] = str(exc)
    finally:
        for session in sessions.values():
            session.close()
        writer.write(dict(kind='summary', **result))
        writer.write(dict(kind='end', status=result['status']))
        writer.close()
    return result
