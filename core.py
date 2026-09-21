"""Offline development core. No vehicle transport is implemented."""
from collections import deque
from dataclasses import dataclass
from datetime import datetime, timezone
import json
from pathlib import Path

@dataclass(frozen=True)
class Change:
    identity: str
    key: str
    before: bool
    after: bool

class Simulator:
    identity = 'SIMULATOR-ONLY:BCM:1'
    def __init__(self):
        self.values = {'demo_door_light': False, 'demo_mirror_fold': False}
        self.speed = 0
        self.voltage = 13.2
    def read(self, key):
        return self.values[key]
    def write(self, key, value):
        self.values[key] = value

class Coding:
    def __init__(self, transport, journal):
        if type(transport) is not Simulator:
            raise ValueError('Only simulator transport is supported')
        self.transport, self.journal = transport, Path(journal)
    def prepare(self, key, value):
        if type(value) is not bool:
            raise ValueError('Boolean required')
        return Change(self.transport.identity, key, self.transport.read(key), value)
    def apply(self, change):
        t = self.transport
        if change.identity != t.identity or t.read(change.key) != change.before:
            raise ValueError('Identity or original value changed; prepare again')
        if t.speed != 0 or not 12.5 <= t.voltage <= 15:
            raise ValueError('Simulator must be stopped with stable voltage')
        record = dict(time=datetime.now(timezone.utc).isoformat(), **change.__dict__)
        self.journal.parent.mkdir(parents=True, exist_ok=True)
        # Persist original value before attempting any change.
        with self.journal.open('a', encoding='utf8') as f:
            f.write(json.dumps({**record, 'state': 'prepared'}) + '\n')
            f.flush()
            import os
            os.fsync(f.fileno())
        t.write(change.key, change.after)
        if t.read(change.key) != change.after:
            raise RuntimeError('Readback mismatch; state uncertain; no automatic retry')
        with self.journal.open('a', encoding='utf8') as f:
            f.write(json.dumps({**record, 'state': 'verified'}) + '\n')
        return Change(change.identity, change.key, change.after, change.before)

class Recorder:
    def __init__(self, capacity=300, post_samples=50):
        self.ring = deque(maxlen=capacity)
        self.events = []
        self.pending = None
        self.post_samples = post_samples
        self.remaining = 0
        self.last_gear = None
    def feed(self, sample):
        sample = dict(sample)
        self.ring.append(sample)
        if self.pending is not None:
            self.pending['samples'].append(sample)
            self.remaining -= 1
            if self.remaining <= 0:
                self.events.append(self.pending)
                self.pending = None
        elif self.last_gear == 2 and sample['gear'] == 1:
            self.pending = {'trigger': 'D2→D1', 'samples': list(self.ring)}
            self.remaining = self.post_samples
        self.last_gear = sample['gear']
    def export(self, path):
        Path(path).write_text(json.dumps({'source': 'simulation', 'samples': list(self.ring), 'events': self.events, 'pending_event': self.pending}, ensure_ascii=False, indent=2), encoding='utf8')
