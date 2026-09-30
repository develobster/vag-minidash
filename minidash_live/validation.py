"""First-session safety and private evidence, using the existing diagnostic runtime.

Inventory is offline. It never loads a DLL, opens a bus or certifies a vehicle.
"""
import json
import math
import shutil
import time
from datetime import datetime, timezone
from pathlib import Path


def d_pdu_descriptions(doctor):
    """Read registered provider descriptions, not live MVCI module enumeration."""
    from urllib.parse import urlparse, unquote
    from xml.etree import ElementTree
    from struct import error as StructError
    from .discovery import pe_info
    results = []
    for entry in doctor.get('d_pdu_registry', []):
        root_path = entry.get('values', {}).get('Root File')
        if not isinstance(root_path, str):
            continue
        try:
            if Path(root_path).stat().st_size > 1024 * 1024:
                raise ValueError('D-PDU root description too large')
            root = ElementTree.parse(root_path).getroot()
            for provider in root.findall('MVCI_PDU_API'):
                item = dict(name=provider.findtext('SHORT_NAME'), connected=False,
                            configured_resources=[], modules=[])
                for tag in ('LIBRARY_FILE', 'MODULE_DESCRIPTION_FILE'):
                    element = provider.find(tag)
                    if element is None:
                        continue
                    uri = urlparse(element.attrib.get('URI', ''))
                    if uri.scheme != 'file' or uri.netloc:
                        raise ValueError('Only local file provider URIs supported')
                    local = unquote(uri.path).lstrip('/')
                    if tag == 'LIBRARY_FILE':
                        item['dll'] = pe_info(local)
                    else:
                        if Path(local).stat().st_size > 8 * 1024 * 1024:
                            raise ValueError('MDF description too large')
                        mdf = ElementTree.parse(local).getroot()
                        item['modules'] = [node.findtext('SHORT_NAME') for node in mdf.findall('MODULETYPE')]
                        item['configured_resources'] = sorted({node.attrib['IDREF']
                            for node in mdf.iter('RESOURCE') if 'IDREF' in node.attrib and
                            any(word in node.attrib['IDREF'] for word in ('RAWCAN', 'UDS_CAN'))})
                results.append(item)
        except (OSError, ValueError, StructError, ElementTree.ParseError) as exc:
            results.append(dict(root=root_path, error=str(exc), connected=False))
    return results

MARKERS = frozenset(('IGNITION_ON', 'ENGINE_START', 'IDLE', 'RPM_1500',
    'RPM_2000', 'RPM_2500', 'BRAKE', 'ACCELERATOR', 'STEERING_BUTTON',
    'VEHICLE_MOVING', 'VEHICLE_STOP', 'IGNITION_OFF', 'BASELINE', 'BUTTON_RELEASE'))
EVIDENCE_STATUSES = frozenset(('candidate', 'observed', 'decoded',
                              'cross-checked', 'validated', 'rejected'))


def strict_json(text):
    def pairs(items):
        result = {}
        for key, value in items:
            if key in result:
                raise ValueError('Duplicate JSON key')
            result[key] = value
        return result
    def constant(value):
        raise ValueError('Non-finite JSON value')
    return json.loads(text, object_pairs_hook=pairs, parse_constant=constant)


def read_request(pdu):
    """First milestone: one RDID per request; no session change or write escape."""
    pdu = bytes(pdu)
    if len(pdu) != 3 or pdu[0] != 0x22:
        raise ValueError('First session permits only single-DID service 0x22')
    return pdu


def private_directory(path):
    """Force all new vehicle artifacts under an ignored .local directory."""
    path = Path(path).resolve()
    if '.local' not in path.parts:
        raise ValueError('Private vehicle artifacts must be under .local')
    path.mkdir(parents=True, exist_ok=True)
    return path


def marker(directory, name, label=''):
    if name not in MARKERS:
        raise ValueError('Unknown stage marker')
    if not isinstance(label, str) or len(label) > 80 or any(ord(c) < 32 for c in label):
        raise ValueError('Marker label must be short printable text without identifiers')
    record = dict(kind='marker', schema=1, marker=name, label=label,
                  monotonic=time.monotonic(), timestamp=datetime.now(timezone.utc).isoformat())
    # Separate sidecar preserves the existing CAN trace schema. One writer per file.
    with (private_directory(directory) / 'markers.jsonl').open('a', encoding='utf-8') as output:
        output.write(json.dumps(record, allow_nan=False) + '\n')
    return record


