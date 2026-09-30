# MiniDash embedded UI status

Current backlight update (2026-09-30): GPIO4 is owner-confirmed and enabled for
LEDC PWM (5 kHz, 12 bit, gamma 2.0), using the existing Brightness setting and
NVS. Manual Settings control, 10% readable minimum, reboot persistence and cold
USB boot have passed physical bench acceptance. Prior FIXED/3V3 descriptions
below record the preceding wiring. See [backlight implementation and acceptance](../hardware/backlight-pwm-2026-09-30.md).


UI delivery record updated 2026-09-30. Original work branch: `feature/ui-ux-touch-zones`, based on
`feature/production-ui` at `00c6317c08b055ae0f56791ed250225c14b4301e`.

## Current state

The ESP32-S3 firmware has a read-only LVGL UI for a 320x240 landscape display.
It receives canonical signal samples through a firmware publishing API; it
contains no CAN identifiers, DIDs, ECU selection or vehicle-specific decoding.
CAN remains disabled and no live vehicle data provider is connected.

Hardware profile: ESP32-S3, 16 MiB flash, 8 MiB PSRAM, ST7789 ESP-IDF panel
backend, logical rotation 2, XPT2046 touch on shared SPI. The LCD silicon
marking is not confirmed. The LED backlight is wired directly to 3.3 V, so
software dimming is unavailable and Settings now shows `FIXED`.

## Implemented

- Dashboard, Performance, Temperatures, Drivetrain, Service Status, Custom,
  Settings, Diagnostics and generic Focus View screens.
- Canonical signal binding with VALID, STALE, UNAVAILABLE, UNSUPPORTED and
  ERROR states. Missing or expired samples are never shown as zero.
- Calibrated XPT2046 coordinates, five-point affine calibration stored in NVS,
  large side hit zones, visible arrows, swipe navigation, card-to-Focus and
  pressed feedback. A development touch overlay can be enabled at build time.
- A versioned semantic action layer shared by touch and console, ready for
  future GPIO and evidence-backed vehicle button adapters.
- Long press on a card opens a signal picker on OEM, Sport and Minimal
  dashboards, Performance, Temperatures, Drivetrain and Custom. Fixed screens
  persist per-card overrides separately; Custom keeps its widget bindings.
  Tapping a signal changes only that card. BACK/long press cancels.
- Custom dashboard layouts for 1, 2, 4, 6 and 8 widgets, plus 1-large/2-small,
  1-large/4-small and 2-large/4-small layouts. Signal binding remains separate
  from layout geometry. Settings provides per-slot selectors for eight signal
  bindings and six pages of settings. Language switches between English and
  Russian; translated strings use embedded Cyrillic font subsets.
- Numeric value cards with a compact rendering for dense tiles. Narrow cards
  use one-line signal aliases (RPM, SPD, LOAD, etc.); Focus View keeps the full
  signal name. Numeric, bar, quality-status and short-history graph widgets now render in LVGL. Focus includes min/max/history; gaps and demo/live transitions reset history.
- OEM, Sport and Minimal themes, day/night palettes, metric/imperial conversion,
  and configurable warning enable switches. Warning thresholds remain disabled
  by default and are not vehicle-validated.
- Canonical schema-1 CRC configuration stores four pages of eight widgets, logical geometry, type/range/precision/labels, palettes and warning settings. Legacy v1/v2/v3 settings and separate touch calibration remain intact.
- Explicit temporary WPA2 AP hosts a mobile Web editor with firmware registries, layouts/themes, import/export, Preview/Save/Cancel and resets. Preview switches to Custom; Save/Cancel restores the previous screen. Ten-minute absolute timeout, session token, revision checks, bounded strict JSON and no diagnostic write routes.
- Eight portable C modules compile without LVGL/ESP-IDF. Button debounce/long-press/repeat/lockout, renderer abstraction, logical presentation slicing and capability-gated brightness transitions exist. Physical GPIO/OLED/automotive adapters remain future work.
- Optional USB Serial/JTAG host bridge from `minidash_live watch`; it forwards
  only canonical, arbitrated valid values and preserves quality/TTL states.

