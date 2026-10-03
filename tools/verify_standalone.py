"""Deterministic VirtualBus integration of the real firmware C acquisition core.

No hardware interfaces or wall-clock acquisition deadlines. Synthetic ECU data
is deliberately not vehicle validation evidence.
"""
import argparse
from pathlib import Path
import subprocess
import can


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--harness', type=Path, required=True)
    args = parser.parse_args()
    process = subprocess.Popen([str(args.harness.resolve())], stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE, text=True, bufsize=1)
    device = can.Bus(interface='virtual', channel='minidash-offline')
    ecu = can.Bus(interface='virtual', channel='minidash-offline')
    pending, trace, transcript = [], [], []
    online = True

    def command(text):
        process.stdin.write(text + '\n')
        process.stdin.flush()
        rows, raw = [], []
        while True:
            line = process.stdout.readline().strip()
            raw.append(line)
            if line == 'END':
                transcript.append((text, raw))
                return rows
            if not line:
                raise RuntimeError('C harness exited before command completion')
            if line.startswith('TX '):
                _, ident, data = line.split()
                message = can.Message(arbitration_id=int(ident, 16), data=bytes.fromhex(data), is_extended_id=False)
                trace.append((text, message.arbitration_id, data))
                device.send(message)
            else:
                rows.append(line)

    def reply(payload):
        if len(payload) <= 7:
            frames = [bytes([len(payload)]) + payload]
        else:
            frames = [bytes([0x10 | (len(payload) >> 8), len(payload) & 255]) + payload[:6]]
            pending[:] = [bytes([0x20 | (i & 15)]) + payload[offset:offset + 7]
                          for i, offset in enumerate(range(6, len(payload), 7), 1)]
        for data in frames:
            ecu.send(can.Message(arbitration_id=0x7e8, data=data.ljust(8, b'\0'), is_extended_id=False))

    def pump(now):
        for _ in range(32):
            tx, rx = ecu.recv(timeout=0), device.recv(timeout=0)
            if tx is None and rx is None:
                return
            if tx is not None:
                d = tx.data
                assert tx.arbitration_id in (0x7df, 0x7e0)
                if not online:
                    pending.clear()
                elif d[0] == 0x30:
                    for frame in pending:
                        ecu.send(can.Message(arbitration_id=0x7e8, data=frame.ljust(8, b'\0'), is_extended_id=False))
                    pending.clear()
                elif d[1] == 1 and d[2] == 0:
                    reply(bytes.fromhex('410000180000'))
                elif d[1] == 9 and d[2] == 2:
                    reply(b'\x49\x02\x01TESTVAN1234567890')
                elif d[1] == 1 and d[2] == 0x0c:
                    reply(bytes.fromhex('410C3200'))
                elif d[1] == 1 and d[2] == 0x0d:
                    reply(bytes.fromhex('410D00'))
                elif d[1] == 3:
                    reply(bytes.fromhex('43030133C1230300'))
                else:
                    raise AssertionError(f'Unexpected production TX: {d.hex()}')
            if rx is not None:
                command(f'rx {now} {rx.arbitration_id:x} {rx.data.hex()}')
        raise AssertionError('Unbounded exchange')

    def run(start, stop):
        for now in range(start, stop, 50):
            command(f'tick {now}')
            pump(now)

    try:
        run(0, 5000)
        status = command('status 5000')[0]
        assert 'LIVE TESTVAN1234567890' in status and '3200' in status, status
        assert command('dtc 5000') == ['DTC_REQUEST 1']
        run(5000, 6500)
        assert ' 1 3 ' in command('status 6500')[0]
        online = False
        run(6500, 12000)
        assert command('status 12000')[0].startswith('STATUS QUIET ')
        tx_count = len(trace)
        run(12000, 30000)
        assert len(trace) == tx_count, 'quiet must stop all TX'
        command('bus 30000 0')
        assert command('status 30000')[0].startswith('STATUS CAN FAULT ')
        online = True
        command('bus 31000 1')
        run(31000, 36000)
        assert 'LIVE TESTVAN1234567890' in command('status 36000')[0]
        assert any(data.startswith('30000A') for _, _, data in trace)
        replay = subprocess.run([str(args.harness.resolve())],
                                input=''.join(text + '\n' for text, _ in transcript),
                                capture_output=True, text=True, timeout=10, check=True)
        expected = [line for _, lines in transcript for line in lines]
        assert replay.stdout.splitlines() == expected, 'synthetic replay diverged'
        print(f'PASS: firmware C over VirtualBus, VIN/DTC ISO-TP, ECU loss, quiet, bus fault, reconnect and exact synthetic replay; {len(trace)} TX frames')
    finally:
        device.shutdown()
        ecu.shutdown()
        process.stdin.close()
        try:
            process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
        process.stdout.close()


if __name__ == '__main__':
    main()
