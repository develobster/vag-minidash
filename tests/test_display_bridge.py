import threading
import time
import unittest

from minidash_live.display_bridge import (
    DisplaySample,
    SerialDisplayBridge,
    encode_sample,
    encode_snapshot,
    project_snapshot,
)


class FakeSerial:
    def __init__(self):
        self.frames = []
        self.closed = False
        self.lock = threading.Lock()

    def write(self, frame):
        with self.lock:
            self.frames.append(frame)
        return len(frame)

    def close(self):
        self.closed = True


class BlockingSerial(FakeSerial):
    def __init__(self):
        super().__init__()
        self.first_write_started = threading.Event()
        self.release_first_write = threading.Event()

    def write(self, frame):
        with self.lock:
            self.frames.append(frame)
            first = len(self.frames) == 1
        if first:
            self.first_write_started.set()
            self.release_first_write.wait(1)
        return len(frame)


class DisplayBridgeTests(unittest.TestCase):
    def test_projects_only_arbitrated_valid_values(self):
        snapshot = {
            "decisions": {
                "engine.rpm": {"selected": {"quality": "valid", "value": 2450,
                                               "ttl": 1.5}, "ambiguous": False},
                "vehicle.speed": {"selected": None, "ambiguous": False},
                "engine.load": {"selected": {"quality": "candidate", "value": 42},
                                "ambiguous": False},
            },
            "signals": [
                {"signal": "engine.rpm", "quality": "valid", "ttl": 1.5},
                {"signal": "vehicle.speed", "quality": "stale", "ttl": 2.0},
                {"signal": "engine.load", "quality": "candidate", "ttl": 1.0},
            ],
        }
        samples = project_snapshot(snapshot, ("engine.rpm", "vehicle.speed", "engine.load"))
        self.assertEqual([(s.canonical_signal, s.state, s.ttl_ms, s.value) for s in samples], [
            ("engine.rpm", "VALID", 1500, 2450.0),
            ("vehicle.speed", "STALE", 2000, None),
            ("engine.load", "UNAVAILABLE", 1000, None),
        ])

    def test_errors_ambiguous_and_unsupported_do_not_publish_values(self):
        snapshot = {
            "decisions": {"engine.rpm": {"selected": None, "ambiguous": True}},
            "signals": [{"signal": "engine.rpm", "quality": "valid", "ttl": 1.0}],
        }
        sample, = project_snapshot(snapshot, ("engine.rpm",))
        self.assertEqual((sample.state, sample.value), ("ERROR", None))

        snapshot["decisions"]["engine.rpm"]["ambiguous"] = False
        snapshot["signals"] = [{"signal": "engine.rpm", "quality": "unsupported", "ttl": 1.0}]
        sample, = project_snapshot(snapshot, ("engine.rpm",))
        self.assertEqual((sample.state, sample.value), ("UNSUPPORTED", None))

    def test_frames_are_versioned_bounded_and_never_encode_missing_as_zero(self):
        valid = encode_sample(DisplaySample("engine.rpm", "VALID", 1000, 2450))
        missing = encode_sample(DisplaySample("engine.rpm", "UNAVAILABLE", 1000))
        self.assertEqual(valid, b"MDS1|engine.rpm|VALID|1000|2450\n")
        self.assertEqual(missing, b"MDS1|engine.rpm|UNAVAILABLE|1000|-\n")
        with self.assertRaises(ValueError):
            encode_sample(DisplaySample("engine.rpm", "UNAVAILABLE", 1000, 0))
        with self.assertRaises(ValueError):
            encode_sample(DisplaySample("engine.unknown", "VALID", 1000, 1))
        with self.assertRaises(ValueError):
            encode_sample(DisplaySample("engine.rpm", "VALID", 1000, float("nan")))

    def test_latest_snapshot_writer_is_independent_and_closes(self):
        port = FakeSerial()
        bridge = SerialDisplayBridge(port)
        snapshot = {"decisions": {"engine.rpm": {
                        "selected": {"quality": "valid", "value": 10, "ttl": 1},
                        "ambiguous": False}},
                    "signals": []}
        self.assertTrue(bridge.publish(snapshot, ("engine.rpm",)))
        deadline = time.monotonic() + 1
        while not port.frames and time.monotonic() < deadline:
            time.sleep(.005)
        bridge.close()
        self.assertEqual(port.frames, [b"MDS1|engine.rpm|VALID|1000|10\n"])
        self.assertTrue(port.closed)

    def test_slow_serial_drops_old_batch_without_blocking_producer(self):
        port = BlockingSerial()
        bridge = SerialDisplayBridge(port)

        def snapshot(value):
            return {"decisions": {"engine.rpm": {
                        "selected": {"quality": "valid", "value": value, "ttl": 1},
                        "ambiguous": False}},
                    "signals": []}

        self.assertTrue(bridge.publish(snapshot(1), ("engine.rpm",)))
        self.assertTrue(port.first_write_started.wait(1))
        self.assertTrue(bridge.publish(snapshot(2), ("engine.rpm",)))
        start = time.monotonic()
        self.assertTrue(bridge.publish(snapshot(3), ("engine.rpm",)))
        self.assertLess(time.monotonic() - start, .05)
        self.assertEqual(bridge.dropped_batches, 1)
        port.release_first_write.set()
        bridge.close()
        self.assertEqual(port.frames, [
            b"MDS1|engine.rpm|VALID|1000|1\n",
            b"MDS1|engine.rpm|VALID|1000|3\n",
        ])


if __name__ == "__main__":
    unittest.main()
