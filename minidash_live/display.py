"""Offline reference for the embedded display contract; vehicle data stays in SignalStore.

The ESP-IDF implementation lives in firmware/main. This module lets the host test
profile selection, navigation and canonical signal projection without panel GPIOs.
"""
from dataclasses import dataclass
from enum import Enum

from .arbitration import AmbiguousSourceError, SourceArbitrator
from .runtime import SignalStore


class Driver(str, Enum):
    ST7789 = "st7789"
    SSD1306 = "ssd1306"
    SH1106 = "sh1106"
    GC9A01 = "gc9a01"
    ILI9341 = "ili9341"


class Touch(str, Enum):
    UNKNOWN = "unknown"
    ABSENT = "absent"
    PRESENT = "present"


@dataclass(frozen=True)
class DisplayProfile:
    driver: Driver
    width: int
    height: int
    rotation: int
    color_depth: int
    touch: Touch
    backlight: bool


def select_profile(driver=Driver.ST7789, *, rotation=0):
    """Only the ordered ST7789 is supported; enum reserves future drivers."""
    if driver != Driver.ST7789 or rotation not in range(4):
        raise ValueError("Unsupported display profile or rotation")
    return DisplayProfile(driver, 240 if rotation % 2 else 320,
                          320 if rotation % 2 else 240, rotation, 16,
                          Touch.UNKNOWN, True)


class InputManager:
    """All input producers send navigation actions through one small boundary."""
    SOURCES = {"touch", "button", "can", "console"}
    def __init__(self):
        self.page = 0
        self.sleeping = False

    def dispatch(self, source, action):
        if source not in self.SOURCES:
            raise ValueError("Unknown input source")
        if action == "sleep":
            self.sleeping = True
        elif action == "wake":
            self.sleeping = False
        elif action == "next" and not self.sleeping:
            self.page = (self.page + 1) % 2
        elif action == "previous" and not self.sleeping:
            self.page = (self.page - 1) % 2
        elif action not in {"next", "previous"}:
            raise ValueError("Unknown navigation action")


SIGNALS = ("engine.rpm", "vehicle.speed", "engine.oil_temperature",
           "engine.coolant_temperature", "engine.charge_pressure_actual",
           "engine.ambient_pressure", "vehicle.battery_voltage")


def project_ui(store, input_manager, arbitrator=None):
    arbitrator = arbitrator or SourceArbitrator()
    values = {}
    for name in SIGNALS:
        try:
            row = store.best(name, arbitrator)
        except AmbiguousSourceError:
            row = None
        values[name] = (row["value"] if row and row["quality"] == "valid" else None)
    actual, ambient = (values["engine.charge_pressure_actual"],
                       values["engine.ambient_pressure"])
    boost = (actual - ambient) / 1000 if actual is not None and ambient is not None else None
    return {"page": input_manager.page, "sleeping": input_manager.sleeping,
            "values": values, "boost_bar": boost}


def publish_demo(store, *, ttl=3):
    """Synthetic samples follow exactly the same publish path as CAN/UDS samples."""
    samples = (("engine", "engine.rpm", 2450, "rpm"),
               ("abs", "vehicle.speed", 73, "km/h"),
               ("engine", "engine.oil_temperature", 96, "degC"),
               ("engine", "engine.coolant_temperature", 91, "degC"),
               ("engine", "engine.charge_pressure_actual", 1713, "hPa"),
               ("engine", "engine.ambient_pressure", 993, "hPa"),
               ("vehicle", "vehicle.battery_voltage", 14.3, "V"))
    for ecu, name, value, unit in samples:
        store.publish(ecu, name, "demo", value, ttl, "valid", unit,
                      definition_id=f"demo.{name}", confidence="confirmed")


class DisplayManager:
    """Backend receives drawing commands; page layout never imports a panel driver."""
    def __init__(self, profile, backend):
        self.profile = profile
        self.backend = backend

    def render(self, model):
        self.backend.power(not model["sleeping"])
        if model["sleeping"]:
            return
        v = model["values"]
        if model["page"] == 0:
            fields = (("RPM", v["engine.rpm"], "rpm"),
                      ("SPEED", v["vehicle.speed"], "km/h"),
                      ("BOOST", model["boost_bar"], "bar"),
                      ("VOLT", v["vehicle.battery_voltage"], "V"))
        else:
            fields = (("OIL", v["engine.oil_temperature"], "degC"),
                      ("COOLANT", v["engine.coolant_temperature"], "degC"),
                      ("RPM", v["engine.rpm"], "rpm"))
        positions = ((8, 8), (170 if self.profile.width >= 320 else 8,
                              8 if self.profile.width >= 320 else 60),
                     (8, 92), (170 if self.profile.width >= 320 else 8, 92))
        self.backend.present({"size": (self.profile.width, self.profile.height),
                              "mode": "demo",
                              "page": model["page"] + 1,
                              "fields": tuple((x, y, *field)
                                              for (x, y), field in zip(positions, fields))})


def offline_demo():
    store = SignalStore()
    publish_demo(store)
    return project_ui(store, InputManager())
