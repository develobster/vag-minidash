# Embedded UI architecture

## Boundary

The Python runtime remains the authority for transport, ISO-TP, UDS, discovery,
identity, knowledge providers, acquisition planning, freshness and source
arbitration. Python does not run in the ESP32-S3 firmware. The embedded UI does
not contain CAN identifiers, diagnostic requests, ECU selection, DTC logic or
vehicle-specific decoders. `can=disabled` remains the firmware boot state.

The optional USB Serial/JTAG bridge from the host `watch` runtime to the
firmware's canonical publishing API is described in
[the host bridge contract](host-bridge.md).

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
when their rendered values change. Stable screens are created at startup; the
custom dashboard is rebuilt only when its layout or signal bindings change.
Fixed telemetry cards resolve their persisted per-dashboard signal overrides
against the same canonical signal store.
The telemetry refresh path uses fixed snapshots and static label buffers.
Upstream LVGL examples and demos are disabled in the firmware configuration.
Views include four dashboard choices (OEM, Sport, Minimal and Custom),
performance, temperatures, drivetrain, service status and settings.

Input arrives as semantic actions through `md_display_app_dispatch()`. The API
accepts touch, button, steering-wheel, stalk, BLE or console sources, so hardware
drivers do not need to know about screens or settings. At present only the
console and XPT2046 touch reader are connected. The touch task filters and
calibrates raw coordinates into the verified 320x240 landscape orientation.
Large side zones navigate between screens, central card taps open the generic
Focus View, and swipes remain available. Settings rows select without changing
their values; explicit CHANGE, MORE and PREV controls apply actions. Before
calibration, full-screen gestures remain available. A touch while sleeping
wakes the display. GPIO buttons, steering controls and BLE input have no
hardware adapters yet.

## Settings and warnings

The shared schema-1 configuration validates up to four pages of eight widgets,
logical geometry, bindings, types, ranges/precision, colors and warnings.
Its bounded explicit little-endian CRC blob is committed to NVS before publication.
Legacy v1/v2/v3 records are retained; touch calibration has an independent key.
Local writes are deferred 1.5 seconds and are blocked during preview/editor sessions.
Revision checks prevent stale edits. Invalid records are retained without erase.
See [configuration](configuration.md) for Preview/Save/Cancel and HTTP contracts.

Long press opens the signal picker for cards on OEM, Sport and Minimal
dashboards, Performance, Temperatures, Drivetrain and Custom. The first six
groups persist fixed-card overrides separately; Custom keeps its existing
widget bindings. Picker changes affect only UI signal selection and never
send a vehicle request.

The Custom dashboard supports eight value bindings on a 12 by 8 grid. Built-in
layout presets include 1, 2, 4, 6 and 8 widgets plus 1-large/2-small,
1-large/4-small and 2-large/4-small mixed arrangements. Layout geometry is
stored independently from canonical signal names. Settings provides a six-page
layout and per-slot signal selector. Numeric, bar, quality-status and graph widgets render in LVGL. Four Custom pages support validated free logical geometry and a local mobile Web editor. Gauge widgets are not implemented. The 8-widget layout has not
yet passed a physical readability review.

Warnings are disabled by default and apply only to fresh valid values. Thresholds
are configurable through `md_display_app_set_warning()` for the four supported
warning slots. Settings exposes the global warning switch and per-signal enable
switches; numeric threshold values are editable through the Web editor and are not validated on a vehicle. Brightness is shown as FIXED and its control is locked on the
current board because the backlight is wired directly to 3.3 V. No GPIO drives
the LED load.

## Build and verification

The project pins LVGL 9.5.0 and `esp_lvgl_port` 2.9.0. `firmware/partitions.csv`
provides 3 MiB for the factory app plus NVS and PHY-init partitions. The current
16 MiB board profile has room for this layout.

Run `idf.py build` from `firmware/` with ESP-IDF 5.5.5. The deterministic UI
core checks are available on target with the serial command `ui-test`; a
successful compile does not execute those checks. The 2026-09-30 build succeeded
and the app-only image was flashed to COM5 at 0x10000, preserving NVS. On-device
`ui-test` reported 25 passed and 0 failed; four consecutive real AP/HTTP test cycles reported 24 passed and 0 failed each. Layout readability and touch
navigation still require a person to inspect the real panel. See
[the current status and next steps](status.md) for details.

Custom rebuilds bind static LVGL label text to permanent screen storage. A new root is loaded before deleting the previous root under the LVGL lock. The renderer contract and signal/history model are in the portable C core; ESP storage, AP/HTTP and LVGL adapters remain platform-specific.
