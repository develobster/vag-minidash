import unittest
import tempfile
from pathlib import Path
from core import Simulator, Coding, Recorder

class CoreTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.sim = Simulator()
        self.path = Path(self.tmp.name)/'journal.jsonl'
        self.coding = Coding(self.sim, self.path)
    def test_write_and_restore(self):
        undo = self.coding.apply(self.coding.prepare('demo_door_light', True))
        self.assertTrue(self.sim.read('demo_door_light'))
        self.coding.apply(undo)
        self.assertFalse(self.sim.read('demo_door_light'))
        self.assertEqual(len(self.path.read_text().splitlines()), 4)
    def test_motion_blocks_write(self):
        self.sim.speed = 1
        with self.assertRaises(ValueError):
            self.coding.apply(self.coding.prepare('demo_door_light', True))
        self.assertFalse(self.sim.read('demo_door_light'))
    def test_voltage_blocks_write(self):
        self.sim.voltage = 11
        with self.assertRaises(ValueError):
            self.coding.apply(self.coding.prepare('demo_door_light', True))
    def test_stale_preview(self):
        change = self.coding.prepare('demo_door_light', True)
        self.sim.values['demo_door_light'] = True
        with self.assertRaises(ValueError):
            self.coding.apply(change)
    def test_journal_failure_prevents_write(self):
        self.path.mkdir()
        with self.assertRaises(OSError):
            self.coding.apply(self.coding.prepare('demo_door_light', True))
        self.assertFalse(self.sim.read('demo_door_light'))
    def test_failed_readback(self):
        self.sim.write = lambda key, value: None
        with self.assertRaises(RuntimeError):
            self.coding.apply(self.coding.prepare('demo_door_light', True))
        self.assertNotIn('verified', self.path.read_text())
    def test_blackbox(self):
        r = Recorder(capacity=3, post_samples=2)
        for g in [3, 2, 2, 1, 1, 1]:
            r.feed({'gear':g})
        self.assertEqual(len(r.events), 1)
        self.assertEqual([s['gear'] for s in r.events[0]['samples']], [2,2,1,1,1])

if __name__ == '__main__':
    unittest.main()
