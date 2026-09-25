import unittest

from minidash_live.display import (
    DisplayManager, Driver, InputManager, Touch, offline_demo,
    project_ui, publish_demo, select_profile,
)
from minidash_live.runtime import SignalStore


class FakePanel:
    def __init__(self):
        self.frames = []
        self.powered = []

    def present(self, frame):
        self.frames.append(frame)

    def power(self, enabled):
        self.powered.append(enabled)


class DisplayTests(unittest.TestCase):
    def test_profile_and_rotation(self):
        normal = select_profile()
        self.assertEqual((normal.width, normal.height, normal.color_depth), (320, 240, 16))
        self.assertEqual(normal.touch, Touch.UNKNOWN)
        self.assertTrue(normal.backlight)
        self.assertEqual((select_profile(rotation=1).width,
                          select_profile(rotation=1).height), (240, 320))
        self.assertEqual(select_profile(rotation=2).width, 320)
        with self.assertRaises(ValueError):
            select_profile(rotation=4)
        with self.assertRaises(ValueError):
            select_profile(Driver.ILI9341)

    def test_navigation_and_sleep(self):
        inputs = InputManager()
        inputs.dispatch("button", "next")
        self.assertEqual(inputs.page, 1)
        inputs.dispatch("can", "next")
        self.assertEqual(inputs.page, 0)
        inputs.dispatch("touch", "previous")
        self.assertEqual(inputs.page, 1)
        inputs.dispatch("console", "sleep")
        inputs.dispatch("button", "next")
        self.assertEqual(inputs.page, 1)
        inputs.dispatch("console", "wake")
        self.assertFalse(inputs.sleeping)

    def test_demo_uses_canonical_store_and_expires(self):
        now = [0.0]
        store = SignalStore(clock=lambda: now[0])
        publish_demo(store, ttl=3)
        model = project_ui(store, InputManager())
        self.assertEqual(model["values"]["engine.rpm"], 2450)
        self.assertAlmostEqual(model["boost_bar"], .72)
        self.assertEqual({row["signal"] for row in store.snapshot()}, set(model["values"]))
        now[0] = 3.0
        stale = project_ui(store, InputManager())
        self.assertIsNone(stale["values"]["engine.rpm"])
        self.assertIsNone(stale["boost_bar"])

    def test_ui_is_driver_independent(self):
        panel = FakePanel()
        manager = DisplayManager(select_profile(), panel)
        inputs = InputManager()
        model = offline_demo()
        manager.render(model)
        self.assertEqual(panel.frames[-1]["size"], (320, 240))
        self.assertEqual([row[2] for row in panel.frames[-1]["fields"]],
                         ["RPM", "SPEED", "BOOST", "VOLT"])
        inputs.dispatch("button", "next")
        store = SignalStore()
        publish_demo(store)
        manager.render(project_ui(store, inputs))
        self.assertEqual(panel.frames[-1]["page"], 2)
        self.assertEqual([row[2] for row in panel.frames[-1]["fields"]],
                         ["OIL", "COOLANT", "RPM"])
        inputs.dispatch("button", "sleep")
        manager.render(project_ui(store, inputs))
        self.assertFalse(panel.powered[-1])
        self.assertEqual(len(panel.frames), 2)


if __name__ == "__main__":
    unittest.main()
