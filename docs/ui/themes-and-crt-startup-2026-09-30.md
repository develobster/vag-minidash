# MiniDash themes and CRT startup — 2026-09-30

## Behavior

The existing Settings → Theme control now selects nine presets. OEM, Sport and
Minimal retain IDs 0–2; six new IDs are appended. Existing saved records remain
valid without a schema change. No second theme selector or brightness control
is added. Color/style changes preserve dashboards, signal bindings, warnings,
language, calibrated XPT2046 input and the confirmed ST7789 pin/orientation map.

| Theme | Color character | Surface/style |
| --- | --- | --- |
| OEM | Blue/teal instrument cluster | Existing rounded cards |
| Sport | Neutral graphite/silver | Existing instrument style |
| Minimal | Black/white | Existing restrained style |
| Terminal | Green phosphor, black-green | Square outlines, spaced headings |
| Matrix | Bright emerald, near-black | Square frames, faint green glow, scanlines |
| Fallout | Olive/lime phosphor | Double borders, small corners, soft CRT texture |
| Amber CRT | Warm amber/orange | Soft phosphor glow and scanlines |
| Cyberpunk | Magenta, cyan, violet | Angular double frames with neon glow |
| Arctic | Ice blue | Rounded 10px panels; separate light DAY palette |

Terminal/Matrix/Fallout/Amber intentionally retain dark phosphor backgrounds even
in DAY mode. Cyberpunk also retains its dark neon palette. Warning, critical,
unavailable and stale states retain separate semantic colors. Scanlines are
low-opacity, static draw effects, not animated noise obscuring measurements.
Styles apply across dashboards, details, settings and pickers; raw hit zones
and navigation remain unchanged. Russian glyphs keep the existing bundled fonts.
These are original palettes/styles inspired by the requested aesthetics; no
third-party game/movie art, fonts, logos or asset packs are included.

The Web `/api/themes` resource carries preset names and display-only style
metadata. Its selector derives options from this resource rather than a fixed
three-item list. Card preview reflects corners, border thickness, glow and
scanline backgrounds. Metadata is not written into the persisted configuration
schema. Existing custom color editing continues to use the chosen preset style.

## Startup

GPIO4 remains off from early application initialization through panel/LVGL setup.
After the first masked frame is flushed, startup shows a small central phosphor
point; over ~300 ms it widens into a horizontal line. Starting at 250 ms, the
opening expands vertically using smoothstep over ~1500 ms, exposing the actual
restored UI. White core and theme-colored glow fade away, leaving the real screen.
Four noninteractive black masks and one glow object are temporary LVGL top-layer
objects and are deleted at the end. No full-screen animation bitmap is allocated.

Concurrently, the backlight controller uses ESP-IDF LEDC hardware fade from OFF
to the gamma-corrected duty of the last saved brightness. LEDC remains 5 kHz,
12 bits, low-speed timer/channel 0. Fade scale/cycle values are selected for
roughly 2 seconds and ~1000 hardware duty steps (within resolution limits),
so brightness does not become a sequence of large jumps when SPI rendering is
busy. Duty changes during the fade are autonomous; the UI task only animates
masks and sets the exact target at completion. A fade initialization/error leaves
the output inactive and logs failure.

The normal UI's manual setting still spans 10–100%; zero is temporary startup
or sleep/off only. New installations default to **70%**. Existing saved brightness,
including values other than 70%, is retained. Temporary ramp levels never enter
NVS. Settings, calibration and the configuration schema are unchanged. Startup
runs on boot, after display initialization, not on every navigation or theme change.
Touch starts once the startup reveal completes; console readiness for UI operations
therefore occurs after this short boot effect. Sleep/wake keeps its existing behavior.

## Implementation and verification

- `ui_core.[ch]`: appended theme IDs and changed new-settings default from 80 to 70.
- `ui_theme.[ch]`: palettes, names and style traits; existing color formatting remains.
- `ui_startup.[ch]`: portable bounded reveal geometry/time model.
- `ui_screens.[ch]`: theme traits, draw-post scanlines, temporary startup objects.
- `display_app.c`: boot-only startup orchestration; existing manual settings path.
- `backlight_controller.[ch]`: autonomous LEDC startup fade with bounded parameters.
- `ui_configuration_json.c`, `ui_web.[ch]`, `webui.html`: theme names/style metadata
  and Web selector/preview. Configuration CRC, schema, storage and rotation remain.
