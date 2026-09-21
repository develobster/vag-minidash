"""Windows J2534-1 04.04 ISO15765, normal 11-bit addressing only.

Generic adapter implementation, NOT a claim that VNCI installs a J2534 DLL.
DLL path and physical CAN IDs must come from local discovery/evidence.
"""
import ctypes as C
import os
import time
from pathlib import Path
from .transports import validate_can

U32 = C.c_uint32
P32 = C.POINTER(U32)


class Message(C.Structure):
    _fields_ = [(name, U32) for name in ('ProtocolID','RxStatus','TxFlags','Timestamp','DataSize','ExtraDataIndex')] + [('Data',C.c_ubyte * 4128)]


def message(can_id, payload=b'', padding=False):
    data = can_id.to_bytes(4,'big') + payload
    if len(data) > 4128:
        raise ValueError('PDU too large')
    result = Message()
    result.ProtocolID, result.TxFlags, result.DataSize = 6, 0x40 if padding else 0, len(data)
    result.Data[:len(data)] = data
    return result


def received_pdu(msg, rxid):
    if msg.ProtocolID != 6 or msg.RxStatus & (0x01 | 0x02 | 0x08):
        return None  # TX indications/loopback and first-frame-only notifications
    if msg.RxStatus & 0x10:
        raise OSError('ISO15765 padding error')
    if not 4 <= msg.DataSize <= 4128:
        raise OSError('Invalid J2534 DataSize')
    end = msg.ExtraDataIndex or msg.DataSize
    if not 4 <= end <= msg.DataSize:
        raise OSError('Invalid J2534 ExtraDataIndex')
    if bytes(msg.Data[:4]) != rxid.to_bytes(4,'big'):
        return None
    return bytes(msg.Data[4:end])


class J2534Transport:
    def __init__(self, dll, txid, rxid, bitrate):
        validate_can(txid, rxid, bitrate)
        if os.name != 'nt':
            raise OSError('J2534 DLL requires Windows and matching Python/DLL bitness')
        path = Path(dll)
        if not path.is_absolute() or not path.is_file():
            raise ValueError('Existing absolute FunctionLibrary DLL path required')
        self.device, self.channel, self.filter = None, None, None
        self.txid, self.rxid = txid, rxid
        self.search = os.add_dll_directory(str(path.parent))
        try:
            self.dll = C.WinDLL(str(path))
            pm = C.POINTER(Message)
            signatures = {
                'PassThruOpen':[C.c_void_p,P32], 'PassThruClose':[U32],
                'PassThruConnect':[U32,U32,U32,U32,P32], 'PassThruDisconnect':[U32],
                'PassThruStartMsgFilter':[U32,U32,pm,pm,pm,P32],
                'PassThruStopMsgFilter':[U32,U32],
                'PassThruReadMsgs':[U32,pm,P32,U32], 'PassThruWriteMsgs':[U32,pm,P32,U32],
            }
            for name,args in signatures.items():
                fn = getattr(self.dll,name)
                fn.argtypes, fn.restype = args, C.c_int32
            device = U32()
            self.call('PassThruOpen',None,C.byref(device))
            self.device = device.value
            channel = U32()
            self.call('PassThruConnect',self.device,6,0,bitrate,C.byref(channel))
            self.channel = channel.value
            mask, pattern, flow = message(0xffffffff,padding=True), message(rxid,padding=True), message(txid,padding=True)
            fid = U32()
            self.call('PassThruStartMsgFilter',self.channel,3,C.byref(mask),C.byref(pattern),C.byref(flow),C.byref(fid))
            self.filter = fid.value
        except BaseException:
            self.close()
            raise

    def call(self,name,*args):
        result = getattr(self.dll,name)(*args)
        if result:
            raise OSError(f'{name}: J2534 status 0x{result:08X}')

    def send(self,pdu,timeout):
        msg, count = message(self.txid,pdu,padding=True), U32(1)
        self.call('PassThruWriteMsgs',self.channel,C.byref(msg),C.byref(count),max(1,int(timeout*1000)))
        if count.value != 1:
            raise OSError('J2534 did not accept request')

    def recv(self,timeout):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            msg, count = Message(), U32(1)
            status = self.dll.PassThruReadMsgs(self.channel,C.byref(msg),C.byref(count),
                        max(1,int((deadline-time.monotonic())*1000)))
            if status not in (0,9,16):  # ERR_TIMEOUT / ERR_BUFFER_EMPTY
                raise OSError(f'PassThruReadMsgs: J2534 status 0x{status:08X}')
            if count.value:
                payload = received_pdu(msg,self.rxid)
                if payload is not None:
                    return payload
            elif status in (9,16):
                time.sleep(min(.005,max(0,deadline-time.monotonic())))
        return None

    def close(self):
        # Cleanup must attempt all stages even if device has disconnected.
        if getattr(self,'dll',None):
            for attribute, name, prefix in [('filter','PassThruStopMsgFilter',[self.channel]),
                                          ('channel','PassThruDisconnect',[]),
                                          ('device','PassThruClose',[])]:
                value = getattr(self,attribute)
                if value is not None:
                    try:
                        self.call(name,*prefix,value)
                    except OSError:
                        pass
                    setattr(self,attribute,None)
        if getattr(self,'search',None):
            self.search.close()
            self.search = None
