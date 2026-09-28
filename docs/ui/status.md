# MiniDash embedded UI status

Updated: 2026-09-29; working branch: `feature/production-ui`.

## Current state

The existing ESP32-S3 firmware now has a production-oriented, read-only LVGL UI
on the tested display/touch hardware. The UI consumes canonical signal samples
through the firmware publishing API; it contains no CAN IDs, DIDs, ECU selection
or vehicle-specific decoding. CAN remains disabled and no live vehicle data
provider is connected, so unavailable values are expected at boot.

Hardware profile:

- ESP32-S3 with 16 MiB flash and 8 MiB PSRAM.
- ST7789 ESP-IDF panel backend, 320x240 landscape, logical rotation 2.
- XPT2046 touch controller on the shared SPI bus.
- The LCD controller silicon marking has not been confirmed independently.
- The display backlight is wired directly to 3.3 V, so software brightness
  control cannot dim it on this board.

## Delivered

- LVGL screens for Dashboard, Performance, Temperatures, Drivetrain, Service
  Status, Custom and Settings.
- Canonical signal binding with explicit valid, stale, unavailable, unsupported
  and error states. Missing or expired samples are not shown as zero.
- Semantic input actions shared by touch and the serial console. Touch uses
  full-screen tap, double-tap and hold gestures; location calibration is not
  needed for these gestures.
- OEM, Sport and Minimal themes, day/night setting, units, NVS persistence,
  validated custom-widget settings and configurable warning thresholds through
  the firmware API.
- Shared panel/LVGL orientation mapping for the working landscape rotation.
- UI spacing and typography corrections: removed duplicate card padding, raised
  lower `NO DATA` labels, kept long headings from wrapping, and shortened/limited
  footer text to one line.
- Navigation correction: ordinary next/previous navigation cycles Dashboard,
  Performance, Temperatures, Drivetrain, Service Status and Custom, then wraps
  to Dashboard. Settings is opened separately by holding and returns to the
  originating screen.

## Verification

- ESP-IDF v5.5.5 firmware build succeeded after the latest navigation change.
- The build was flashed to the board on COM5; esptool verified the image hash.
- Serial boot reported the ST7789 backend at 320x240, rotation 2, and both UI
  and touch input ready.
- The user visually reviewed the prior layout update and reported that the text
  now looks aligned. Physical confirmation of the latest navigation behavior is
  still pending.
- The on-device `ui-test` command reported 6 passed and 0 failed on the build
  immediately before the navigation correction. It was not rerun after that
  correction.

## Known limits

- No live signal provider is connected; all vehicle values remain unavailable.
- CAN is disabled. No CAN bus or vehicle was connected during the UI work.
- Custom widget bindings can be stored and validated, but there is no on-screen
  dashboard layout editor yet.
- The Settings screen exposes the global warning switch; per-signal threshold
  editing is not available on-screen.
- The touch controller currently maps the whole display to gestures; calibrated
  location-based touch targets are not implemented.
- Backlight dimming is unavailable with the current direct 3.3 V wiring.
- The LCD controller silicon ID is unknown; the verified software backend is
  ST7789.
- Runtime heap/PSRAM high-water marks, LVGL render timing and sustained update
  performance have not yet been measured.

## Next steps

### Next bench session

1. Confirm the updated screen cycle on the physical display: Custom → Dashboard.
2. Hold to open Settings from a content screen, then hold to verify it returns
   to that same screen.
3. Add a navigation self-check for the content-screen cycle and Settings return
   behavior, then rerun the on-device UI self-checks.

### Following work

1. Define and connect a trusted read-only signal provider to the existing
   canonical publishing API. Preserve the host/runtime and UI boundary; do not
   put CAN/DID decoding in the display layer.
2. Measure free/minimum heap, PSRAM use, task stack headroom and LVGL refresh
   timing under representative signal updates.
3. Add an on-screen custom-dashboard editor and per-signal warning-threshold
   controls only after the core provider path is stable.
4. Before enabling any vehicle bus work, verify the transceiver termination,
   OBD cable continuity, fused power and grounding. Keep CAN disabled until
   listen-only behavior is separately reviewed.
5. Identify the LCD controller by its marking or a verified ID-read path before
   documenting its silicon model as confirmed.
