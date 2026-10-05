# Targets and shared MiniDash core

Updated 2026-09-30. This remains one repository. No STM32 firmware, OLED
controller, OLED resolution, pin assignment or verified automotive button
decoder has been added.

## Boundaries implemented now

`core/CMakeLists.txt` compiles the **same physical C sources** used by ESP-IDF:
`ui_core`, `ui_model`, `ui_configuration`, `ui_platform`, `ui_theme`, `ui_i18n`,
`ui_touch_map`, and `display_serial_protocol`. These modules include neither
LVGL nor ESP-IDF. JSON is optional and currently used by the ESP32 editor;
the bounded binary configuration codec needs no JSON library.

The shared core owns canonical signal names, quality/TTL/demo handling, warning
thresholds, actions and navigation, logical 12×8 dashboard cells, widget types,
presentation ranges, history/min/max, configuration validation, CRC and legacy
settings conversion. `md_ui_renderer_t` consumes semantic settings/state/store
and diagnostics. Its working adapter is `md_ui_lvgl_renderer()`.

The production diagnostic pipeline remains unchanged:
Transport → discovery → VehicleIdentity/EcuIdentity → IdentityContext → Knowledge
Providers → Signal Registry → Acquisition Planner → passive CAN/UDS → SignalStore
→ SourceArbitrator → UI/CLI/debug. Python discovery/providers/UDS/J2534 cannot
be compiled into STM32 or ESP32 firmware. The embedded C presentation store
consumes canonical samples; it does not replace the runtime SourceArbitrator.
Only twelve canonical signal definitions are currently exposed by the firmware
registry. Extending telemetry, including ignition corrections, still requires
reviewed decoders and vehicle evidence.

### Hardware versus product features

`md_target_profile_t.hardware` says which hardware facilities a target has;
`implemented_features` says which product functions have an implementation.
A CAN peripheral alone does not imply UDS or DTC support. Wi-Fi alone does not
imply a Web UI. Brightness AUTO additionally needs a verified ambient source.

| Facility | Current ESP32-S3 bench | Future Lite proposal |
|---|---|---|
| Display | ST7789 backend, 320×240, rotation 2; LCD marking unconfirmed | Monochrome intent; controller/geometry unconfirmed |
| Input | Calibrated XPT2046 + console | Buttons intended; pins/driver unconfirmed |
| Signals/warnings/navigation/config model | Shared C implementation | Compiles as C for Cortex-M3; no board acceptance |
| Telemetry acquisition on MCU | No CAN acquisition connected; USB canonical bridge | Not implemented |
| CAN | Disabled; external hardware not validated | MCU/peripheral/transceiver must be confirmed |
| UDS, DTC read/clear | Not implemented in this firmware | Not implemented |
| Coding/adaptation | No implementation or configuration endpoint | No implementation |
| Wi-Fi / Web configuration | Explicit temporary AP + HTTP editor | Not a Lite capability |
| BLE | Bring-up tests; no configuration app protocol | Not a Lite capability |
| Brightness | Owner-confirmed GPIO4 LEDC PWM, manually controlled and bench-tested; AUTO needs ambient input | Hardware unconfirmed |
| Logging | Host trace facilities; no embedded recording backend | Resource/storage dependent; not implemented |
| OLED renderer | Not applicable | Not implemented |

## Configuration and rendering

`md_device_configuration_t.common` contains UI language, units, themes, warnings,
fixed-card overrides and up to four pages of eight widgets. Optional `platform`
contains supported color/backlight settings and rotation. Touch calibration
is outside both, in its own NVS key. Export contains no network password.
A Lite importer can use `common` with an absent platform section, subject to
its own capability validation. It must not advertise touch, Wi-Fi or dimming.

Shared layout geometry is logical rather than an OLED framebuffer. A future
renderer can use `md_ui_presentation_slice(6, 3, subpage, ...)` to show a
six-binding page in two presentations, preserving binding order. It should
show value/quality/warning/focus first; dense color tiles, gradients and charts
are optional presentation. No physical OLED format is assumed by this helper.

`md_ui_button_sample()` supplies 30 ms debounce, release-based short press,
800 ms long press and 180 ms repeat only for actions explicitly marked
repeatable. A lockout suppresses the entire held contact until stable release.
The automotive adapter rejects input without an evidence-verified mapping;
it contains no guessed CAN/LIN IDs. A future GPIO adapter only feeds samples
and actions, not LVGL objects.

