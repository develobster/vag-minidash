import unittest
from minidash_live.uds import ReadClient, ProtocolError
from minidash_live.runtime import EcuSession, SignalStore, PollJob, Scheduler
from minidash_live.passive import Field, PassiveDecoder


class Wire:
    def __init__(self, responses):
        self.responses, self.sent, self.closed = list(responses), [], False
    def send(self, pdu, timeout):
        self.sent.append(pdu)
    def recv(self, timeout):
        return self.responses.pop(0) if self.responses else None
    def close(self):
        self.closed = True


class RuntimeTests(unittest.TestCase):
    def setUp(self):
        self.now = 0
        self.clock = lambda: self.now
    def session(self, wire, **kwargs):
        return EcuSession(lambda: wire, clock=self.clock,
                          client_options={"interval":.00001}, **kwargs)
    def test_session_and_tester_present(self):
        wire = Wire([bytes.fromhex("5003003201f4"), b"\x7e\x00"])
        client = ReadClient(wire, interval=.00001)
        client.session_control(3, allow_extended=True)
        self.assertEqual((client.p2, client.p2_star), (.05, 5))
        client.tester_present()
        self.assertEqual(wire.sent, [b"\x10\x03", b"\x3e\x00"])
    def test_unsafe_services_rejected(self):
        wire = Wire([])
        client = ReadClient(wire)
        for session in (0, 2, 3, 4, True):
            with self.assertRaises(ValueError): client.session_control(session)
        for pdu in (b"\x11\x01", b"\x27\x01", b"\x2e\x12\x34", b"\x31\x01"):
            with self.assertRaises(ValueError): client._exchange(pdu)
        self.assertEqual(wire.sent, [])
    def test_malformed_session_taints(self):
        for raw in ("5001", "5002003201f4", "500100000000", "5001ffff01f4"):
            client = ReadClient(Wire([bytes.fromhex(raw)]))
            with self.assertRaises(ProtocolError): client.session_control()
            self.assertTrue(client.tainted)
    def test_pending_session_no_resend(self):
        wire = Wire([bytes.fromhex("7f1078"), bytes.fromhex("5001003201f4")])
        ReadClient(wire).session_control()
        self.assertEqual(wire.sent, [b"\x10\x01"])
    def test_reconnect_fresh_transport_and_reverify(self):
        wires = [Wire([bytes.fromhex("5001003201f4"), None]),
                 Wire([bytes.fromhex("5001003201f4"), bytes.fromhex("62123455")])]
        opened, verified = [], []
        def factory():
            wire = wires[len(opened)]; opened.append(wire); return wire
        session = EcuSession(factory, clock=self.clock, verify=lambda c: verified.append(c) or True,
                             client_options={"interval":.00001})
        with self.assertRaises(TimeoutError): session.read(0x1234)
        self.assertTrue(wires[0].closed)
        self.assertFalse(session.identity_verified)
        self.assertFalse(session.ready())
        self.now = 1
        self.assertEqual(session.read(0x1234), b"\x55")
        self.assertEqual(len(verified), 2)
        session.close()
    def test_keepalive_and_expiry(self):
        wire = Wire([bytes.fromhex("5003003201f4"), b"\x7e\x00"])
        session = self.session(wire, session=3, allow_extended=True)
        self.assertTrue(session.ready())
        self.now = 1.1
        self.assertTrue(session.maintain())
        self.assertEqual(wire.sent[-1], b"\x3e\x00")
        self.now = 7
        session.ready()
        self.assertTrue(wire.closed)
    def test_freshness_and_sources(self):
        store = SignalStore(self.clock)
        store.publish("engine", "rpm", "uds", 800, 1)
        store.publish("engine", "rpm", "can", 801, 2)
        self.now = 1
        self.assertEqual(store.snapshot()[0]["quality"], "stale")
        self.assertIsNone(store.snapshot()[0]["value"])
        store.invalidate("engine")
        self.assertEqual(store.snapshot()[1]["value"], 801)
    def test_scheduler_failure_isolation_and_unknown_identity(self):
        bad = self.session(Wire([]))
        good = self.session(Wire([bytes.fromhex("5001003201f4"), bytes.fromhex("62123455")]))
        store = SignalStore(self.clock)
        def forbidden(raw): raise AssertionError("Decoder must not run without identity")
        jobs = [PollJob(e, "rpm", 0x1234, 1, 2, forbidden) for e in ("bad", "good")]
        scheduler = Scheduler({"bad":bad, "good":good}, store, jobs, cooldown=.2, clock=self.clock)
        scheduler.step(); scheduler.step()
        self.assertEqual(scheduler.stats["errors"], 1)
        self.assertEqual(scheduler.stats["success"], 1)
        self.assertEqual(store.snapshot()[-1]["quality"], "identity_unverified")
        self.assertFalse(scheduler.step())
    def test_permanent_nrc_disables_job(self):
        wire = Wire([bytes.fromhex("5001003201f4"), bytes.fromhex("7f2231")])
        job = PollJob("e", "s", 0x1234, 1, 2, lambda raw: {})
        scheduler = Scheduler({"e":self.session(wire)}, SignalStore(self.clock), [job], cooldown=.2, clock=self.clock)
        scheduler.step()
        self.assertFalse(job.enabled)
    def test_on_demand(self):
        job = PollJob("e", "s", 0x1234, None, 2, lambda raw: {}, "ON_DEMAND")
        wire = Wire([bytes.fromhex("5001003201f4"), bytes.fromhex("62123455")])
        scheduler = Scheduler({"e":self.session(wire)}, SignalStore(self.clock), [job], cooldown=.2, clock=self.clock)
        self.assertFalse(scheduler.step())
        scheduler.request("s", "e")
        self.assertTrue(scheduler.step())
        self.assertFalse(scheduler.step())
    def test_passive_decode_and_invalid(self):
        store = SignalStore(self.clock)
        field = Field("fixture", "temperature", 0x123, False, 2, 0, 2, "degC",
                      scale=.5, signed=True, invalid_raw=("7fff",), evidence="synthetic test")
        decoder = PassiveDecoder([field], store, profile_verified=True)
        self.assertEqual(decoder.feed(0x123, bytes.fromhex("fffe")), 1)
        self.assertEqual(store.snapshot()[0]["value"], -1)
        decoder.feed(0x123, b"\x7f\xff")
        self.assertEqual(store.snapshot()[0]["quality"], "invalid")
        decoder.feed(0x123, b"\x00")
        self.assertEqual(store.snapshot()[0]["quality"], "invalid_length")
        self.assertEqual(decoder.feed(0x123, b"\x00\x01", extended=True), 0)
        self.assertEqual(decoder.feed(0x123, b"\x00\x01", error=True), 0)
    def test_passive_identity_gating(self):
        store = SignalStore(self.clock)
        decoder = PassiveDecoder([Field("e", "s", 1, False, 1, 0, 1, "x", evidence="fixture")], store)
        decoder.feed(1, b"\x05")
        self.assertIsNone(store.snapshot()[0]["value"])


if __name__ == "__main__":
    unittest.main()