- Firmware/core CMake, native driver doubles, portable/device self-tests and
  Web logic tests are updated alongside the implementation.

Native CTest passed portable-core, backlight-driver-contract and configuration-json
(3/3). Tests cover all nine theme settings/config codec round trips, old stable
IDs, distinct valid palettes, 70% default, reveal monotonicity/bounds at every
millisecond and all Manual target levels, saturation after completion, hardware
fade planning (~2 seconds), invalid fade requests and remembered sleep/wake.
The Web logic test passed nine named presets and Matrix/Arctic preview style
changes along with existing editor operations. These are logic tests, not a
browser pixel rendering claim.

ESP-IDF 5.5.5 and its existing Windows environment are reused. Build directory
`firmware/build-backlight` is retained. COM5 is the same native USB Serial/JTAG
ESP32-S3, VID:PID 303A:1001, serial 14:C1:9F:50:93:64. App-only flash uses
0x10000, preserving bootloader, partition table, NVS and touch calibration.

First software-stepped prototype device tests passed 26/26 and measured a 2139 ms
reveal to saved 70%, but SPI locks delayed visible brightness steps; final hardware
fade replaces that prototype. Final hardware acceptance is recorded below after
actual tests. Code/register/timing evidence does not substitute for physical
visual sign-off or an electrical current/waveform measurement.

## Remaining work and recovery

P0: obtain visual sign-off of the final startup/fade and styles. All-preset
switching and saved theme/brightness after reboot have passed device checks. P1: future optional boot-effect toggle or
adjustable duration can use the bounded startup model without changing acquisition
or NVS write cadence. P2: TEMT6000/Auto and verified vehicle illumination remain
separate work; no ambient input or vehicle state is fabricated for this effect.

To change theme, use the existing Theme row and CHANGE in Settings, or the Web
Preset selector. Brightness remains on its existing row. GPIO4 and TFT LED wiring
are unchanged from the verified PWM delivery; do not connect LED to GPIO and 3V3
simultaneously. If rolling back to the previous nine-theme-unaware firmware,
select OEM/Sport/Minimal first and let NVS save; that older firmware cannot decode
a newly appended theme ID. Rollback source baseline is commit `8548678`.

References: [LEDC ESP-IDF 5.5.5](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/api-reference/peripherals/ledc.html),
[LVGL 9.5 drawing](https://lvgl.io/docs/open/9.5/main-modules/draw/), and the local
SDK source headers inspected for event layers, line descriptors and fade behavior.

## Final measured device results

- Final ESP-IDF 5.5.5 build passed without compiler warnings/errors in the captured
  build log; app-only COM5 flash passed, with esptool's written hash verification.
- App size: 1736208 bytes; SHA-256 `13570a08e81fc40e2b0a4c29ebc681b3ee54f4b28aef210e1fe9f6271e0318d7`.
- CRT startup elapsed: **2029 ms**, final target 70%.
- The LEDC hardware fade is configured with duty target 2007, scale 3 and cycle
  count 15 at 5000 Hz. Status reads during startup were monotonic:
  0, 3, 252, 486, 768, 909, 1188, 1329, 1611, 1749, 2007, then stable 2007.
  These are sampled hardware counts; intermediate hardware steps are autonomous.
- All nine theme IDs were exercised using existing semantic Settings actions,
  with the dashboard displayed between changes: 6,7,8,0,1,2,3,4,5. The owner's
  selected starting theme was Fallout (5), and it was restored after the cycle.
- After the cycle, full common configuration before/after reboot matched exactly;
  platform configuration/brightness matched, with Fallout and 70% retained.
  Boot restored calibrated XPT2046 controls and the existing ST7789 geometry.
- Device self-tests: **26 PASS / 0 FAIL**, including the new theme/startup tests.
  Native tests: **3/3 PASS**; named Web theme selector and card-style logic PASS.
  No panic/brownout/crash was observed during the captured checks.
- GPIO4, SPI/touch pins, partition table, data model and CAN-disabled state remain
  unchanged. No vehicle testing or sensor input is part of this delivery.
- Physical visual sign-off of the final CRT reveal/fade and theme appearance is
  pending the owner's answer; register/timing tests alone do not establish it.

The existing scratch location for this chat holds build/flash, sampled hardware,
configuration and Web-fixture logs in `work/themes-startup`. The tested binary
contains the pre-commit version string; a later documentation/source commit does
not replace that hardware-tested image. Rebuilds may change metadata and hash.
