"""Actual UDS/ISO-TP libraries over an isolated python-can VirtualBus.

No hardware/interface/channel arguments are exposed. Every run creates its own
random in-process channel. CAN IDs below belong to this test fixture only.
"""
import argparse
from contextlib import ExitStack
import json
from queue import Queue
import threading
import time
import uuid


def run_transport_lab(scenario='normal'):
    if scenario not in ('normal', 'pending', 'negative', 'silent', 'truncated'):
        raise ValueError('Unknown transport scenario')
    try:
        import can
        import isotp
        import udsoncan
        from udsoncan.client import Client
        from udsoncan.connections import PythonIsoTpConnection
        from udsoncan.configs import default_client_config
    except ImportError as exc:
        raise RuntimeError('Install requirements-transport.txt in your virtual environment') from exc

    events = Queue()
    stop = threading.Event()
    channel = 'minidash-' + uuid.uuid4().hex
    vin = b'WVWZZZ00000000000'
    large = b'LAB-DATA-' * 20  # synthetic 180-byte payload exercises multiple FC blocks
    result = {'source':'python_can_virtual', 'scenario':scenario,
              'hardware_connected':False, 'socketcan_tested':False,
              'vin':None, 'large_payload_bytes':0, 'error':None}
    frames = []
    with ExitStack() as resources:
        buses = []
        for _ in range(3):
            # Bypass environment / can config files that might name physical hardware.
            bus = can.Bus(interface='virtual', channel=channel, ignore_config=True)
            resources.callback(bus.shutdown)
            buses.append(bus)
        client_bus, server_bus, monitor_bus = buses
        params = {'stmin':1, 'blocksize':8, 'tx_data_length':8,
                  'rx_flowcontrol_timeout':500, 'rx_consecutive_frame_timeout':500}
        client_stack = isotp.CanStack(bus=client_bus, address=isotp.Address(
            isotp.AddressingMode.Normal_11bits, txid=0x700, rxid=0x708),
            params=params.copy(), error_handler=lambda e: events.put({'transport_error':str(e)}))
        server_stack = isotp.CanStack(bus=server_bus, address=isotp.Address(
            isotp.AddressingMode.Normal_11bits, txid=0x708, rxid=0x700),
            params=params.copy(), error_handler=lambda e: events.put({'transport_error':str(e)}))
        server_stack.start()
        resources.callback(server_stack.stop)

        def serve():
            try:
                while not stop.is_set():
                    request = server_stack.recv(block=True, timeout=.05)
                    if request is None:
                        continue
                    request = bytes(request)
                    events.put({'request':request.hex()})
                    # This fake ECU supports exactly two read requests.
                    payload = {b'\x22\xf1\x90':vin, b'\x22\xf1\xa0':large}.get(request)
                    if scenario == 'silent':
                        continue
                    if payload is None:
                        response = b'\x7f\x22\x31'
                    elif scenario == 'negative':
                        response = b'\x7f\x22\x22'
                    elif scenario == 'truncated':
                        response = b'\x62\xf1'
                    else:
                        response = b'\x62' + request[1:] + payload
                    if scenario == 'pending':
                        server_stack.send(b'\x7f\x22\x78')
                        if stop.wait(.08):
                            return
                    server_stack.send(response)
            except Exception as exc:
                events.put({'server_error':repr(exc)})

        worker = threading.Thread(target=serve, name='minidash-virtual-ecu', daemon=True)
        worker.start()

        def end_worker():
            stop.set()
            worker.join(timeout=2)
            if worker.is_alive():
                raise RuntimeError('Virtual ECU worker did not stop')

        resources.callback(end_worker)
        config = default_client_config.copy()
        config.update({'data_identifiers': {0xF190:udsoncan.AsciiCodec(17),
                                            0xF1A0:udsoncan.AsciiCodec(len(large))},
                       'p2_timeout':.3, 'p2_star_timeout':.5, 'request_timeout':1.5,
                       'use_server_timing':False})
        connection = PythonIsoTpConnection(client_stack)
        started = time.monotonic()
        try:
            with Client(connection, config=config) as client:
                result['vin'] = client.read_data_by_identifier_first(0xF190)
                received = client.read_data_by_identifier_first(0xF1A0)
                if received.encode('ascii') != large:
                    raise ValueError('Large payload mismatch')
                result['large_payload_bytes'] = len(received)
        except Exception as exc:
            result['error'] = {'type':type(exc).__name__, 'message':str(exc)}
        result['elapsed_seconds'] = round(time.monotonic() - started, 3)
        # Independent monitor endpoint: never drains either ISO-TP endpoint.
        for _ in range(500):
            message = monitor_bus.recv(timeout=.02)
            if message is None:
                break
            data = bytes(message.data)
            frames.append({'id':f'{message.arbitration_id:03X}', 'data':data.hex(),
                           'pci_type':data[0] >> 4 if data else None})
    result['events'] = []
    while not events.empty():
        result['events'].append(events.get_nowait())
    result['frames'] = frames
    result['frame_counts'] = {name:sum(f['pci_type']==n for f in frames)
                              for n,name in enumerate(('single','first','consecutive','flow_control'))}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--scenario', choices=('normal','pending','negative','silent','truncated'), default='normal')
    args = parser.parse_args()
    try:
        result = run_transport_lab(args.scenario)
    except (RuntimeError, ValueError) as exc:
        parser.exit(1, str(exc)+'\n')
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 1 if result['error'] else 0


if __name__ == '__main__':
    raise SystemExit(main())
