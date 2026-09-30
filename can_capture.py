"""Linux Classical CAN recorder. No transmit API."""
import argparse
import json
import math
import socket
import struct
import time
from collections import Counter
from pathlib import Path
FRAME = struct.Struct('=IB3x8s')


def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f'Duplicate trace key: {key}')
        result[key] = value
    return result


def _finite_time(value):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError('Trace timestamp must be finite and non-negative')
    try:
        value = float(value)
    except OverflowError as exc:
        raise ValueError('Trace timestamp exceeds finite range') from exc
    if not math.isfinite(value) or value < 0:
        raise ValueError('Trace timestamp must be finite and non-negative')
    return value


def _invalid_constant(value):
    raise ValueError(f'Invalid JSON constant: {value}')


class CanTraceReader:
    """Bounded, receive-only reader for the recorder's schema-1 Classical CAN log.

    Metadata and timestamps describe capture conditions, not vehicle validation.
    An interrupted log without an end record is replayable but marked incomplete.
    Malformed records, decreasing time and inconsistent end counts fail closed.
    """
    MAX_LINE = 8192

    def __init__(self, path, *, max_frames=100000):
        if type(max_frames) is not int or max_frames <= 0:
            raise ValueError('Positive integer frame limit required')
        self.path, self.max_frames = Path(path), max_frames
        self.metadata, self.complete, self.frames = None, False, 0

    def __iter__(self):
        self.metadata, self.complete, self.frames = None, False, 0
        previous = None
        with self.path.open(encoding='utf-8') as source:
            while True:
                line = source.readline(self.MAX_LINE + 1)
                if not line:
                    break
                if len(line) > self.MAX_LINE:
                    raise ValueError('CAN trace line exceeds 8192 characters')
                row = json.loads(line, object_pairs_hook=_unique_object,
                                 parse_constant=_invalid_constant)
                if not isinstance(row, dict):
                    raise ValueError('Trace record must be an object')
                if self.complete:
                    raise ValueError('Record after CAN trace end')
                if self.metadata is None:
                    if row.get('type') != 'metadata' or type(row.get('schema')) is not int \
                            or row['schema'] != 1 or \
                            row.get('timestamp_kind') != 'userspace_receive':
                        raise ValueError('Expected schema-1 userspace_receive metadata')
                    self.metadata = row
                    continue
                if row.get('type') == 'end':
                    if type(row.get('frames')) is not int or row['frames'] != self.frames:
                        raise ValueError('Trace end frame count mismatch')
                    self.complete = True
                    continue
                if row.get('type') != 'frame':
                    raise ValueError('Unknown CAN trace record')
                if self.frames >= self.max_frames:
                    raise ValueError('CAN trace frame limit exceeded')
                for flag in ('extended', 'remote', 'error'):
                    if type(row.get(flag)) is not bool:
                        raise ValueError(f'Trace flag {flag} must be boolean')
                limit = 0x1fffffff if row['extended'] or row['error'] else 0x7ff
                if type(row.get('id')) is not int or not 0 <= row['id'] <= limit:
                    raise ValueError('CAN trace identifier out of range')
                if type(row.get('length')) is not int or not 0 <= row['length'] <= 8:
                    raise ValueError('Expected Classical CAN DLC 0..8')
                data = row.get('data')
                if not isinstance(data, str) or any(c not in '0123456789abcdefABCDEF' for c in data):
                    raise ValueError('CAN trace data must be unseparated hexadecimal')
                expected = 0 if row['remote'] else row['length'] * 2
                if len(data) != expected:
                    raise ValueError('CAN trace data length does not match DLC')
                timestamp = _finite_time(row.get('monotonic'))
                if 'wall_time' in row:
                    _finite_time(row['wall_time'])
                if previous is not None and timestamp < previous:
                    raise ValueError('Decreasing CAN trace monotonic timestamp')
                previous = timestamp
                row['monotonic'] = timestamp
                self.frames += 1
                yield row
        if self.metadata is None:
            raise ValueError('Empty CAN trace')

def decode_frame(raw):
    if len(raw) != 16: raise ValueError('Expected Classical CAN frame (16 bytes)')
    cid, size, data = FRAME.unpack(raw)
    if size > 8: raise ValueError('Invalid Classical CAN payload length')
    extended, remote, error = [bool(cid & mask) for mask in (0x80000000,0x40000000,0x20000000)]
    return dict(id=cid & (0x1fffffff if extended or error else 0x7ff), extended=extended,
                remote=remote, error=error, length=size, data='' if remote else data[:size].hex())

def capture(channel, output, seconds=60, limit=100000):
    if not channel or not math.isfinite(seconds) or seconds <= 0 or limit <= 0:
        raise ValueError('Positive finite duration, frame limit and channel required')
    if not hasattr(socket,'AF_CAN'): raise RuntimeError('Requires Linux SocketCAN')
    count=0
    with Path(output).open('x',encoding='utf8') as f:
        f.write(json.dumps(dict(type='metadata',schema=1,source='socketcan',channel=channel,
            timestamp_kind='userspace_receive',hardware_listen_only_verified=False))+'\n')
        with socket.socket(socket.AF_CAN,socket.SOCK_RAW,socket.CAN_RAW) as bus:
            bus.settimeout(min(.2,seconds))
            bus.setsockopt(socket.SOL_CAN_RAW,2,struct.pack('=I',0x1fffffff))
            bus.bind((channel,))
            start=time.monotonic()
            try:
                while count<limit and time.monotonic()-start<seconds:
                    try: raw=bus.recv(72)
                    except socket.timeout: continue
                    row=decode_frame(raw)
                    row.update(type='frame',wall_time=time.time(),monotonic=time.monotonic())
                    f.write(json.dumps(row)+'\n')
                    count+=1
                    if count%100==0: f.flush()
            except KeyboardInterrupt: pass
        f.write(json.dumps(dict(type='end',frames=count))+'\n')
    return count

def inspect(path):
    counts=Counter()
    with Path(path).open(encoding='utf8') as f:
        for line in f:
            r=json.loads(line)
            if r.get('type')=='frame': counts[(r['id'],r['extended'],r['remote'],r['error'])]+=1
    return [dict(id=hex(k[0]),extended=k[1],remote=k[2],error=k[3],count=v) for k,v in sorted(counts.items())]

def main():
    p=argparse.ArgumentParser(description=__doc__)
    s=p.add_subparsers(dest='command',required=True)
    c=s.add_parser('capture')
    c.add_argument('--channel',required=True)
    c.add_argument('--output',required=True)
    c.add_argument('--seconds',type=float,default=60)
    c.add_argument('--max-frames',type=int,default=100000)
    s.add_parser('inspect').add_argument('path')
    a=p.parse_args()
    try:
        if a.command=='capture': print('Frames saved:',capture(a.channel,a.output,a.seconds,a.max_frames))
        else: print(json.dumps(inspect(a.path),indent=2))
    except (OSError,ValueError,RuntimeError) as e: p.exit(1,f'Error: {e}\n')
if __name__=='__main__': main()