def preflight(config_path=None, artifact_dir='.local/real-car', *, require_passive=True):
    from .__main__ import doctor_report, _registry, _connection
    from .replay import RecordedUdsTransport
    blockers = []
    config = {}
    try:
        directory = private_directory(artifact_dir)
        disk = shutil.disk_usage(directory).free
    except (OSError, ValueError) as exc:
        directory, disk = None, 0
        blockers.append(f'Private artifacts: {exc}')
    try:
        if config_path:
            config = strict_json(Path(config_path).read_text(encoding='utf-8'))
            if not isinstance(config, dict):
                config = {}
                raise ValueError('Configuration must be an object')
        else:
            blockers.append('No explicit transport/ECU config selected')
        doctor = doctor_report(config_path=config_path, artifact_dir=directory)
    except (OSError, ValueError, TypeError) as exc:
        doctor = {}
        blockers.append(f'Config/inventory: {exc}')
    if not doctor.get('dependencies_ready'):
        blockers.append('Required Python dependencies unavailable')
    if doctor.get('platform') != 'Windows':
        blockers.append('This VNCI preflight targets Windows; SocketCAN is a separate hardware workflow')
    if config_path and not doctor.get('config', {}).get('valid'):
        blockers.append('Runtime configuration invalid; inspect doctor config errors')
    if directory and not doctor.get('artifact_directory', {}).get('writable'):
        blockers.append('Artifact write permission check failed')
    if disk < 100 * 1024 * 1024:
        blockers.append('At least 100 MiB free space required')
    ecus = config.get('ecus', {})
    if not isinstance(ecus, dict):
        ecus = {}
    if set(ecus) != {'engine'}:
        blockers.append('First session requires exactly one explicit engine endpoint')
    try:
        connection = _connection(config, 'engine')
        if connection['kind'] != 'j2534':
            blockers.append('Windows VNCI milestone requires an evidenced J2534 adapter; D-PDU is not implemented')
        elif not doctor.get('preflight_ready'):
            blockers.append('Selected J2534 DLL missing, incompatible or missing PassThru exports')
    except (ValueError, TypeError, KeyError) as exc:
        blockers.append(f'Transport: {exc}')
    if require_passive:
        blockers.append('VNCI Windows passive CAN capture adapter/listen-only verification not implemented')
        engine_config = ecus.get('engine', {})
        probes = engine_config.get('identity_probes', []) if isinstance(engine_config, dict) else []
        targets = {item.get('target') for item in probes if isinstance(item, dict)} \
                  if isinstance(probes, list) else set()
        required = {'ecu.part_number', 'ecu.software_version', 'ecu.hardware_part_number',
                    'ecu.hardware_version', 'ecu.asam_id', 'ecu.asam_version'}
        if not required <= targets:
            blockers.append('Telemetry identity probe/layout evidence incomplete; VIN-only discovery is insufficient')
    try:
        count = len(_registry().definitions)
        if not count:
            raise ValueError('empty registry')
    except (OSError, ValueError) as exc:
        count = 0
        blockers.append(f'Knowledge providers: {exc}')
    return dict(status='BLOCKED' if blockers else 'OFFLINE PREFLIGHT PASS',
        blockers=blockers, doctor=doctor, knowledge_definitions=count,
        d_pdu_descriptions=d_pdu_descriptions(doctor),
        free_bytes=disk, safety='single-DID 0x22 only; no retries after fault',
        recorded_uds_replay=RecordedUdsTransport.__name__, vehicle_io_performed=False,
        connected_interface_verified=False, car_validation=False,
        note='Offline PASS only authorizes a connection attempt, not READY or car validation')


