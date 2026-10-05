"""GET-only collector for the device's temporary local validation evidence API.

Optional development tool; acquisition/display never depend on this process.
No CAN interface, diagnostic command, configuration write or device reset.
"""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import time
from urllib.parse import urlsplit
from urllib.request import urlopen


class EvidenceTracker:
    def __init__(self):
        self.previous = {}
        self.uptime = None
        self.epoch = 0

    def inspect(self, snapshot):
        if snapshot.get('schema') != 1 or snapshot.get('validation_mode') is not True:
            raise ValueError('Device must run a car-validation preset (schema 1)')
        uptime = snapshot['uptime_ms']
        if type(uptime) is not int or uptime < 0:
            raise ValueError('Invalid device uptime')
        if self.uptime is not None and uptime < self.uptime:
            self.epoch += 1
            self.previous.clear()
        gaps = {}
        for name, counter in (('frames', 'trace_sequence'), ('exchanges', 'exchange_sequence')):
            rows = snapshot[name]
            latest = snapshot[counter]
            if type(latest) is not int or not 0 <= latest <= 0xffffffff or not isinstance(rows, list):
                raise ValueError('Invalid evidence sequence')
            if len(rows) > (96 if name == 'frames' else 16):
                raise ValueError('Evidence ring exceeds capacity')
            if rows:
                sequences = [row['sequence'] for row in rows]
                if any(type(n) is not int or not 0 <= n <= 0xffffffff for n in sequences):
                    raise ValueError('Invalid ring sequence')
                if sequences[-1] != latest or any((b-a) & 0xffffffff != 1
                                                  for a, b in zip(sequences, sequences[1:])):
                    raise ValueError('Evidence ring is not contiguous')
            delta = (latest - self.previous.get(name, 0)) & 0xffffffff
            gaps[name] = max(0, delta-len(rows))
            self.previous[name] = latest
        self.uptime = uptime
        return dict(epoch=self.epoch, missing_since_previous=gaps,
                    complete_since_previous=not any(gaps.values()))


def collect(url, output, duration, interval, *, fetch=None, clock=time.monotonic, sleep=time.sleep):
    parts = urlsplit(url)
    if parts.scheme != 'http' or not parts.hostname or parts.username or parts.password \
            or parts.path not in ('', '/') or parts.query or parts.fragment:
        raise ValueError('Use the local device HTTP base URL without a path or credentials')
    if not 0 < duration <= 600 or not .25 <= interval <= 10:
        raise ValueError('Duration must be 0–600 s and interval 0.25–10 s')
    endpoint = url.rstrip('/') + '/api/validation'
    tracker = EvidenceTracker()
    if fetch is None:
        def fetch():
            with urlopen(endpoint, timeout=5) as response:
                data = response.read(262145)
            if len(data) > 262144:
                raise ValueError('Snapshot too large')
            return json.loads(data)
    with Path(output).open('x', encoding='utf-8', buffering=1) as stream:
        deadline = clock() + duration
        while clock() < deadline:
            snapshot = fetch()
            integrity = tracker.inspect(snapshot)
            row = dict(schema=1, captured_utc=datetime.now(timezone.utc).isoformat(),
                       integrity=integrity, device=snapshot)
            stream.write(json.dumps(row, ensure_ascii=False, allow_nan=False) + '\n')
            if not integrity['complete_since_previous']:
                print('Evidence gap:', integrity['missing_since_previous'], flush=True)
            sleep(interval)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--url', default='http://192.168.4.1')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--duration', type=float, default=60)
    parser.add_argument('--interval', type=float, default=.5)
    args = parser.parse_args()
    try:
        collect(args.url, args.output, args.duration, args.interval)
    except (OSError, ValueError, KeyError, TypeError) as exc:
        parser.exit(1, f'Capture stopped; preserve any partial output: {exc}\n')
    print(f'Saved private car evidence: {args.output}')


if __name__ == '__main__':
    main()