## Historical UI delivery verification (6f436dd)

Current integration acceptance and host counts: [repository report](../research/repository-consolidation-2026-09-30.md). The hardware results below belong to the prior UI delivery; consolidation does not flash the board.

- Host suite: `python -m unittest discover -s tests -v` — 129 tests, 13 skipped,
  no failures.
- ESP-IDF 5.5.5 build succeeded. App image: 1,717,984 bytes (`0x1a36e0`),
  with 45% of the 3 MiB app partition free. Russian font subsets are
  uncompressed because `LV_USE_FONT_COMPRESSED` is disabled in this build.
- App-only flash to COM5 at `0x10000` succeeded and esptool verified the image.
  NVS was not erased; the stored language and touch calibration remain intact.
- On-device `ui-test`: 25 passed, 0 failed. It also verifies Cyrillic glyphs
  at all four UI font sizes and rejects compressed font assets in this build.
- Factory backup was rechecked and unchanged:
  `artifacts/hardware/esp32s3-factory-16mb.bin`, SHA-256
  `CD0F30AC9491DA499EAF2167F07B5ACCF23CF6779C506110431AA766F346273E`.
- The touch calibration record is on a separate NVS key. Earlier six- and
  eight-widget photos supplied by the user were from older firmware. Physical
  readability after switching to Russian still needs confirmation on-panel.

## Limits and blockers

- No CAN or live signal provider is connected; telemetry will remain unavailable
  until a reviewed source publishes it.
- Eight-widget and mixed layouts pass geometry and firmware tests, but their
  readability, touchability and practical maximum density are not physically
  verified on the panel.
- Four Custom pages, numeric/bar/status/graph widgets and Web configuration are implemented. Gauge/dial widgets remain future work. Browser visual inspection and sustained render timing remain unverified.
- No physical GPIO buttons or car-button CAN/LIN adapter is implemented. No
  vehicle button IDs or illumination signals are assumed.
- Brightness control is locked because of the confirmed current wiring. PWM
  must wait for a verified control input or an external LED driver.
- Warning presets are existing firmware values, disabled by default and not
  validated as vehicle safety limits.
- AP cycles measured internal free memory 115,331 before / 115,259 after, minimum 39,488 bytes. Sustained rendering latency remains unmeasured.
- AUTO brightness has capability gates but no adaptive algorithm or verified ambient input. Only current rotation 2 is accepted by configuration writes.
- Native CTest 2/2 executables passed (eight portable groups plus JSON); Node editor logic tests passed. Four consecutive on-device real AP/HTTP cycles passed 24/24 each. Reboot retained original Russian settings and one Custom page; UI tests again passed 25/25.
- Eight modules compile to Cortex-M3 objects totaling 22,546 bytes of text; this is not a linked Lite firmware or complete memory budget.
- Stress tests exposed and fixed NULL diagnostic-label retheme and freed static-text buffers during Custom rebuild. A failed intermediate test persisted its synthetic pages; untouched legacy settings restored user configuration before final tests. No calibration key was overwritten.

## Immediate bench check

Switch Language to Russian and confirm that the settings labels, buttons and
page indicator remain visible. Then press and hold a card on Performance,
Temperatures or Drivetrain; choose a different signal and confirm only that
card changes. Repeat on a built-in dashboard and Custom. A normal card tap
should still open Focus View. The board has no live data provider, so values
remain unavailable during this check.

## Local editor acceptance

Settings page 6 → Wi-Fi configuration → CHANGE. Connect a phone to the shown SSID/password and open http://192.168.4.1. Create three/four pages, Preview with NEXT/PREV, Cancel, then Save a deliberate configuration and reboot. Check Russian labels, long hold and dense hit targets on-panel. Device tests verify contracts; mobile visual/touch acceptance still needs physical observation. See [configuration](configuration.md) and [targets](../architecture/targets.md).