class EvidenceWriter:
    """Exclusive private JSONL trace. Raw identities stay private for faithful replay."""
    def __init__(self, path, metadata):
        path = Path(path)
        private_directory(path.parent)
        self.output = path.open('x', encoding='utf-8', buffering=1)
        self.last_exchange = {}
        self.write(dict(kind='metadata', schema=1, format='recorded_uds_v1',
                        private=True, **metadata))

    def write(self, record):
        if record.get('kind') == 'pdu':
            if record.get('direction') == 'tx':
                self.last_exchange = dict(request=record['pdu'], responses=[])
            else:
                self.last_exchange.setdefault('responses', []).append(record['pdu'])
        row = dict(monotonic=time.monotonic(),
                   timestamp=datetime.now(timezone.utc).isoformat(), **record)
        self.output.write(json.dumps(row, ensure_ascii=False, allow_nan=False) + '\n')

    def close(self):
        self.output.close()


class StopGate:
    def __init__(self):
        self.reason = None

    def check(self):
        if self.reason:
            raise RuntimeError('Active diagnostics STOP: ' + self.reason)


class RecordingTransport:
    """Transparent existing Transport decorator with a session-wide fault latch."""
    def __init__(self, transport, writer, gate, ecu='engine'):
        self.transport, self.writer, self.gate, self.ecu = transport, writer, gate, ecu
        self.request = None

    def send(self, pdu, timeout):
        self.gate.check()
        try:
            self.request = read_request(pdu)
            self.writer.write(dict(kind='pdu', ecu=self.ecu, direction='tx', pdu=self.request.hex()))
            self.transport.send(self.request, timeout)
        except Exception as exc:
            self.gate.reason = type(exc).__name__
            self.writer.write(dict(kind='fault', ecu=self.ecu, error=type(exc).__name__))
            raise

    def recv(self, timeout):
        self.gate.check()
        try:
            response = self.transport.recv(timeout)
            self.writer.write(dict(kind='pdu', ecu=self.ecu, direction='rx',
                                   pdu=None if response is None else bytes(response).hex()))
            if response is None:
                self.gate.reason = 'timeout'
            elif bytes(response)[:1] == b'\x7f':
                if bytes(response) != b'\x7f\x22\x78':
                    self.gate.reason = 'negative response'
            elif self.request is None or bytes(response)[:3] != b'\x62' + self.request[1:] \
                    or len(response) < 4:
                self.gate.reason = 'malformed/unrelated response'
            return response
        except Exception as exc:
            self.gate.reason = type(exc).__name__
            self.writer.write(dict(kind='fault', ecu=self.ecu, error=type(exc).__name__))
            raise

    def close(self):
        self.transport.close()


def sanitize_trace(source, destination):
    """Export PDUs only; remove all identity payloads, metadata, exact times, results.

    Sanitized files deliberately cannot replay identity verification. They are
    review aids, not automatically publishable fixtures (inspect non-identity DIDs).
    """
    records = []
    sensitive = False
    for line in Path(source).read_text(encoding='utf-8').splitlines():
        row = strict_json(line)
        if row.get('kind') != 'pdu':
            continue
        pdu = row.get('pdu')
        if row.get('direction') == 'tx':
            request = read_request(bytes.fromhex(pdu))
            sensitive = 0xF180 <= int.from_bytes(request[1:], 'big') <= 0xF1FF
        item = dict(kind='pdu', direction=row['direction'], ecu=row['ecu'], pdu=pdu)
        if sensitive and row['direction'] == 'rx' and pdu and not pdu.startswith('7f'):
            item.update(pdu=pdu[:6], payload_redacted=True)
        records.append(item)
    with Path(destination).open('x', encoding='utf-8') as output:
        for row in records:
            output.write(json.dumps(row) + '\n')
    return len(records)


