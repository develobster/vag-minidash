# ESP32-S3 display prototype (Rev.A software)

## Scope and boundary

The existing Python host runtime owns transport, ISO-TP, UDS, discovery, identity,
knowledge providers, acquisition planning, `SignalStore`, source arbitration and CLI.
Python does not run inside this ESP32-S3 firmware. The production firmware UI accepts
canonical signal samples through a small publishing API and does not implement CAN,
UDS, ECU discovery, or vehicle-specific decoding. Normal boot starts with unavailable
values until a trusted provider publishes them; there is no synthetic signal producer
in the normal path. Earlier DEMO screens and the host offline reference belong to the
preceding prototype. See [the UI architecture](../ui/architecture.md) and
[settings guide](../ui/user-settings.md) for the current subsystem contract.

The production UI has now been built and flashed to the display board. Current UI
verification and open work are recorded in [the UI status note](../ui/status.md).
The bench results below retain the history of the earlier display prototype and
hardware setup; they should not be read as the latest UI verification state.

For the panel smoke test, the initial ILI9341 trial did not produce a correct
full-panel image. The current successful smoke image selects ESP-IDF v5.5.5's
built-in ST7789 backend and runs red, green, blue and black fills before showing
`MINIDASH` / `DISPLAY OK`. The user confirmed the latest orientation correction is
now okay. This verifies the ST7789 software backend and pin mapping with this
module; it does not independently identify the controller silicon. The TFT SDO pin remains unconnected; MISO GPIO13 is used by the XPT2046 touch
controller. The TFT controller marking has not been inspected. Do not feed raw CAN bytes or unverified DIDs directly into the UI.

## Ordered hardware and open decisions

| Item | Current status |
| --- | --- |
| ESP32-S3 | Existing board; 16 MB flash, 8 MB PSRAM; ESP-IDF 5.5.5 bring-up project exists. |
| Display | Board silkscreen: 2.4-inch TFT SPI 240x320 V1.3. The ESP-IDF v5.5.5 ST7789 backend produces the verified image in 320x240 landscape; LCD controller silicon marking/ID remains unconfirmed. Touch IC U1 is marked XPT2046 and its raw touch path was verified over SPI. Backlight LED is tied to 3.3 V. |
| CAN transceiver | SN65HVD230 module ordered. Check whether it carries a 120 Ω termination resistor; disconnect that resistor before joining an already terminated automotive CAN bus. Verify the actual board circuit first. |
| Power | LM2596S fixed 5 V, advertised 8–40 V input. Automotive transient/TVS, reverse-polarity protection, sleep current and final power design remain open. |
| Vehicle lead | OBD-II extension cable ordered; pinout must be continuity-checked before cutting or connecting. |

## Software configuration

`firmware/main/Kconfig.projbuild` and `sdkconfig.defaults` select the ST7789
backend and LVGL UI; the RGB/black boot smoke option is disabled after its
successful initial test. The UI contains dashboard, performance, temperatures,
drivetrain, diagnostics and settings views. The working profile is 320x240 landscape,
logical rotation 2, with the board-specific MADCTL override `swap_xy=true`,
`MX=false`, `MY=true`; the user confirmed this orientation is readable. Display
pins are SCLK=GPIO12, MOSI=GPIO11, CS=GPIO10, DC=GPIO8 and RESET=GPIO9. The TFT
LED/backlight pin is tied directly to 3.3 V, so software does not control it.
LVGL and its ESP-IDF port are pinned in `firmware/main/idf_component.yml`. The
custom partition table reserves 3 MiB for the application and a dedicated NVS
partition for UI settings; use the same table when programming a device.

The touch controller U1 is marked XPT2046. It shares SCLK=GPIO12 and MOSI=GPIO11
with the panel, returns data on MISO=GPIO13, and uses its own CS=GPIO14; TFT CS
remains GPIO10. T_IRQ is unconnected; firmware polls the controller at 1 MHz.
A detected touch maps full-screen tap, double-tap and hold gestures to semantic
navigation/settings actions. Gestures do not depend on raw coordinate
calibration; location-based controls still require calibration. The component
manager also pins `espressif/esp_lcd_ili9341` to
v2.0.2 for the unsuccessful alternate panel-driver trial. Both panel backends
implement `md_display_backend_t`, keeping signal projection independent of the
panel driver. RAM offset and color inversion remain configurable.

Built-in dashboards and detail views display canonical samples supplied through
the firmware UI API. `engine.boost` must be published by a provider; the UI does
not derive it from charge and ambient pressure. Unsupported, unavailable, stale
and invalid values are represented separately and never replaced with zero. The
`vehicle.battery_voltage` name remains provisional until a provider and measurement
point are defined. Console navigation and settings commands use semantic input
actions shared with touch and future physical controls. Sleep turns off the panel;
the current directly powered backlight remains on. The diagnostics view reports
brightness as fixed on this wiring.

## Bench wiring

| ESP32-S3 / supply | TFT pin | Status |
| --- | --- | --- |
| GPIO12 | SCK and T_CLK | Shared SPI clock; connected |
| GPIO11 | SDI(MOSI) and T_DIN | Shared SPI MOSI; connected |
| GPIO13 | T_DO | XPT2046 MISO; connected. TFT SDO(MISO) stays unconnected |
| GPIO14 | T_CS | XPT2046 CS; separate from display CS |
| Unconnected | T_IRQ | Optional; polling used |
| GPIO10 | CS | Display CS; connected |
| GPIO8 | DC | Connected |
| GPIO9 | RESET | Connected |
| 3.3 V / GND | VCC / GND | Connected |
| 3.3 V | LED | Connected directly; backlight always on |

