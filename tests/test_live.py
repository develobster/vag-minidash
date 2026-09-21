import ctypes
import importlib.util
import json
from pathlib import Path
import tempfile
import threading
import unittest
import uuid
from urllib.request import urlopen
from minidash_live.uds import ReadClient, NegativeResponse, ProtocolError
from minidash_live.profiles import load_profile, decode
from minidash_live.j2534 import Message, message, received_pdu
from minidash_live.transports import CanTransport, validate_can
from minidash_live.api import State, serve
from minidash_live.discovery import inventory


class Pdus:
    def __init__(self,*responses): self.responses=list(responses); self.sent=[]
    def send(self,pdu,timeout): self.sent.append(pdu)
    def recv(self,timeout): return self.responses.pop(0) if self.responses else None


class LiveTests(unittest.TestCase):
    def test_pending_is_not_retransmitted(self):
        t=Pdus(bytes.fromhex('7f2278'),bytes.fromhex('62f40582'))
        self.assertEqual(ReadClient(t).read_did(0xf405),b'\x82')
        self.assertEqual(t.sent,[bytes.fromhex('22f405')])

    def test_negative_can_continue(self):
        t=Pdus(bytes.fromhex('7f2231'),bytes.fromhex('62f40582'))
        client=ReadClient(t,interval=.001)
        with self.assertRaises(NegativeResponse) as e: client.read_did(0xf405)
        self.assertEqual(e.exception.code,0x31)
        self.assertEqual(client.read_did(0xf405),b'\x82')

    def test_timeout_taints_session(self):
        t=Pdus(None,bytes.fromhex('62f40582'))
        client=ReadClient(t)
        with self.assertRaises(TimeoutError): client.read_did(0xf405)
        with self.assertRaises(ProtocolError): client.read_did(0xf405)
        self.assertEqual(len(t.sent),1)

    def test_wrong_echo_and_bad_pdu(self):
        for response in ('62f43380','62f405','7f2378','7f22','', '41f40582'):
            with self.subTest(response=response):
                with self.assertRaises(ProtocolError):
                    ReadClient(Pdus(bytes.fromhex(response))).read_did(0xf405)

    def test_pending_total_deadline(self):
        class Pending(Pdus):
            def recv(self,timeout): return bytes.fromhex('7f2278')
        t=Pending()
        with self.assertRaises(TimeoutError): ReadClient(t,total=.01).read_did(0xf405)
        self.assertEqual(len(t.sent),1)

    def test_no_invented_decoders(self):
        root=Path(__file__).resolve().parents[1]/'profiles/diagnostic'
        for path in root.glob('*.json'):
            profile=load_profile(path)
            for item in profile['dids']:
                self.assertIsNone(item['decoder'])
                reading=decode(item,b'\x00\x00')
                self.assertIsNone(reading['value'])
                self.assertEqual(reading['quality'],'unsupported' if item['status']=='unsupported' else 'raw_only')

    def test_scalar_codec_and_invalid(self):
        item={'did':'1234','signal':'fixture','unit':'unit','status':'candidate','decoder':{
            'length':2,'byteorder':'big','signed':True,'scale':.5,'offset':1,
            'status':'candidate','invalid_raw':['7fff'],'range':[-20,20]}}
        self.assertEqual(decode(item,bytes.fromhex('fffe'))['value'],0)
        self.assertEqual(decode(item,bytes.fromhex('fffe'))['quality'],'candidate')
        self.assertEqual(decode(item,b'\x00')['quality'],'invalid_length')
        self.assertEqual(decode(item,bytes.fromhex('7fff'))['quality'],'invalid')
        self.assertEqual(decode(item,bytes.fromhex('0080'))['quality'],'invalid')

    def test_decoder_is_gated_by_verified_ecu_identity(self):
        item={'did':'1234','signal':'fixture','unit':'unit','status':'candidate','decoder':{
            'length':2,'byteorder':'big','signed':False,'scale':1,'offset':0,
            'status':'confirmed','invalid_raw':[],'range':[0,65535]}}
        reading=decode(item,bytes.fromhex('1234'),identity_verified=False)
        self.assertIsNone(reading['value'])
        self.assertEqual(reading['quality'],'identity_unverified')

    def test_j2534_abi_header_and_filtering(self):
        self.assertEqual(ctypes.sizeof(Message),4152)
        m=message(0x708,bytes.fromhex('62f40582'))
        self.assertEqual(received_pdu(m,0x708),bytes.fromhex('62f40582'))
        self.assertIsNone(received_pdu(m,0x709))
        for flag in (1,2,8):
            m.RxStatus=flag
            self.assertIsNone(received_pdu(m,0x708))
        m.RxStatus=0x10
        with self.assertRaises(OSError): received_pdu(m,0x708)
        m.RxStatus=0; m.DataSize=5000
        with self.assertRaises(OSError): received_pdu(m,0x708)

    def test_no_address_defaults_or_broadcast(self):
        for tx,rx in [(0x7df,0x708),(0x700,0x700),(-1,0x708),(0x1800,0x708)]:
            with self.assertRaises(ValueError): validate_can(tx,rx,500000)

    def test_api_replaces_stale_value_on_error(self):
        state=State()
        state.update({'kind':'signal','did':'F405','value':90})
        state.update({'kind':'error','did':'F405','message':'NRC'})
        server,thread=serve(state,0)
        try:
            with urlopen(f'http://127.0.0.1:{server.server_port}/api/signals') as r: body=json.load(r)
            self.assertIsNone(body['signals']['unknown.did.F405']['value'])
            self.assertEqual(body['signals']['unknown.did.F405']['quality'],'error')
        finally:
            server.shutdown();thread.join(2);server.server_close()

    def test_doctor_does_not_perform_vehicle_io(self):
        self.assertFalse(inventory()['vehicle_io_performed'])