def compare_buttons(trace_path, marker_path, *, seconds=2.0):
    """Compare explicit same-host windows; raw byte differences remain candidates."""
    from collections import defaultdict
    from can_capture import CanTraceReader
    if isinstance(seconds, bool) or not isinstance(seconds, (int, float)) \
            or not math.isfinite(seconds) or not 0 < seconds <= 10:
        raise ValueError('Comparison window must be in (0, 10] seconds')
    if Path(marker_path).stat().st_size > 1024 * 1024:
        raise ValueError('Marker file too large')
    markers = [strict_json(line) for line in Path(marker_path).read_text(encoding='utf-8').splitlines()]
    previous, baseline, comparisons = -1, None, []
    for event in markers:
        stamp = event.get('monotonic')
        if event.get('kind') != 'marker' or event.get('marker') not in MARKERS or \
                isinstance(stamp, bool) or not isinstance(stamp, (int, float)) or \
                not math.isfinite(stamp) or stamp < previous:
            raise ValueError('Invalid/non-monotonic marker file')
        previous = stamp
        if event['marker'] == 'BASELINE':
            baseline = stamp
        elif event['marker'] == 'STEERING_BUTTON':
            if baseline is None or stamp < baseline + seconds:
                raise ValueError('Button press needs a preceding non-overlapping BASELINE window')
            comparisons.append(dict(baseline=baseline, press=stamp, label=event.get('label', '')))
            if len(comparisons) > 128:
                raise ValueError('At most 128 button comparison windows supported')
    if not comparisons:
        raise ValueError('No baseline/button pairs')
    frames = list(CanTraceReader(trace_path))
    if not frames:
        raise ValueError('Empty CAN capture')
    output = []
    for pair in comparisons:
        if frames[0]['monotonic'] > pair['baseline'] or frames[-1]['monotonic'] < pair['press'] + seconds:
            raise ValueError('Markers/windows outside capture; use the same host monotonic clock')
        windows = []
        for start in (pair['baseline'], pair['press']):
            rows = defaultdict(list)
            for frame in frames:
                if start <= frame['monotonic'] < start + seconds and not frame['remote'] and not frame['error']:
                    rows[(frame['id'], frame['extended'])].append(frame['data'])
            windows.append(rows)
        before, during = windows
        differences = []
        for key in sorted(set(before) | set(during)):
            new = sorted(set(during[key]) - set(before[key]))
            if new or len(before[key]) != len(during[key]):
                differences.append(dict(id=hex(key[0]), extended=key[1],
                    baseline_frames=len(before[key]), press_frames=len(during[key]), new_payloads=new))
        output.append(dict(**pair, differences=differences))
    return dict(status='candidate', vehicle_validation=False, production_mapping=False,
                seconds=seconds, comparisons=output,
                limitation='Traffic correlation alone cannot identify a button or bus origin')


def cross_check(trace_path, signal, reference, unit, source, tolerance):
    """Operator-supplied same-stage reference; never grants validated status."""
    import hashlib
    for value in (reference, tolerance):
        if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
            raise ValueError('Reference and tolerance must be finite numbers')
    if tolerance < 0 or not isinstance(source, str) or not source.strip() or len(source) > 160:
        raise ValueError('Non-negative tolerance and explicit reference provenance required')
    path = Path(trace_path)
    if path.stat().st_size > 32 * 1024 * 1024:
        raise ValueError('Evidence file too large')
    matches = []
    for line in path.read_text(encoding='utf-8').splitlines():
        row = strict_json(line)
        if row.get('kind') == 'evidence' and row.get('signal') == signal:
            matches.append(row)
    if len(matches) != 1:
        raise ValueError('Select a capture with exactly one sample of this signal')
    evidence = matches[0]
    reading = evidence.get('reading') or {}
    value = reading.get('value')
    if evidence.get('status') != 'decoded' or reading.get('quality') not in ('valid', 'candidate') \
            or isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
        raise ValueError('No decoded numeric sample to cross-check; raw-only is not a value')
    if unit != reading.get('unit'):
        raise ValueError('Reference unit must exactly match the decoder; no implicit conversion')
    difference = abs(value-reference)
    return dict(kind='reference_review', schema=1, signal=signal,
        status='cross-checked' if difference <= tolerance else 'rejected',
        capture_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        sample_timestamp=evidence['timestamp'], value=value, unit=unit,
        reference=reference, reference_source=source, tolerance=tolerance,
        absolute_difference=difference, operator_same_stage_reference_required=True,
        offline=evidence.get('offline'), vehicle_validation=False,
        note='Cross-check annotation only; independent review required for validated promotion')
