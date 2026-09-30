import tempfile
import unittest
import socket
from pathlib import Path
from unittest.mock import patch
from can_capture import FRAME,decode_frame,capture,inspect
class CANTests(unittest.TestCase):
    def test_standard(self):
        r=decode_frame(FRAME.pack(0x123,2,b'\x10\x20'))
        self.assertEqual(r['data'],'1020')
        self.assertFalse(r['extended'])
    def test_extended(self):
        self.assertEqual(decode_frame(FRAME.pack(0x80012345,0,b''))['id'],0x12345)
    def test_remote(self):
        self.assertEqual(decode_frame(FRAME.pack(0x40000123,8,b'garbage!'))['data'],'')
    def test_error(self):
        r=decode_frame(FRAME.pack(0x20001000,8,b''))
        self.assertTrue(r['error'])
        self.assertEqual(r['id'],0x1000)
    def test_malformed(self):
        for b in [b'',bytes(72),FRAME.pack(1,9,b'')]:
            with self.assertRaises(ValueError): decode_frame(b)
    @unittest.skipUnless(hasattr(socket,'AF_CAN'), 'SocketCAN requires Linux AF_CAN')
    def test_capture(self):
        with tempfile.TemporaryDirectory() as d,patch('can_capture.socket.socket') as factory:
            bus=factory.return_value.__enter__.return_value
            bus.recv.return_value=FRAME.pack(0x123,1,b'\xaa')
            p=Path(d)/'capture.jsonl'
            self.assertEqual(capture('vcan0',p,1,2),2)
            bus.bind.assert_called_once_with(('vcan0',))
            bus.send.assert_not_called()
            bus.sendto.assert_not_called()
            self.assertEqual(inspect(p)[0]['count'],2)
            with self.assertRaises(FileExistsError): capture('vcan0',p,1,2)
    def test_limits(self):
        for v in [0,-1,float('nan'),float('inf')]:
            with self.assertRaises(ValueError): capture('vcan0','unused',v)
