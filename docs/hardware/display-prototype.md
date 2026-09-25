# ESP32-S3 display prototype (Rev.A software)

## Scope and boundary

The existing Python host runtime owns transport, ISO-TP, UDS, discovery, identity,
knowledge providers, acquisition planning, `SignalStore`, source arbitration and CLI.
Python does not run inside this ESP32-S3 firmware. The existing ESP-IDF project now
contains a small display demo: embedded canonical signal samples → UI projection →
display abstraction → ST7789 SPI backend. Its values are **synthetic**, never vehicle
measurements. The host offline reference in `minidash_live/display.py` publishes via
the real Python `SignalStore` and uses `SourceArbitrator`; it checks the shared
canonical names, freshness and page contract. A future embedded acquisition adapter
will replace `md_demo_publish` after CAN/decoder parity is proven. Do not feed raw
CAN bytes or unverified DIDs directly into the UI.
The firmware marks both pages `DEMO` while synthetic values are shown.

## Ordered hardware and open decisions

| Item | Current status |
| --- | --- |
| ESP32-S3 | Existing board; 16 MB flash, 8 MB PSRAM; ESP-IDF 5.5.5 bring-up project exists. |
| Display | Ordered 2.4-inch ST7789, advertised 320×240, SPI. Touch is advertised but its controller and wiring are **unverified**. Touch stays disabled. |
| CAN transceiver | SN65HVD230 module ordered. Check whether it carries a 120 Ω termination resistor; disconnect that resistor before joining an already terminated automotive CAN bus. Verify the actual board circuit first. |
| Power | LM2596S fixed 5 V, advertised 8–40 V input. Automotive transient/TVS, reverse-polarity protection, sleep current and final power design remain open. |
| Vehicle lead | OBD-II extension cable ordered; pinout must be continuity-checked before cutting or connecting. |

## Software configuration

`firmware/main/Kconfig.projbuild` keeps the display disabled by default. All SPI
GPIOs default to `-1`. After identifying the **actual** display PCB, configure
`MINIDASH_DISPLAY_ENABLE`, SCLK, MOSI, CS and DC in `idf.py menuconfig`; RESET and
backlight GPIO are optional. Confirm backlight polarity and any level shifting.
There are deliberately no assumed GPIO assignments in the repository. ST7789
rotation 0/2 gives 320×240; 1/3 gives 240×320. The profile records RGB565,
rotation, unknown touch and backlight capability. The UI takes a generic backend;
ST7789 RAM offsets and color inversion are configurable because modules differ;
confirm them with the actual display before bench use.
SSD1306, SH1106, GC9A01 and ILI9341 are reserved profile/driver IDs but are not
implemented or selectable yet. A future driver can implement `md_display_backend_t`
without changing signal projection or navigation.

The current two pages show RPM/speed/boost/battery and oil/coolant/RPM. Boost is
computed only when both `engine.charge_pressure_actual` and
`engine.ambient_pressure` are fresh, in hPa. It is their difference divided by
1000, shown in bar. Missing or stale values show `--`, not zero. The provisional
`vehicle.battery_voltage` name is used only by the synthetic demo until a provider
and measurement point are defined. Console `next`, `prev`, `sleep`, `wake` commands
use the same InputManager actions intended for buttons, verified touch or steering
controls. Sleep commands turn off the panel and, when a confirmed backlight switch
GPIO exists, set that GPIO to its configured off level. If the backlight is tied
directly to power, software cannot physically switch it off; measure and revise the
circuit before claiming a low-power sleep state.

## Wiring template (no GPIO assignments)

| Source | Destination | Confirm first |
| --- | --- | --- |
| ESP32-S3 3.3 V / GND | Display logic power / GND | Module voltage, regulator and logic levels from its actual PCB. |
| ESP32-S3 SPI SCLK / MOSI | ST7789 SCLK / DIN | Exact pin labels and configured ESP32 GPIOs. |
| ESP32-S3 GPIO | ST7789 CS / DC / optional RESET | Module pinout, shared bus behavior and reset polarity. |
| ESP32-S3 GPIO or switched rail | Backlight input/supply | Backlight voltage, current, polarity and whether it can be physically shut off. |
| Unassigned | Advertised touch interface | Identify controller, bus, voltage and interrupt wiring after arrival. |
| ESP32-S3 TWAI TX/RX | SN65HVD230 TXD/RXD | Actual module pinout and 3.3 V compatibility; not initialized in this prototype. |
| OBD pin 6 / 14 | CAN-H / CAN-L | Continuity, vehicle bus location and termination; first vehicle test must be listen-only. |
| OBD pin 16 / 4,5 | Fused power input / common ground | Verify cable pins; LM2596 5 V output with multimeter before connecting electronics. |

Do not connect the display or vehicle using this template alone. First inspect the
delivered PCB labels and schematic, meter the cable and DC/DC output, and decide
which ESP32-S3 pins are free from boot, USB and PSRAM constraints. No display build
has been flashed by this change.

## Offline and bench checks

1. Run `python -m unittest discover -s tests -v` to check the host runtime and
   display reference, including profiles, rotation, page switching, freshness and
   driver-independent UI rendering.
2. Run `python -m minidash_live.runtime_demo` for the existing synthetic
   CAN/UDS smoke test. With ESP-IDF available, run `idf.py build` in `firmware/`.
3. On arrival, photograph/read both sides of the display and identify ST7789,
   touch controller (if any), power/backlight arrangement and pin labels. Enter
   only confirmed GPIO values, build, then test pages over USB power on the bench.
4. Measure backlight off/on current and verify `sleep` truly extinguishes it.
5. Inspect the SN65HVD230 module for 120 Ω; remove/disable extra termination
   before connecting to the vehicle. Validate OBD cable continuity, fused supply,
   common ground and LM2596 output independently.
6. Start vehicle CAN in listen-only mode. Capture evidence for actual signals
   and their source quality before connecting embedded acquisition to the UI.
