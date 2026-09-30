"""Synthetic private-format traces only; no vehicle I/O or validation claims."""
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from minidash_live.discovery import IdentityProbe, discover_endpoint
from minidash_live.identity import EcuIdentity
from minidash_live.registry import SignalRegistry, SignalDefinition, SignalSource, Provenance
from minidash_live.replay import RecordedUdsTransport, ReplayClock
from minidash_live.uds import ReadClient, NegativeResponse, ProtocolError
from minidash_live.validation import (preflight, read_request, EvidenceWriter,
    RecordingTransport, StopGate, marker, sanitize_trace, strict_json, compare_buttons, cross_check)
from minidash_live.validation_session import run_session


class ValidationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name) / '.local'
        self.directory.mkdir()
        self.path = self.directory / 'capture.jsonl'

    def trace(self, pairs, **metadata):
        rows = [dict(kind='metadata', schema=1, format='recorded_uds_v1', monotonic=100, **metadata)]
        for request, responses in pairs:
            rows.append(dict(kind='pdu', ecu='engine', direction='tx', pdu=request))
            rows.extend(dict(kind='pdu', ecu='engine', direction='rx', pdu=value) for value in responses)
        rows.append(dict(kind='end'))
        for i, row in enumerate(rows):
            row.setdefault('monotonic', 100+i*.01)
        self.path.write_text(''.join(json.dumps(row)+'\n' for row in rows), encoding='utf-8')
        return rows

    def transport(self, pairs):
        self.trace(pairs)
        return RecordedUdsTransport(self.path)

    def test_missing_transport_blocks_without_vehicle_io(self):
        result = preflight(artifact_dir=self.directory)
        self.assertEqual(result['status'], 'BLOCKED')
        self.assertFalse(result['vehicle_io_performed'])
        self.assertTrue(any('Transport' in item for item in result['blockers']))

    def test_readonly_rejects_writes_sessions_and_multidid(self):
        for pdu in ('14ffffff', '1101', '2701', '2ef40500', '3101', '2f0001',
                    '1001', '1003', '3e00', '19', '22f405f433', ''):
            with self.subTest(pdu=pdu), self.assertRaises(ValueError):
                read_request(bytes.fromhex(pdu))
        self.assertEqual(read_request(bytes.fromhex('22f405')), bytes.fromhex('22f405'))

    def test_d_pdu_config_cannot_be_ready(self):
        config = dict(transport={'kind':'d_pdu'}, ecus={'engine':{
            'txid':'7E0','rxid':'7E8','bitrate':500000,'address_evidence':'fixture only'}})
        path = self.directory / 'config.json'
        path.write_text(json.dumps(config))
        result = preflight(path, self.directory, require_passive=False)
        self.assertEqual(result['status'], 'BLOCKED')
        self.assertTrue(any('D-PDU' in reason for reason in result['blockers']))

    def test_preflight_nonobject_config_reports_blockers(self):
        path = self.directory / 'invalid.json'
        path.write_text('[]')
        self.assertEqual(preflight(path, self.directory)['status'], 'BLOCKED')

    def test_buttons_comparison_is_candidate_only(self):
        from tests.test_can_replay import write_trace, frame
        trace = self.directory / 'can.jsonl'
        write_trace(trace, [frame(monotonic=100,data='00'), frame(monotonic=101,data='00'),
                           frame(monotonic=104,data='01'),frame(monotonic=107,data='00')])
        markers = self.directory / 'markers.jsonl'
        markers.write_text(json.dumps(dict(kind='marker',marker='BASELINE',monotonic=100))+'\n'+
            json.dumps(dict(kind='marker',marker='STEERING_BUTTON',monotonic=104,label='OK'))+'\n')
        result = compare_buttons(trace, markers)
        self.assertEqual(result['status'], 'candidate')
        self.assertFalse(result['production_mapping'])
        self.assertEqual(result['comparisons'][0]['differences'][0]['new_payloads'], ['01'])
        with self.assertRaises(ValueError):
            compare_buttons(trace, markers, seconds=10)

    def test_discovery_nrc_stops_next_probe(self):
        transport = self.transport([('22f187',['7f2231']),('22f189',['62f18930303031'])])
        with self.assertRaises(NegativeResponse):
            discover_endpoint('engine', lambda: transport,
                probes=(IdentityProbe(0xF187,'ecu.part_number'),
                        IdentityProbe(0xF189,'ecu.software_version')),stop_on_error=True)
        self.assertEqual(transport.index, 2)

    def test_timeout_and_wrong_echo_latch(self):
        for response in (None, '62f43350'):
            transport = self.transport([('22f405',[response])])
            writer = EvidenceWriter(self.directory / ('timeout.jsonl' if response is None else 'echo.jsonl'), {})
            gate = StopGate()
            try:
                client = ReadClient(RecordingTransport(transport,writer,gate))
                with self.assertRaises((TimeoutError,ProtocolError)):
                    client.read_did(0xF405)
                with self.assertRaises(RuntimeError):
                    gate.check()
            finally:
                writer.close()

    def test_transport_fault_replay(self):
        rows = self.trace([('22f405',[])])
        rows.insert(-1, dict(kind='fault',ecu='engine',error='OSError',monotonic=100.015))
        self.path.write_text(''.join(json.dumps(r)+'\n' for r in rows))
        transport = RecordedUdsTransport(self.path)
        with self.assertRaises(OSError):
            transport.send(bytes.fromhex('22f405'),1)

    def test_reference_review_never_promotes_to_validated(self):
        self.path.write_text(json.dumps(dict(kind='evidence',signal='engine.coolant_temperature',
            status='decoded', timestamp='synthetic', offline=True,
            reading=dict(value=40,unit='degC',quality='candidate')))+'\n')
        result = cross_check(self.path,'engine.coolant_temperature',41,'degC','synthetic reference',2)
        self.assertEqual(result['status'],'cross-checked')
        self.assertFalse(result['vehicle_validation'])
        self.assertEqual(cross_check(self.path,'engine.coolant_temperature',90,'degC','synthetic',2)['status'],
                         'rejected')
        with self.assertRaises(ValueError):
            cross_check(self.path,'engine.coolant_temperature',40,'F','synthetic',2)

    def test_pending_and_repeated_did_keep_order(self):
        clock = ReplayClock()
        self.trace([('22f405', ['7f2278', '62f40550']), ('22f405', ['62f40551'])])
        transport = RecordedUdsTransport(self.path, clock=clock)
        client = ReadClient(transport, interval=.001)
        self.assertEqual(client.read_did(0xF405), b'\x50')
        transport.close()
        self.assertEqual(client.read_did(0xF405), b'\x51')
        self.assertTrue(transport.exhausted)
        self.assertGreater(clock(), 0)

    def test_request_order_mismatch(self):
        transport = self.transport([('22f405', ['62f40550'])])
        with self.assertRaises(ValueError):
            transport.send(bytes.fromhex('22f433'), 1)

    def test_recorded_nrc(self):
        client = ReadClient(self.transport([('22f405', ['7f2231'])]))
        with self.assertRaises(NegativeResponse):
            client.read_did(0xF405)

    def test_recorded_timeout(self):
        client = ReadClient(self.transport([('22f405', [None])]))
        with self.assertRaises(TimeoutError):
            client.read_did(0xF405)

    def test_recorded_malformed_response_production_parser(self):
        client = ReadClient(self.transport([('22f405', ['62f43350'])]))
        with self.assertRaises(ProtocolError):
            client.read_did(0xF405)

    def test_malformed_trace_before_any_replay(self):
        for changes in ({'ecu':'gearbox'}, {'direction':'oops'}, {'pdu':'xx'},
                        {'payload_redacted':True}, {'monotonic':-1}):
            rows = self.trace([('22f405', ['62f40550'])])
            rows[1].update(changes)
            self.path.write_text(''.join(json.dumps(r)+'\n' for r in rows), encoding='utf-8')
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                RecordedUdsTransport(self.path)

    def test_unsafe_request_in_trace_rejected(self):
        self.trace([('14ffffff', ['54'])])
        with self.assertRaises(ValueError):
            RecordedUdsTransport(self.path)

    def test_incomplete_trace_and_duplicate_json_rejected(self):
        self.trace([('22f405', ['7f2278'])])
        with self.assertRaises(ValueError):
            RecordedUdsTransport(self.path)
        with self.assertRaises(ValueError):
            strict_json('{"a":1,"a":2}')
        with self.assertRaises(ValueError):
            strict_json('{"a":NaN}')

    def test_recording_nrc_latches_all_followup_io(self):
        transport = self.transport([('22f405', ['7f2231'])])
        output = self.directory / 'recorded.jsonl'
        writer, gate = EvidenceWriter(output, {}), StopGate()
        self.addCleanup(writer.close)
        wrapped = RecordingTransport(transport, writer, gate)
        client = ReadClient(wrapped, interval=.001)
        with self.assertRaises(NegativeResponse):
            client.read_did(0xF405)
        with self.assertRaises(RuntimeError):
            wrapped.send(bytes.fromhex('22f433'), 1)
        self.assertEqual(transport.index, 2)

    def test_discovery_stops_on_identity_mismatch(self):
        transport = self.transport([('22f187', ['62f187424144']), ('22f189', ['62f18930303031'])])
        with self.assertRaises(ValueError):
            discover_endpoint('engine', lambda: transport,
                identity=EcuIdentity(part_number='EXPECTED'),
                probes=(IdentityProbe(0xF187, 'ecu.part_number'),
                        IdentityProbe(0xF189, 'ecu.software_version')), stop_on_error=True)
        self.assertEqual(transport.index, 2)

    def test_marker_sidecar_and_exclusive_evidence(self):
        self.assertEqual(marker(self.directory, 'STEERING_BUTTON', 'OK')['marker'], 'STEERING_BUTTON')
        with self.assertRaises(ValueError):
            marker(self.directory, 'GUESS_CAN_ID')
        writer = EvidenceWriter(self.path, {})
        writer.close()
        with self.assertRaises(FileExistsError):
            EvidenceWriter(self.path, {})
        with patch('minidash_live.validation.Path.resolve', return_value=Path('C:/not-private/output')):
            with self.assertRaises(ValueError):
                EvidenceWriter(Path(self.temp.name)/'not-private.jsonl', {})

    def test_sanitization_removes_identity_metadata_and_times(self):
        vin = 'TESTVIN0000000001'.encode().hex()
        self.trace([('22f190', ['62f190'+vin]), ('22f405', ['62f40550'])],
                   config={'serial':'PRIVATE-SECRET'})
        output = self.directory / 'sanitized.jsonl'
        sanitize_trace(self.path, output)
        text = output.read_text()
        for private in (vin, 'PRIVATE-SECRET', 'monotonic', 'timestamp'):
            self.assertNotIn(private, text)
        self.assertIn('62f40550', text)
        with self.assertRaises(ValueError):
            RecordedUdsTransport(output)

    def test_production_pipeline_replay_candidate_not_validated(self):
        config = dict(transport={'kind':'replay', 'fixture':'unused.json'},
            ecus={'engine':{'identity':{'part_number':'TEST'},
                'identity_probes':[{'did':'F187', 'target':'ecu.part_number'}]}},
            uds_timing={'interval':.001})
        config_path = self.directory / 'config.json'
        config_path.write_text(json.dumps(config))
        self.trace([('22f187',['62f18754455354']), ('22f187',['62f18754455354']),
                    ('22f405',['62f40550'])], config=config, signals=['engine.coolant_temperature'])
        registry = SignalRegistry([SignalDefinition('fixture.coolant', 'engine.coolant_temperature',
            'engine', SignalSource('uds', did=0xF405),
            Provenance('synthetic_test', 'candidate', 'fixture://not-car'),
            applicability={'part_number':'TEST'}, unit='degC', ttl=5, expected_period=2,
            decoder={'length':1,'byteorder':'big','signed':False,'scale':1,'offset':-40,
                     'source':'fixture://not-car'})])
        output = self.directory / 'replayed.jsonl'
        with patch('minidash_live.__main__._registry', return_value=registry):
            result = run_session(config_path, output,
                signals=['engine.coolant_temperature'], replay=self.path)
        self.assertEqual(result['status'], 'CAPTURED', result)
        self.assertEqual(result['signals'][0]['value'], 40)
        self.assertEqual(result['signals'][0]['quality'], 'candidate')
        self.assertEqual(result['decisions']['engine.coolant_temperature']['selected']['value'],40)
        rows = [json.loads(line) for line in output.read_text().splitlines()]
        evidence = next(row for row in rows if row['kind']=='evidence')
        self.assertEqual(evidence['status'], 'decoded')
        self.assertFalse(evidence['vehicle_validation'])
        self.assertEqual(evidence['raw_exchange']['responses'], ['62f40550'])

    def test_four_targets_use_real_provider_in_requested_order(self):
        # Synthetic DID/layout choices here are fixture-only, not car probe recommendations.
        identities = [('part_number','5NA907115D'),('software_version','0003'),
            ('hardware_part_number','06L907309B'),('hardware_version','H31'),
            ('asam_id','EV_ECM20TFS0115NA907115D'),('asam_version','001004')]
        probes = [dict(did=f'{0xF180+i:04X}',target='ecu.'+name) for i,(name,_) in enumerate(identities)]
        config = dict(transport={'kind':'replay','fixture':'unused.json'},
            ecus={'engine':{'identity':dict(identities),'identity_probes':probes}},
            uds_timing={'interval':.001})
        path = self.directory/'config.json'
        path.write_text(json.dumps(config))
        pairs = [(f'22{0xF180+i:04x}',[f'62{0xF180+i:04x}'+value.encode().hex()])
                 for i,(_,value) in enumerate(identities)]
        signals = ['engine.coolant_temperature','engine.ambient_pressure','engine.rpm','vehicle.speed']
        self.trace(pairs+pairs+[('22f405',['62f40550']),('22f433',['62f43303e8']),
                               ('2216f3',['6216f30000']),('22f40d',['62f40d00'])],
                   config=config,signals=signals)
        output = self.directory/'four-replay.jsonl'
        result = run_session(path,output,signals=signals,replay=self.path)
        self.assertEqual(result['status'],'CAPTURED',result)
        self.assertEqual(len(result['signals']),4)
        evidence = [json.loads(line) for line in output.read_text().splitlines()
                    if json.loads(line)['kind']=='evidence']
        self.assertEqual([row['did'] for row in evidence],['F405','F433','16F3','F40D'])
        self.assertEqual([row['status'] for row in evidence],['decoded','observed','observed','observed'])
        self.assertFalse(result['vehicle_validation'])
