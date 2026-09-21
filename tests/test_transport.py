import importlib.util
import threading
import unittest
from transport_lab import run_transport_lab

AVAILABLE = all(importlib.util.find_spec(module) for module in ('can','isotp','udsoncan'))


@unittest.skipUnless(AVAILABLE, 'Install requirements-transport.txt for integration tests')
class TransportTests(unittest.TestCase):
    def test_real_libraries_multiframe(self):
        result = run_transport_lab()
        self.assertIsNone(result['error'])
        self.assertEqual(result['vin'], 'WVWZZZ00000000000')
        self.assertEqual(result['large_payload_bytes'], 180)
        self.assertGreaterEqual(result['frame_counts']['first'], 2)
        self.assertGreaterEqual(result['frame_counts']['consecutive'], 27)
        self.assertGreaterEqual(result['frame_counts']['flow_control'], 4)
        self.assertFalse(any('transport_error' in e or 'server_error' in e for e in result['events']))
        self.assertEqual([e['request'] for e in result['events'] if 'request' in e], ['22f190','22f1a0'])
        self.assertFalse(any(t.name == 'minidash-virtual-ecu' for t in threading.enumerate()))

    def test_pending_no_retry(self):
        result = run_transport_lab('pending')
        self.assertIsNone(result['error'])
        self.assertEqual(len([e for e in result['events'] if 'request' in e]), 2)
        self.assertEqual(sum(f['data'].startswith('037f2278') for f in result['frames']), 2)

    def test_negative(self):
        result = run_transport_lab('negative')
        self.assertEqual(result['error']['type'], 'NegativeResponseException')
        self.assertIsNone(result['vin'])

    def test_timeout(self):
        result = run_transport_lab('silent')
        self.assertEqual(result['error']['type'], 'TimeoutException')
        self.assertEqual(len([e for e in result['events'] if 'request' in e]), 1)

    def test_truncated(self):
        result = run_transport_lab('truncated')
        self.assertEqual(result['error']['type'], 'InvalidResponseException')

    def test_no_hardware_parameter(self):
        with self.assertRaises(ValueError):
            run_transport_lab('socketcan')
