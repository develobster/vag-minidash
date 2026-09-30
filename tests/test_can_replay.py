"""Synthetic capture records test replay contracts, never vehicle evidence."""
import importlib.util
import io
import json
import tempfile
import unittest
from contextlib import redirect_stdout, redirect_stderr
from pathlib import Path
from unittest.mock import patch

from can_capture import CanTraceReader
from minidash_live.__main__ import main
from minidash_live.dbc import load_cantools_database
from minidash_live.identity import VehicleIdentity
from minidash_live.pipeline import AcquisitionPlanner, RuntimeEngine, SourceAvailability
from minidash_live.registry import SignalRegistry
from minidash_live.replay import ReplayClock, replay_can
from minidash_live.vag import VagKnowledgeProvider

ROOT = Path(__file__).resolve().parents[1]
METADATA = {'type': 'metadata', 'schema': 1, 'source': 'synthetic_test',
            'timestamp_kind': 'userspace_receive', 'hardware_listen_only_verified': False}


def frame(**changes):
    row = dict(type='frame', id=0x123, extended=False, remote=False, error=False,
               length=1, data='ab', monotonic=100.0, wall_time=1000.0)
    return dict(row, **changes)


def write_trace(path, frames=(), *, complete=True):
    records = [METADATA, *frames]
    if complete:
        records.append({'type': 'end', 'frames': len(frames)})
    path.write_text(''.join(json.dumps(row) + '\n' for row in records), encoding='utf-8')


class TraceReaderTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name) / 'trace.jsonl'

    def test_complete_and_remote_error_extended_flags(self):
        rows = [frame(), frame(id=0x12345, extended=True),
                frame(remote=True, length=8, data=''), frame(id=0x1000, error=True)]
        write_trace(self.path, rows)
        reader = CanTraceReader(self.path)
        self.assertEqual(list(reader), rows)
        self.assertTrue(reader.complete)
        self.assertEqual(reader.frames, 4)

    def test_interrupted_trace_is_explicitly_incomplete(self):
        write_trace(self.path, [frame()], complete=False)
        reader = CanTraceReader(self.path)
        self.assertEqual(len(list(reader)), 1)
        self.assertFalse(reader.complete)

    def test_bad_frame_fields_rejected(self):
        for changes in ({'id': True}, {'id': 0x800}, {'id': -1}, {'extended': 1},
                        {'remote': 'no'}, {'error': None}, {'length': 9},
                        {'length': True}, {'data': 'ab cd'}, {'data': 'a'},
                        {'data': 'xx'}, {'remote': True}, {'monotonic': True},
                        {'monotonic': -1}, {'monotonic': 10**800},
                        {'wall_time': -1}, {'monotonic': float('nan')},
                        {'monotonic': float('inf')}):
            with self.subTest(changes=changes):
                write_trace(self.path, [frame(**changes)])
                with self.assertRaises(ValueError):
                    list(CanTraceReader(self.path))

    def test_bad_metadata_and_empty_input(self):
        for row in ({}, [], {'type': 'metadata', 'schema': True},
                    {'type': 'metadata', 'schema': 1, 'timestamp_kind': 'unknown'}):
            self.path.write_text(json.dumps(row) + '\n', encoding='utf-8')
            with self.assertRaises(ValueError):
                list(CanTraceReader(self.path))
        self.path.write_text('', encoding='utf-8')
        with self.assertRaises(ValueError):
            list(CanTraceReader(self.path))

    def test_duplicate_keys_bad_json_and_line_limit(self):
        for text in ('{"type":"metadata","type":"frame"}\n', '{truncated',
                     json.dumps(dict(METADATA, filler='x'*8192))+'\n'):
            self.path.write_text(text, encoding='utf-8')
            with self.assertRaises(ValueError):
                list(CanTraceReader(self.path))

    def test_order_end_count_and_trailing_records(self):
        write_trace(self.path, [frame(monotonic=2), frame(monotonic=1)])
        with self.assertRaises(ValueError):
            list(CanTraceReader(self.path))
        write_trace(self.path, [frame()])
        text = self.path.read_text(encoding='utf-8')
        for bad in (text.replace('"frames": 1', '"frames": 2'), text+json.dumps(frame())+'\n'):
            self.path.write_text(bad, encoding='utf-8')
            with self.assertRaises(ValueError):
                list(CanTraceReader(self.path))

    def test_frame_limits(self):
        write_trace(self.path, [frame(), frame()])
        with self.assertRaises(ValueError):
            list(CanTraceReader(self.path, max_frames=1))
        for count in (True, 0, -1, 1.2):
            with self.assertRaises(ValueError):
                CanTraceReader(self.path, max_frames=count)

    def test_clock_rejects_invalid_or_backward_time(self):
        clock = ReplayClock()
        clock.advance_to(1)
        for seconds in (0, True, float('inf'), float('nan')):
            with self.assertRaises(ValueError):
                clock.advance_to(seconds)