No vehicle or CAN wiring is part of this display smoke test. The SN65HVD230,
vehicle lead and automotive supply checks remain separate future work.

## Bench smoke result (2026-09-28)

- ESP-IDF v5.5.5 in `C:\Espressif` is installed and usable; the ESP32-S3 firmware
  built successfully and was flashed to COM5.
- The first candidate, Espressif `esp_lcd_ili9341` v2.0.2, initialized and passed
  serial checks, but the supplied photos showed a noisy vertical band and incorrect
  area/orientation. It is retained as an alternate backend, not the selected one.
- The successful on-device software driver is ESP-IDF v5.5.5's built-in ST7789
  backend (`esp_lcd_panel_st7789`). Its working profile is 320×240 landscape,
  rotation 2, `swap_xy=true`, `MX=false`, `MY=true`. The user confirmed the latest
  image is now okay after earlier rotations/mirror settings produced reversed or
  upside-down text. The tested wiring is SCK=GPIO12, MOSI=GPIO11, CS=GPIO10,
  DC=GPIO8 and RESET=GPIO9.
- Serial output confirmed display initialization, red/green/blue/black patterns,
  and submission of the final `MINIDASH / DISPLAY OK` frame. These serial PASS
  markers plus the user's visual confirmation verify the software driver/profile
  with this module.
- The TFT controller silicon model remains unconfirmed: TFT SDO is unconnected,
  no TFT ID readback occurred, and the LCD controller marking was not inspected.
  Record the tested driver as the ST7789 ESP-IDF backend, but do not claim the
  controller silicon itself was positively identified. The XPT2046 touch IC is
  independently marked on the module and was exercised over MISO GPIO13.
- Flashing used the app-only command and wrote only the factory app at `0x10000`;
  the 16 MiB factory backup SHA-256 remained
  `CD0F30AC9491DA499EAF2167F07B5ACCF23CF6779C506110431AA766F346273E`.
- CAN remained disabled; no vehicle/CAN operations were performed.

The current firmware and defaults use the ST7789 backend with the visual-confirmed
rotation/MADCTL and pin mapping above. Do not treat UART initialization/PASS markers
alone as controller silicon identification.

## Normal UI and resistive-touch test (2026-09-28)

- ESP-IDF v5.5.5 built the ESP32-S3 app. The final image was written on COM5
  only to the factory app partition at `0x10000`; esptool verified the written
  image hash. Boot output reported ESP-IDF v5.5.5 and `can=disabled`.
- Serial output reported `MINIDASH_DISPLAY_INIT status=PASS driver=ST7789`
  and `MINIDASH_TOUCH_INIT status=PASS controller=XPT2046`, with SCLK=GPIO12,
  MOSI=GPIO11, touch MISO=GPIO13 and touch CS=GPIO14. The normal UI then
  reported `touch=ready`.
- Touch-down samples appeared on COM5 with raw X/Y, Z1/Z2, pressure and
  `action=NEXT`; the touch controller and page-navigation event path are working.
  Coordinate-to-screen calibration remains open. The display's exact LCD
  controller silicon marking/ID remains unconfirmed; document the working
  software driver as ST7789, not as a positively identified silicon part.
- That earlier test used synthetic readings visibly marked `DEMO`. The current
  production UI has no built-in demo publisher and shows unavailable values until
  a provider publishes canonical samples. No CAN or vehicle connection was used.
  The TFT LED pin is powered from 3.3 V continuously.

## Production UI layout review (2026-09-29)

- After the user reviewed the physical display, the card spacing, lower `NO DATA`
  labels and footer were adjusted to keep text inside the 320x240 layout. The user
  confirmed that the text now looks aligned.
- Normal screen navigation was corrected so Settings is no longer included in the
  next/previous screen cycle. Settings remains reachable by holding, and Back
  returns to the screen that opened it.
- The latest firmware build succeeded and was flashed to COM5. Serial boot output
  confirmed `ST7789` at 320x240, rotation 2, and that the UI and touch input were
  ready. The latest navigation change has not yet been confirmed with a physical
  gesture after flashing.
- CAN remained disabled; no vehicle connection or live telemetry was used.

## Offline and bench checks

1. Run `python -m unittest discover -s tests -p test_display.py -v` for the host
   display reference contract. Run the broader suite where the host supports its
   platform-specific SocketCAN test.
2. With ESP-IDF 5.5.5 available, run `idf.py build` in `firmware/`. The UI core
   also provides a `ui-test` serial command; those firmware self-tests require a
   flashed target and have not been run as part of the source build.
3. Flash and visually inspect the new production UI on USB power before using it
   in a vehicle. Program the custom partition table as part of the flash. Enable the RGB/
   black smoke option only when explicitly checking panel fills. To identify the
   TFT controller silicon independently, inspect its marking or wire TFT SDO to
   a verified ESP32-S3 input and add ID readback.
4. Verify touch gestures and `sleep`/backlight behavior on the physical board.
   Calibrate coordinates before adding location-based targets; the LED is always on.
5. Inspect the SN65HVD230 module for 120 Ω; remove/disable extra termination
   before connecting to the vehicle. Validate OBD cable continuity, fused supply,
   common ground and LM2596 output independently.
6. Keep CAN disabled until the signal provider and vehicle wiring have their own
   reviewed listen-only validation.
