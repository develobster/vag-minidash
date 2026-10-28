"""Read the CYD console or send explicit UI commands. Never flashes or opens CAN."""
import argparse
from pathlib import Path
import time
import serial

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',required=True)
    parser.add_argument('--log',type=Path,required=True)
    parser.add_argument('--reset',action='store_true',help='Pulse RTS and capture a fresh boot')
    parser.add_argument('--commands',nargs='*',default=['status'])
    parser.add_argument('--seconds',type=float,default=3)
    args=parser.parse_args()
    if not 0<=args.seconds<=60:parser.error('--seconds must be between 0 and 60')
    args.log.parent.mkdir(parents=True,exist_ok=True)
    port=serial.Serial()
    port.port=args.port;port.baudrate=115200;port.timeout=.1;port.write_timeout=5
    port.dtr=False;port.rts=False
    with port, args.log.open('ab',buffering=0) as log:
        def capture(seconds):
            end=time.monotonic()+seconds
            while time.monotonic()<end:
                chunk=port.read(port.in_waiting or 1)
                if chunk:
                    log.write(chunk)
                    print(chunk.decode('utf-8',errors='replace'),end='',flush=True)
        if args.reset:
            port.rts=True;time.sleep(.15);port.rts=False;capture(12)
        for command in args.commands:
            if '\r' in command or '\n' in command:parser.error('one command per argument')
            port.write((command+'\n').encode('utf-8'));port.flush()
            capture(14 if command=='reboot' else 7 if command in ('run','ui-test','web-test') else .5)
        capture(args.seconds)

if __name__=='__main__':main()
