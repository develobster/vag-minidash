import ctypes
import json
from pathlib import Path
import random
import shutil
import subprocess
import sys
import tempfile
import unittest

from minidash_live.vag_can_data import DEFINITIONS, SIGNAL_NAMES
from minidash_live.vag_can_db import VagCanDatabase, unpack
from minidash_live.vag_replay import NativeReplay, read_log, replay

ROOT = Path(__file__).resolve().parents[1]


class VagDbTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cc = shutil.which("clang") or shutil.which("cc")
        if not cc:
            raise unittest.SkipTest("Native parity requires C compiler")
        cls.tmp = tempfile.TemporaryDirectory()
        cls.lib = Path(cls.tmp.name) / "vag.so"
        sources = ["core/vag_native_api.c", *[f"firmware/main/{name}.c" for name in
                   ("vag_can_db", "drag_timer", "performance_timer", "ui_core", "ui_i18n")]]
        subprocess.run([cc, "-std=c11", "-shared", "-fPIC", "-O1", "-Ifirmware/main", *sources,
                        "-lm", "-o", str(cls.lib)], cwd=ROOT, check=True)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_generated_catalogues(self):
        for generator in ("generate_vag_can_db.py", "generate_can_signals.py"):
            subprocess.run([sys.executable, "scripts/" + generator, "--check"], cwd=ROOT, check=True)

    def test_c_python_differential(self):
        rng = random.Random(27102026)
        qualities = ("unavailable", "candidate", "valid", "stale", "error")
        for platform in ("MQB", "PQ35_46"):
            for layout in ("sourced", "requested_experimental"):
                db = VagCanDatabase(platform, layout)
                native = NativeReplay(self.lib, platform, layout)
                try:
                    definitions = [d for d in DEFINITIONS if (d["platform"], d["layout"]) == (platform, layout)]
                    for stamp in range(1000):
                        definition = rng.choice(definitions)
                        data = bytes(rng.getrandbits(8) for _ in range(8 if stamp % 10 else rng.randrange(8)))
                        row = dict(id=definition["can_id"], data=data.hex(), extended=False, remote=stamp % 31 == 0, error=False)
                        db.feed(row["id"], data, stamp, remote=row["remote"])
                        native.feed(row, stamp)
                        snapshot = db.snapshot(stamp)["telemetry"]
                        for i, signal in enumerate(SIGNAL_NAMES):
                            value = ctypes.c_double()
                            quality = native.lib.vag_native_signal(native.handle, i, stamp, ctypes.byref(value))
                            self.assertEqual(qualities[quality], snapshot[signal]["quality"], (platform, layout, signal, row))
                            if quality in (1, 2):
                                self.assertAlmostEqual(value.value, snapshot[signal]["value"], places=8)
                finally:
                    native.close()

    def test_independent_golden_and_conflicts(self):
        self.assertEqual(unpack(bytes.fromhex("abc0"), 7, 12, False), (0xabc, 2748))
        self.assertEqual(unpack(bytes.fromhex("abc0"), 7, 12, False, True), (0xabc, -1348))
        db = VagCanDatabase()
        db.feed(0xfd, bytes.fromhex("0000000070170000"), 10)
        self.assertEqual(db.snapshot(10)["telemetry"]["speed_kph"]["value"], 60)
        self.assertEqual(db.snapshot(10)["telemetry"]["rpm"]["quality"], "unavailable")
        self.assertEqual(db.snapshot(510)["telemetry"]["speed_kph"]["quality"], "stale")
        self.assertEqual(db.feed(0x126, bytes(8), 11), 0)
        experimental = VagCanDatabase("MQB", "experimental")
        self.assertEqual(experimental.feed(0x126, bytes.fromhex("00b48c0000000000"), 0), 2)
        self.assertEqual(experimental.snapshot(0)["telemetry"]["coolant_c"]["value"], 87)
        self.assertEqual(experimental.snapshot(0)["telemetry"]["coolant_c"]["quality"], "candidate")

    def test_candump_and_native_drag_estimate(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "run.log"
            rows = []
            for stamp in range(0, 11601, 100):
                speed = 0 if stamp <= 1000 else min(100, (stamp-1000)/100)
                payload = bytearray(8)
                payload[4:6] = int(speed*100).to_bytes(2, "little")
                rows.append(f"({stamp/1000:.3f}) can0 0FD#{payload.hex()}\n")
            path.write_text("".join(rows))
            decoded = list(replay(path, native_lib=self.lib))
            self.assertFalse(decoded[-1]["vehicle_validation"])
            self.assertNotIn("drag", decoded[-1])
            estimated = list(replay(path, native_lib=self.lib, estimate_drag=True))[-1]
            self.assertEqual(estimated["drag"]["state"], "FINISHED")
            self.assertAlmostEqual(estimated["drag"]["seconds"][1], 10, places=3)
            self.assertTrue(estimated["drag"]["offline_estimate"])
            self.assertEqual(estimated["telemetry"]["speed_kph"]["quality"], "candidate")
            with self.assertRaises(ValueError):
                list(replay(path, estimate_drag=True))
            path.write_text("(1.0) can0 0FD#0000\n(0.5) can0 0FD#0000\n")
            with self.assertRaises(ValueError):
                list(read_log(path))
            path.write_text("(0.0) can0 0FD#123\n")
            with self.assertRaises(ValueError):
                list(read_log(path))
            path.write_text("can0 0FD [8] 00 00 00 00 00 00 00 00\n")
            with self.assertRaises(ValueError):
                list(read_log(path))


if __name__ == "__main__":
    unittest.main()
