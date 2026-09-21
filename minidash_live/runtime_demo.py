"""Offline runtime smoke: python -m minidash_live.runtime_demo. No vehicle I/O."""
import json
import time
from .runtime import EcuSession, SignalStore, Scheduler, PollJob
from .passive import Field, PassiveDecoder


class SyntheticEcu:
    def send(self, request, timeout):
        self.reply = {b"\x10\x01":bytes.fromhex("5001003201f4"),
                      b"\x22\x12\x34":bytes.fromhex("6212340320")}.get(request)
    def recv(self, timeout):
        reply, self.reply = self.reply, None
        return reply
    def close(self): pass


def main():
    store = SignalStore()
    session = EcuSession(SyntheticEcu, verify=lambda client: True)
    job = PollJob("synthetic", "fixture.rpm", 0x1234, 1, .01,
                  lambda raw: dict(value=int.from_bytes(raw, "big"), quality="valid", unit="rpm"))
    scheduler = Scheduler({"synthetic":session}, store, [job], cooldown=.2)
    try:
        scheduler.step()
        passive = PassiveDecoder([Field("synthetic", "fixture.speed", 0x123, False, 1, 0, 1,
                                        "km/h", evidence="synthetic fixture only")], store, profile_verified=True)
        passive.feed(0x123, b"\x05")
        print(json.dumps(dict(mode="synthetic_no_vehicle", signals=store.snapshot(), stats=scheduler.stats)))
        time.sleep(.02)
        print(json.dumps(dict(after_ttl=store.snapshot())))
    finally:
        session.close()


if __name__ == "__main__": main()
