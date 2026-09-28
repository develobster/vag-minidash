# Embedded UI architecture

## Boundary

The Python runtime remains the authority for transport, ISO-TP, UDS, discovery,
identity, knowledge providers, acquisition planning, freshness and source
arbitration. Python does not run in the ESP32-S3 firmware. The embedded UI does
not contain CAN identifiers, diagnostic requests, ECU selection, DTC logic or
vehicle-specific decoders. `can=disabled` remains the firmware boot state.

An embedded data source publishes an already-decoded canonical value with an
explicit state and time-to-live. Until that source is integrated, values remain
unavailable. The display layer does not synthesize vehicle data or infer values
from other signals.

## Firmware data contract

`md_display_app_publish_signal()` in `firmware/main/display_app.h` accepts a
canonical name, numeric value, state, TTL and a flag marking demo data. Names are
matched against this fixed UI vocabulary:

| Canonical name | Canonical unit | Notes |
| --- | --- | --- |
| `engine.rpm` | rpm | Engine speed |
| `vehicle.speed` | km/h | Converted for imperial display |
| `engine.coolant_temperature` | degC | Converted for Fahrenheit display |
| `engine.oil_temperature` | degC | Converted for Fahrenheit display |
| `engine.load` | % | Engine load |
| `engine.charge_pressure_actual` | hPa | Pressure input |
| `engine.ambient_pressure` | hPa | Pressure input |
| `engine.boost` | hPa | Published by a provider; never calculated by the UI |
| `transmission.oil_temperature` | degC | Converted for Fahrenheit display |
| `transmission.gear` | provider-defined integer | No unit conversion |
| `awd.clutch_temperature` | degC | Converted for Fahrenheit display |
| `vehicle.battery_voltage` | V | Provisional canonical name |

Unknown names are rejected and counted in the signal summary. This prevents the
display from silently accepting arbitrary or vehicle-specific signal keys.

Each sample carries one of these states:

| State | Display behavior |
| --- | --- |
| `VALID` | Show the value while its TTL has not expired |
| `STALE` | Hide the value and show stale status |
| `UNAVAILABLE` | Show no value when no current source provides one |
| `UNSUPPORTED` | Show that the signal is not supported |
| `ERROR` | Hide the value and show an error state |

TTL expiration uses wrap-safe millisecond age calculation. A valid sample with a
non-finite value or zero TTL is changed to `ERROR`. The `demo` flag is shown as
`DEMO`; this is a label on provider-supplied sample data, not a built-in demo
publisher.

## Rendering and input

The display controller owns the LVGL screens and polls a fixed-size signal-store
snapshot every 100 ms. Signal publication writes to that fixed store; it does not
depend on display rendering. LVGL uses the ESP LVGL port with two DMA-capable
partial buffers, each 16 rows high. Text and severity colors are updated only
when their rendered values change. Screens and labels are created once at UI
startup; the telemetry refresh path uses fixed snapshots and static label
buffers. Upstream LVGL examples and demos are disabled in the firmware
configuration. Views include four dashboard choices (OEM, Sport, Minimal and
Custom), performance, temperatures, drivetrain, service status and settings.

Input arrives as semantic actions through `md_display_app_dispatch()`. The API
accepts touch, button, steering-wheel, stalk, BLE or console sources, so hardware
drivers do not need to know about screens or settings. At present only the
console and raw XPT2046 touch reader are connected. Touch is debounced and
dispatches next-screen; coordinate calibration and on-screen hit targets are not
implemented. Buttons, steering controls and BLE input have no hardware adapters
yet.

## Settings and warnings

Settings are validated before use, encoded in a versioned fixed-width 393-byte
format with CRC32, and stored as a single NVS value under namespace `minidash_ui`.
Writes are deferred until 1.5 seconds after the latest edit. Invalid or missing
stored settings fall back to defaults without erasing NVS. The custom dashboard
accepts up to six validated, non-overlapping value widgets on a 12 by 8 grid.
Only value widgets are implemented. The settings screen does not yet include a
layout editor; custom bindings use the stored settings model.

Warnings are disabled by default and apply only to fresh valid values. Thresholds
are configurable through `md_display_app_set_warning()` for the four supported
warning slots. The settings screen exposes the global warning enable switch;
per-signal threshold editing is not yet available in the screen.

## Build and verification

The project pins LVGL 9.5.0 and `esp_lvgl_port` 2.9.0. `firmware/partitions.csv`
provides 3 MiB for the factory app plus NVS and PHY-init partitions. The current
16 MiB board profile has room for this layout.

Run `idf.py build` from `firmware/` with ESP-IDF 5.5.5. The six deterministic UI
core checks are available on target with the serial command `ui-test`; a
successful compile does not execute those checks. The new UI has not yet been
flashed or visually verified on physical hardware.
