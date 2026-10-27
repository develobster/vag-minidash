"""Cross-language golden vectors and deterministic differential decoding."""
import ctypes
import random
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from minidash_live.generated_can_signals import SIGNALS, decode_frame, unpack_scalar

ROOT = Path(__file__).resolve().parents[1]


class GeneratedCanTests(unittest.TestCase):
    def test_generated_files_are_current(self):
        subprocess.run([sys.executable, "scripts/generate_can_signals.py", "--check"],
                       cwd=ROOT, check=True)

    def test_golden_vectors_and_candidate_gate(self):
        for platform, can_id, data in (
            ("MQB", 168, bytes.fromhex("0000000000000032")),
            ("PQ35/PQ46", 640, bytes.fromhex("0000003200000000")),
        ):
            decoded = decode_frame(platform, can_id, data)
            self.assertEqual(len(decoded), 1)
            self.assertEqual(decoded[0]["value"], 3200)
            self.assertEqual(decoded[0]["quality"], "candidate")
            for options in ({"extended": True}, {"remote": True}, {"error": True}):
                self.assertEqual(decode_frame(platform, can_id, data, **options), [])
            self.assertEqual(decode_frame(platform, can_id, data[:-1]), [])
        self.assertEqual(decode_frame("UNKNOWN", 168, bytes(8)), [])

    def test_c_python_differential(self):
        cc = shutil.which("clang") or shutil.which("cc")
        if not cc:
            self.skipTest("C compiler is required for parity testing")
        with tempfile.TemporaryDirectory() as tmp:
            library = Path(tmp) / "can.so"
            subprocess.run([cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-shared", "-fPIC",
                            "-Ifirmware/main", "firmware/main/generated_can_signals.c",
                            "firmware/main/ui_core.c", "firmware/main/ui_i18n.c", "-lm",
                            "-o", str(library)], cwd=ROOT, check=True)
            unpack = ctypes.CDLL(str(library)).md_can_unpack_scalar
            unpack.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t,
                               ctypes.c_uint, ctypes.c_uint, ctypes.c_bool, ctypes.c_bool,
                               ctypes.c_double, ctypes.c_double, ctypes.POINTER(ctypes.c_double)]
            unpack.restype = ctypes.c_bool
            rng = random.Random(20261027)
            layouts = [(s["start_bit"], s["bits"], s["little"], s["signed"],
                        s["scale"], s["offset"]) for s in SIGNALS]
            layouts += [(0, 32, True, True, 1., 0.), (7, 1, True, True, 1., 0.),
                        (32, 32, False, True, .01, -10.), (63, 1, True, False, 1., 0.)]
            for layout in layouts:
                for _ in range(1000):
                    data = bytes(rng.getrandbits(8) for _ in range(8))
                    raw = (ctypes.c_uint8 * 8).from_buffer_copy(data)
                    actual = ctypes.c_double()
                    self.assertTrue(unpack(raw, 8, *layout, ctypes.byref(actual)))
                    self.assertAlmostEqual(actual.value, unpack_scalar(data, *layout), places=8)
            for start, bits in ((0, 0), (0, 33), (63, 2), (64, 1), (2**32-1, 1)):
                actual = ctypes.c_double()
                self.assertFalse(unpack(raw, 8, start, bits, True, False, 1, 0, ctypes.byref(actual)))


if __name__ == "__main__":
    unittest.main()
