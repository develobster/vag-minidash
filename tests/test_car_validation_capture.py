import json
from pathlib import Path
import tempfile
import unittest

from tools.capture_car_validation import EvidenceTracker, collect


def snapshot(latest=3, sequences=(1, 2, 3), uptime=1000):
    return dict(schema=1, validation_mode=True, uptime_ms=uptime,
                trace_sequence=latest, exchange_sequence=0, exchanges=[],
                frames=[dict(sequence=n, direction='rx', raw='1234') for n in sequences])


class CaptureTests(unittest.TestCase):
    def test_initial_and_incremental_gaps(self):
        tracker = EvidenceTracker()
        self.assertTrue(tracker.inspect(snapshot())['complete_since_previous'])
        self.assertEqual(tracker.inspect(snapshot(9, (7, 8, 9)))['missing_since_previous']['frames'], 3)
        self.assertTrue(tracker.inspect(snapshot(9, (7, 8, 9)))['complete_since_previous'])

    def test_reboot_and_counter_rollover(self):
        tracker = EvidenceTracker()
        tracker.inspect(snapshot(0xffffffff, (0xfffffffe, 0xffffffff)))
        self.assertTrue(tracker.inspect(snapshot(1, (0xffffffff, 0, 1), 1100))['complete_since_previous'])
        result = tracker.inspect(snapshot(1, (1,), 10))
        self.assertEqual(result['epoch'], 1)
        self.assertTrue(result['complete_since_previous'])

    def test_fail_closed_schema_and_order(self):
        for change in [dict(schema=2), dict(validation_mode=False), dict(uptime_ms=-1),
                       dict(frames=[dict(sequence=3), dict(sequence=2)]),
                       dict(frames=[dict(sequence=2)], trace_sequence=3)]:
            row = snapshot(); row.update(change)
            with self.assertRaises(ValueError): EvidenceTracker().inspect(row)

    def test_collector_preserves_raw_and_existing_output(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'capture.jsonl'
            now = [0.0]
            def sleep(n): now[0] += n
            collect('http://192.168.4.1', output, 1, .5, fetch=snapshot,
                    clock=lambda: now[0], sleep=sleep)
            rows = [json.loads(line) for line in output.read_text(encoding='utf-8').splitlines()]
            self.assertEqual(len(rows), 2)
            self.assertEqual(rows[0]['device']['frames'][0]['raw'], '1234')
            before = output.read_bytes()
            with self.assertRaises(FileExistsError):
                collect('http://192.168.4.1', output, 1, .5, fetch=snapshot)
            self.assertEqual(before, output.read_bytes())

    def test_rejects_invalid_destination_and_budget_before_io(self):
        for url in ('file:///tmp', 'http://host/api/write', 'http://user:pass@host', 'http://host?x=y'):
            with self.assertRaises(ValueError): collect(url, 'unused', 1, .5)
        with self.assertRaises(ValueError): collect('http://host', 'unused', 1, .01)
