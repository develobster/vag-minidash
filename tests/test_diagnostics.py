import unittest
from diagnostics import (FakeECU, DiagnosticLab, DiagnosticTimeout, NegativeResponse,
                         ResponseError, decode_pid, decode_support, run_lab)


class DiagnosticTests(unittest.TestCase):
    def test_vin(self):
        lab = DiagnosticLab(FakeECU())
        self.assertEqual(lab.read_did(0xF190, 17), b'WVWZZZ00000000000')

    def test_negative(self):
        with self.assertRaises(NegativeResponse) as caught:
            DiagnosticLab(FakeECU('negative')).read_did(0xF190, 17)
        self.assertEqual(caught.exception.code, 0x22)

    def test_unknown_did(self):
        with self.assertRaises(NegativeResponse) as caught:
            DiagnosticLab(FakeECU()).read_did(0x1234, 1)
        self.assertEqual(caught.exception.code, 0x31)

    def test_silent(self):
        ecu = FakeECU('silent')
        with self.assertRaises(DiagnosticTimeout):
            DiagnosticLab(ecu).read_did(0xF190, 17)
        self.assertAlmostEqual(ecu.now, .5)
        self.assertEqual(len(ecu.requests), 1)

    def test_pending_no_resend(self):
        ecu = FakeECU('pending')
        self.assertEqual(len(DiagnosticLab(ecu).read_did(0xF190, 17)), 17)
        self.assertEqual(len(ecu.requests), 1)
        self.assertAlmostEqual(ecu.now, .2)

    def test_total_deadline(self):
        ecu = FakeECU('endless_pending')
        with self.assertRaises(DiagnosticTimeout):
            DiagnosticLab(ecu, total=.45).read_did(0xF190, 17)
        self.assertLessEqual(ecu.now, .45)
        self.assertEqual(len(ecu.requests), 1)

    def test_p2_star(self):
        ecu = FakeECU('pending')
        with self.assertRaises(DiagnosticTimeout):
            DiagnosticLab(ecu, p2_star=.05).read_did(0xF190, 17)
        self.assertAlmostEqual(ecu.now, .06)

    def test_corrupt(self):
        for mode in ('wrong_echo', 'truncated'):
            with self.subTest(mode=mode), self.assertRaises(ResponseError):
                DiagnosticLab(FakeECU(mode)).read_did(0xF190, 17)

    def test_discover_pages(self):
        ecu = FakeECU()
        self.assertEqual(DiagnosticLab(ecu).discover_pids(), set(ecu.pids))
        self.assertEqual(ecu.requests, [b'\x01\x00', b'\x01\x20', b'\x01\x40'])

    def test_empty_support(self):
        ecu = FakeECU()
        ecu.pids = {}
        self.assertEqual(DiagnosticLab(ecu).discover_pids(), set())
        self.assertEqual(len(ecu.requests), 1)

    def test_bitmap_order(self):
        self.assertEqual(decode_support(0, bytes.fromhex('410080000001')), {1, 32})
        self.assertEqual(decode_support(32, bytes.fromhex('412080000000')), {33})

    def test_bitmap_rejects_bad(self):
        for pdu in (b'', bytes.fromhex('412000000000'), bytes.fromhex('4100000000')):
            with self.assertRaises(ResponseError): decode_support(0, pdu)

    def test_decode_known_values(self):
        for pid, pdu, value, unit in [
            (0x0C, '410C1F40', 2000, 'rpm'), (0x0D, '410D32', 50, 'km/h'),
            (0x05, '410582', 90, '°C'), (0x05, '410500', -40, '°C'),
            (0x42, '41423390', 13.2, 'V'), (0x5C, '415C88', 96, '°C'),
            (0x04, '4104FF', 100, '%'), (0x0F, '410F41', 25, '°C'),
            (0x11, '411100', 0, '%')]:
            with self.subTest(pid=pid):
                reading = decode_pid(pid, bytes.fromhex(pdu))
                self.assertAlmostEqual(reading.value, value)
                self.assertEqual(reading.unit, unit)

    def test_bad_pid_echo_length(self):
        for pdu in (b'', b'\x41\x0d\x01', b'\x41\x0c\x01', b'\x41\x0c\x01\x01\x00'):
            with self.assertRaises(ResponseError): decode_pid(0x0C, pdu)

    def test_unsupported_not_sent(self):
        ecu = FakeECU()
        with self.assertRaises(ValueError): DiagnosticLab(ecu).read_pid(0x0C, set())
        self.assertEqual(ecu.requests, [])

    def test_write_not_sent(self):
        ecu = FakeECU()
        for request in (b'\x2e\xf1\x90x', b'\x11\x01', b'\x31\x01\x00', b'', b'\x22'):
            with self.assertRaises(ValueError): DiagnosticLab(ecu)._request(request)
        self.assertEqual(ecu.requests, [])

    def test_reject_live_transport(self):
        with self.assertRaises(ValueError): DiagnosticLab(object())

    def test_invalid_timing(self):
        for duration in (0, -1, float('nan'), float('inf'), True, '1'):
            with self.assertRaises(ValueError): DiagnosticLab(FakeECU(), total=duration)

    def test_stale_queue_cleared(self):
        ecu = FakeECU()
        ecu.queue.append((0, b'\x62\xf1\x90STALE'))
        self.assertEqual(len(DiagnosticLab(ecu).read_did(0xF190, 17)), 17)

    def test_report(self):
        result = run_lab()
        self.assertFalse(result['vehicle_connected'])
        self.assertIsNone(result['error'])
        self.assertEqual(len(result['readings']), 8)
        self.assertEqual(run_lab('silent')['error']['type'], 'DiagnosticTimeout')