@unittest.skipUnless(importlib.util.find_spec('cantools'), 'optional cantools required')
class RuntimeReplayTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.path = self.root / 'trace.jsonl'
        self.clock = ReplayClock()
        registry = SignalRegistry(d for d in VagKnowledgeProvider().definitions()
                                  if d.source.kind == 'can')
        availability = SourceAvailability(registry.definitions, clock=self.clock)
        planner = AcquisitionPlanner(registry, VehicleIdentity(platform='MQB'), {}, availability)
        self.database = load_cantools_database(ROOT / 'tests/fixtures/mqb-candidate-subset.dbc')
        self.engine = RuntimeEngine(planner.build(['engine.rpm', 'vehicle.speed']),
            planner, {}, {'opendbc/vw_mqb': self.database}, clock=self.clock)
        rpm = self.database.get_message_by_name('Motor_12').encode({
            'MO_QBit_Drehzahl_01': 0, 'MO_Drehzahl_01': 3000})
        speed = self.database.get_message_by_name('ESP_21').encode({'ESP_v_Signal': 30})
        self.rows = [frame(id=168, length=8, data=rpm.hex(), monotonic=100),
                     frame(id=253, length=8, data=speed.hex(), monotonic=100.6)]
        write_trace(self.path, self.rows)

    def test_runtime_replay_ttl_candidate_and_no_scheduler_or_bus(self):
        samples = []
        with patch.object(self.engine, 'step', side_effect=AssertionError('No polling')), \
                patch('can.Bus', side_effect=AssertionError('No bus')), \
                patch('socket.socket', side_effect=AssertionError('No socket')):
            result = replay_can(self.path, self.engine, self.clock, on_snapshot=samples.append)
        self.assertTrue(result['offline'])
        self.assertFalse(result['vehicle_validation'])
        self.assertEqual(result['frames'], 2)
        self.assertEqual(result['metrics']['uds_requests'], 0)
        self.assertEqual(samples[0]['decisions']['engine.rpm']['selected']['value'], 3000)
        self.assertIsNone(result['decisions']['engine.rpm']['selected'])
        self.assertEqual(result['decisions']['vehicle.speed']['selected']['quality'], 'candidate')

    def test_final_age_and_wrong_clock(self):
        result = replay_can(self.path, self.engine, self.clock, final_age=1)
        self.assertIsNone(result['decisions']['vehicle.speed']['selected'])
        with self.assertRaises(ValueError):
            replay_can(self.path, self.engine, self.clock)
        for age in (-1, True, float('nan')):
            with self.assertRaises(ValueError):
                replay_can(self.path, self.engine, ReplayClock(), final_age=age)
        with self.assertRaises(ValueError):
            replay_can(self.path, self.engine, ReplayClock())

    def test_cli_is_offline_and_output_is_never_overwritten(self):
        config = self.root / 'config.json'
        config.write_text(json.dumps({'transport': {'kind': 'j2534', 'dll': 'DO_NOT_OPEN'},
            'dbc_databases': {'opendbc/vw_mqb': str(ROOT / 'tests/fixtures/mqb-candidate-subset.dbc')}}),
            encoding='utf-8')
        identity = self.root / 'identity.json'
        identity.write_text(json.dumps({'schema_version': 1, 'vehicle': {'platform': 'MQB'},
                                       'ecus': [], 'unavailable': []}), encoding='utf-8')
        output = self.root / 'decoded.jsonl'
        args = ['replay-can', '--config', str(config), '--identity-snapshot', str(identity),
                '--trace', str(self.path), '--output', str(output), 'engine.rpm', 'vehicle.speed']
        with patch('can.Bus', side_effect=AssertionError('No bus')), \
                patch('minidash_live.__main__._discover_runtime', side_effect=AssertionError('No discovery')), \
                patch('minidash_live.__main__._open_transport', side_effect=AssertionError('No VCI')):
            stdout = io.StringIO()
            with redirect_stdout(stdout):
                self.assertEqual(main(args), 0)
            self.assertTrue(json.loads(stdout.getvalue())['offline'])
            before = output.read_bytes()
            with redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                main(args)
            self.assertEqual(output.read_bytes(), before)

    def test_cli_records_failure_without_success_summary(self):
        config = self.root / 'config.json'
        config.write_text('{"dbc_databases":{}}', encoding='utf-8')
        identity = self.root / 'identity.json'
        identity.write_text('{"schema_version":1,"vehicle":{},"ecus":[]}', encoding='utf-8')
        self.path.write_text('{broken', encoding='utf-8')
        output = self.root / 'failed.jsonl'
        with redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            main(['replay-can', '--config', str(config), '--identity-snapshot', str(identity),
                  '--trace', str(self.path), '--output', str(output), 'engine.rpm'])
        records = [json.loads(line) for line in output.read_text(encoding='utf-8').splitlines()]
        self.assertEqual(records[-1]['kind'], 'replay_error')
        self.assertNotIn('replay_summary', [row['kind'] for row in records])