## Bluepill feasibility: conditional, not approved hardware

ST guarantees **64 KiB flash and 20 KiB SRAM for STM32F103C8**; CB has
128 KiB flash but still 20 KiB SRAM. Do not budget the undocumented extra
flash sometimes present on clone boards.
[ST STM32F103x8/xB datasheet](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf).

The common source files were compiled to Cortex-M3 Thumb objects with Zig/Clang,
`-Os -ffreestanding`, without LVGL/ESP-IDF. The final aggregate object text
was 22,546 bytes (including UI translation tables and calibration/serial code).
This is **not a linked MCU image**: libc, float formatting/math, startup,
interrupts, CAN/ISO-TP/UDS, persistence and renderer are excluded. A full map and
stack analysis are mandatory before selecting C8. The Windows-native tests
exercise the same sources; ARM execution has not been tested.

Measured C object sizes on the native build (the data members also align to
4 bytes on Cortex-M3): configuration 1,836 B, page 420 B, widget 48 B, canonical
store 244 B, history 84 B per signal (1,008 B for twelve), legacy settings
552 B and debounced button state 24 B. Current/saved/candidate configurations
alone can consume 5,508 B. JSON and LVGL are **not** part of the Lite budget.
Validation was changed to reuse a 552-byte legacy scratch record instead of
copying another whole 1,836-byte configuration per page.

### Planning RAM envelope (not a measured STM32 implementation)

| Item | Restricted read-only prototype | Broader diagnostic scope |
|---|---:|---:|
| Configuration copies | 1,836 B, one copy | 5,508 B, three copies |
| Canonical store + history | 244 + 84 B, focused signal only | 244 + 1,008 B, twelve histories |
| Validation scratch | 552 B | 552 B |
| CAN receive queue | 384 B planning reserve | 768 B planning reserve |
| ISO-TP buffers | 1,024 B budget, requires explicit response limit | 8,190 B for two 4,095-byte payloads |
| Separate UDS work | 256 B budget | 512 B budget |
| Stack/interrupt margin | 4,096 B planning reserve | 6,144 B planning reserve |
| Subtotal, before renderer/driver globals | 8,476 B | 22,926 B |
| OLED framebuffer | unknown; depends on confirmed panel | unknown |
| Recording storage/cache | excluded | unknown, additional |

These are design envelopes, not allocations or validated stack high-water marks.
The restricted prototype must reject oversized ISO-TP responses; it cannot
silently truncate DTC or identity payloads. The broader illustrative subtotal
already exceeds C8/CB 20 KiB SRAM. Flash objects likewise exclude all drivers,
libc and diagnostic code. C8 is not approved for the full intended product;
CB increases flash only and does not resolve RAM pressure. External recording
storage and its cache budget require a separately selected hardware design.

For a C8 feasibility prototype, keep at most one/two config copies, use a small
caller-owned validation buffer, reduce page/widget capacity where needed,
limit history to focused signals, use fixed-size transport buffers and bound
stack usage. Include a framebuffer only after the OLED is identified. Evaluate
read-only acquisition first; DTC writes, logging and coding require separate
resource and safety review. No complete diagnostic-device fit has been proven
within 64/20 KiB.

For the requested telemetry + UDS/DTC + saved configuration scope, a more
capacious STM32 is a more practical starting point. An STM32F103RE provides
512 KiB flash and 64 KiB SRAM and retains a CAN peripheral; board/transceiver
selection is still required.
[ST STM32F103xC/xD/xE datasheet](https://www.st.com/resource/en/datasheet/stm32f103re.pdf).
This is a conditional hardware recommendation, not a supported target.

## What can actually be shared

All eight core source modules above are shared now. Percentage of future
complete product code cannot be measured before STM32 acquisition and renderer
exist. Platform-specific parts necessarily include display transport, color or
monochrome renderer, touch/GPIO drivers, CAN/transceiver adapter, flash/NVS
storage, scheduling/watchdog, board power and Wi-Fi/BLE. Diagnostic semantics
can remain shared after a separate MCU transport implementation exists.

Next Lite gate: confirm exact MCU and OLED → build/link a bounded core with
chosen toolchain → account flash/RAM/stack → implement one button and renderer
adapter → test read-only acquisition on a bench. Keep this work in the same
repository; do not copy or fork the current ESP32 UI model.
