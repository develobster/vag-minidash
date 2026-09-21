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
