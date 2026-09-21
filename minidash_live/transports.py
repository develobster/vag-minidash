"""Complete-PDU adapters. No implicit vehicle addresses or automatic fallback."""
from contextlib import ExitStack
from queue import SimpleQueue


def validate_can(txid, rxid, bitrate):
    if any(type(x) is not int or not 0 <= x <= 0x7ff for x in (txid,rxid)) or txid == rxid:
        raise ValueError('Distinct verified physical 11-bit CAN IDs required')
    if txid == 0x7df:
        raise ValueError('Functional broadcast addressing is not supported')
    if type(bitrate) is not int or bitrate <= 0:
        raise ValueError('Explicit positive bitrate required')


class CanTransport:
    """python-can + can-isotp; owns its bus. Also usable with VirtualBus in tests."""
    def __init__(self, channel, txid, rxid, bitrate, interface='socketcan'):
        validate_can(txid, rxid, bitrate)
        import can
        import isotp
        self.resources = ExitStack()
        self.errors = SimpleQueue()
        try:
            bus = can.Bus(interface=interface, channel=channel, bitrate=bitrate, ignore_config=True)
            self.resources.callback(bus.shutdown)
            notifier = can.Notifier(bus, [])
            self.resources.callback(notifier.stop)
            self.stack = isotp.NotifierBasedCanStack(bus=bus, notifier=notifier,
                address=isotp.Address(isotp.AddressingMode.Normal_11bits, txid=txid, rxid=rxid),
                params={'stmin':1,'blocksize':8,'tx_padding':0,'blocking_send':True,
                        'max_frame_size':4095}, error_handler=self.errors.put)
            self.stack.start()
            self.resources.callback(self.stack.stop)
        except BaseException:
            self.resources.close()
            raise

    def check(self):
        if not self.errors.empty():
            raise OSError(f'ISO-TP failure: {self.errors.get()}')

    def send(self, pdu, timeout):
        self.check()
        self.stack.send(pdu, send_timeout=timeout)

    def recv(self, timeout):
        response = self.stack.recv(block=True, timeout=timeout)
        self.check()
        return bytes(response) if response is not None else None

    def close(self):
        self.resources.close()