@unittest.skipUnless(all(importlib.util.find_spec(x) for x in ('can','isotp')),'transport dependencies required')
class LiveWireTests(unittest.TestCase):
    def test_real_isotp_multiframe_pending_and_cleanup(self):
        import can,isotp
        channel='live-test-'+uuid.uuid4().hex
        bus=can.Bus(interface='virtual',channel=channel,ignore_config=True)
        server=isotp.CanStack(bus=bus,address=isotp.Address(isotp.AddressingMode.Normal_11bits,txid=0x708,rxid=0x700))
        server.start()
        client_transport=CanTransport(channel,0x700,0x708,500000,interface='virtual')
        payload=bytes(range(256))*2
        requests=[]
        errors=[]
        def respond():
            try:
                requests.append(bytes(server.recv(block=True,timeout=2)))
                server.send(bytes.fromhex('7f2278'))
                server.send(bytes.fromhex('62f405')+payload)
            except Exception as exc: errors.append(exc)
        worker=threading.Thread(target=respond)
        worker.start()
        try:
            result=ReadClient(client_transport).read_did(0xf405)
            self.assertEqual(result,payload)
            worker.join(2)
            self.assertFalse(worker.is_alive())
            self.assertEqual(errors,[])
            self.assertEqual(requests,[bytes.fromhex('22f405')])
        finally:
            client_transport.close();server.stop();bus.shutdown();worker.join(2)

class J2534LifecycleTests(unittest.TestCase):
    def test_open_filter_read_close_and_connect_failure(self):
        from unittest.mock import Mock, patch
        from minidash_live import j2534 as j
        def put(pointer, value):
            ctypes.cast(pointer, ctypes.POINTER(j.U32))[0] = value
        for connect_error in (False, True):
            dll=Mock()
            dll.PassThruOpen.side_effect=lambda name,out: (put(out,10) or 0)
            dll.PassThruConnect.side_effect=lambda dev,proto,flags,rate,out: 7 if connect_error else (put(out,20) or 0)
            def filt(channel,kind,mask,pattern,flow,out):
                self.assertEqual((channel,kind),(20,3))
                messages=[ctypes.cast(x,ctypes.POINTER(j.Message)).contents for x in (mask,pattern,flow)]
                self.assertEqual([m.TxFlags for m in messages],[0x40]*3)
                self.assertEqual([bytes(m.Data[:4]) for m in messages],[bytes.fromhex(x) for x in ('ffffffff','00000708','00000700')])
                put(out,30)
                return 0
            dll.PassThruStartMsgFilter.side_effect=filt
            for name in ('PassThruClose','PassThruDisconnect','PassThruStopMsgFilter','PassThruWriteMsgs'):
                getattr(dll,name).return_value=0
            def read(channel,out,count,timeout):
                msg=j.message(0x708,bytes.fromhex('62f40582'))
                ctypes.memmove(out,ctypes.byref(msg),ctypes.sizeof(msg));put(count,1)
                return 0
            dll.PassThruReadMsgs.side_effect=read
            path=Mock();path.is_absolute.return_value=True;path.is_file.return_value=True
            with patch.object(j,'os') as windows, patch.object(j,'Path',return_value=path), patch.object(j.C,'WinDLL',return_value=dll,create=True):
                windows.name='nt'
                if connect_error:
                    with self.assertRaises(OSError): j.J2534Transport('driver.dll',0x700,0x708,500000)
                    dll.PassThruClose.assert_called_once_with(10)
                    dll.PassThruDisconnect.assert_not_called()
                else:
                    transport=j.J2534Transport('driver.dll',0x700,0x708,500000)
                    transport.send(bytes.fromhex('22f405'),1)
                    self.assertEqual(transport.recv(1),bytes.fromhex('62f40582'))
                    transport.close();transport.close()
                    dll.PassThruStopMsgFilter.assert_called_once_with(20,30)
                    dll.PassThruDisconnect.assert_called_once_with(20)
                    dll.PassThruClose.assert_called_once_with(10)
                windows.add_dll_directory.return_value.close.assert_called_once()

    def test_pe_inspection_reads_bitness_without_loading(self):
        import struct
        from minidash_live.discovery import pe_info
        for magic,bits,size in ((0x10b,32,224),(0x20b,64,240)):
            blob=bytearray(512);blob[:2]=b'MZ'
            struct.pack_into('<I',blob,0x3c,0x80);blob[0x80:0x84]=b'PE\0\0'
            struct.pack_into('<H',blob,0x84,0x14c if bits==32 else 0x8664)
            struct.pack_into('<H',blob,0x94,size);struct.pack_into('<H',blob,0x98,magic)
            with tempfile.TemporaryDirectory() as tmp:
                path=Path(tmp)/'fixture.dll';path.write_bytes(blob)
                self.assertEqual(pe_info(path)['bits'],bits)
                path.write_bytes(b'not executable')
                with self.assertRaises(ValueError): pe_info(path)
