# Device configuration and local editor

CYD 0.5.0: the device-only Demo mode setting is stored in `md_cyd/demo`
separately from schema 4. Web appearance previews/imports/resets cannot arm CAN.
`/api/device.simulation` is now runtime state; `derived_analytics=false` remains
the CYD feature gate even with DEMO disabled. [Details](../hardware/minidash-cyd-can-mode-2026-10-20.md).

20 October 2026: [Display Studio](display-studio-2026-10-20.md) adds an embedded
visual grid editor, pointer/keyboard move and resize, history, themed inspector
and three additive firmware themes (14–16). Schema 4 and write endpoints remain
compatible. CYD/S3 builds and browser/native tests pass; physical 0.4.0 acceptance
was pending at initial delivery. After reconnection, [COM4 installation and
69 HTTP checks passed](../hardware/minidash-cyd-studio-flash-2026-10-20.md),
with calibration, AUTO and user settings preserved. External phone/LCD acceptance
for 0.4.0 remains separate.

Current (2026-10-04): schema 4, 92 stable canonical IDs, six analytics presets
and configurable event/context rules. See [delivery](../research/engine-analytics-delivery-2026-10-04.md).
Dated update paragraphs below describe prior versions.

Focus update (2026-10-01): Volkswagen/Porsche now show a value-driven needle
only in single-signal Focus; ordinary cards have no instrument arcs. Five
reference vehicle profiles, Tiguan fallback and Auto/manual ranges use schema 3.
Automatic matching is connected through host discovery → USB, not autonomous
MCU CAN acquisition. See [implementation, sources and acceptance](focus-instruments-2026-10-01.md).

Wi-Fi/theme update (2026-10-01): local settings remain editable with AP enabled;
Wi-Fi shows an ON/OFF indicator. Eleven themes now include Volkswagen/Porsche,
monospaced retro fonts and distinct frames/instrument artwork. See
[implementation, tests and remaining checks](wifi-and-instrument-themes-2026-10-01.md).

Rotation update (2026-09-30): Settings supports 0/90/180/270 degrees, with
portrait card/grid reflow and transformed calibrated touch. Configuration schema
2 migrates old build-only orientation to relative 0 without changing common
settings. See [implementation, migration and verification](rotation-and-portrait-2026-09-30.md).

Theme/startup update (2026-09-30): nine presets including Terminal, Matrix,
Fallout, Amber CRT, Cyberpunk and Arctic; boot-only CRT reveal with autonomous
LEDC fade (~2 s) to saved brightness; new-settings default is now 70%.
See [implementation and acceptance](themes-and-crt-startup-2026-09-30.md).

Current backlight update (2026-09-30): GPIO4 is owner-confirmed and enabled for
LEDC PWM (5 kHz, 12 bit, gamma 2.0), using the existing Brightness setting and
NVS. Manual Settings control, 10% readable minimum, reboot persistence and cold
USB boot have passed physical bench acceptance. Prior FIXED/3V3 descriptions
below record the preceding wiring. See [backlight implementation and acceptance](../hardware/backlight-pwm-2026-09-30.md).


Updated 2026-09-30. Implementation: `ui_configuration.{h,c}`,
`ui_configuration_nvs.c`, `ui_configuration_json.c`, `ui_web.c`, `webui.html`
and the existing `display_app.c` / `ui_screens.c` adapters.

## Use

1. Open Settings, go to page 6, select **Wi-Fi configuration**, then CHANGE.
   The selected row alone never starts configuration mode. Console `config`
   is equivalent. Driving context rejects this action.
2. The device shows `MiniDash-xxxxxx`, a freshly generated WPA2 password and
   `http://192.168.4.1`. Connect a phone to this AP and open that address.
   The display UI remains operational. There is no internet dependency.
3. Edit the common settings, pages, layouts, signal bindings, numeric/bar/status/
   graph widgets, canonical ranges, precision and labels, colors or warnings.
   Page titles/labels support the embedded Latin/Cyrillic glyph subset.
4. Preview applies a validated candidate to RAM and shows Custom on the display. Save/Cancel restores the previously active screen. Save commits first, then
   publishes it. Cancel restores the preview baseline; without an active preview it preserves local edits. AP shutdown or
   its absolute ten-minute timeout cancels any unsaved preview.
5. Revisit Wi-Fi configuration and CHANGE to stop the AP. Ordinary BACK returns
   to the UI without stopping the session. Wi-Fi is off at normal startup.

The editor uses firmware endpoints for signal definitions, layouts and theme
presets. It does not contain a separate fake signal registry. Its browser
preview is schematic and shows `NO DATA`, never invented live readings.
Import is only staged in the browser until device validation and Preview/Save.
Export is versioned JSON and contains no calibration or Wi-Fi credentials.

## Focus scale settings

Schema 4 retains `common.gauge_auto` (default true) and `common.gauge_ranges`,
92 min/max pairs in canonical signal-registry order. Auto selects the volatile
recognized profile or Tiguan fallback; manual mode uses the stored ranges.
The local Web editor exposes this mode and ranges; local theme/language/rotation
edits preserve them. Schema 1/2 imports initialize these fields and retain all
existing common settings and relative schema-2 rotation. See the
[Focus report](focus-instruments-2026-10-01.md) for sources and variant limits.

## Contract

- `GET /api/config`, `/api/defaults`, `/api/device`, `/api/signals`,
  `/api/layouts`, `/api/themes`; `/api/session` returns a volatile session token.
- `POST /api/preview`, `POST /api/cancel`, `PUT /api/config` accept the complete
  schema-4 configuration with its current `revision`.
- Mutations require `X-MiniDash-Session`, JSON content type and same AP Origin
  when Origin is present. No CORS access is granted. Request bodies are bounded
  to 16 KiB, nesting to eight levels, and duplicate/unknown keys are rejected.
- 400: invalid settings, capability mismatch or rotation outside 0–3;
  403: missing session, invalid origin or driving lockout;
  409: stale revision; 500: allocation/storage/availability failure.
- No DTC clear, coding, adaptation, arbitrary UDS, ECU identity modification,
  remote shell or diagnostic write endpoint exists.

The HTTP task runs below UI/touch priority. Two AP clients, three HTTP sockets,
bounded buffers and receive/send timeouts limit work. There is one editor
revision: local setting writes and card rebinding remain available during an AP
session. Local editing discards a temporary Web preview and invalidates stale
Web drafts (HTTP 409; reload the editor). Preview cannot be saved
by the asynchronous local-settings writer. Before opening AP, pending local
changes are checkpointed. On commit failure the current configuration and
revision remain unchanged.

## Storage / migration

Legacy settings v1/v2/v3/v4 remain readable through the existing migration adapter.
The canonical schema-4 `MDC1` blob is little-endian, length-bounded and CRC32
protected; it is stored as `minidash_ui/configuration`. Legacy `settings` and
the separate touch-calibration key are retained. Invalid/unknown canonical
records are not erased; runtime uses a legacy/default fallback. A failed write
does not publish the candidate. No factory image or full flash erase is used.

Four user pages reuse the existing Custom dashboard renderer and its action
navigation. NEXT/PREV traverses adjacent Custom pages before leaving the
Custom section; entering it in reverse starts at the last page. Fixed OEM,
Sport, Minimal, Performance, Temperatures and Drivetrain keep their independent
signal overrides. Layout changes preserve existing bindings and widget types.

## Quality, history, brightness and limits

History holds sixteen distinct fresh samples per canonical signal. Min/max
cover the current uninterrupted quality window; the graph shows the ring of
samples, not an evenly timed recorder. Invalid/stale/no-data gaps and demo/live
changes reset the window. Focus displays current value, quality, min/max and
the short graph. Ranges are presentation bounds in canonical units; warnings
continue to use their separate configured thresholds.

GPIO4 controls the owner-confirmed backlight using 5 kHz, 12-bit LEDC PWM,
gamma 2.0 and a readable 10% minimum. Manual changes apply immediately; sleep
uses zero. Boot fades to the saved setting over approximately two seconds while
the CRT reveal expands. AUTO remains unavailable without a verified ambient source.
Day/night AUTO uses night fallback without a confirmed lighting signal.

The device remains a read-only telemetry UI. Acquisition, embedded recording,
DTC read/clear and coding/adaptation are not implemented by this editor.
Mobile readability/touch usability and electrical acceptance still require
physical observation. `ui-test` tests shared contracts; `web-test` exercises
the real AP/HTTP server locally on the MCU and cancels its four-page preview.

## Recovery and electrical preparation

USB `config-export` prints current JSON. `config-legacy` explicitly restores the
retained legacy settings into the current schema, replacing newer UI pages.
It requires AP off and no driving context; it never changes calibration.
Normal migration and Reset UI do not invoke this recovery command.

AUTO brightness has no verified ambient provider or adaptive algorithm.
The current owner-confirmed GPIO4 control and electrical acceptance are recorded
in [the backlight report](../hardware/backlight-pwm-2026-09-30.md).

## Analytics presets, rules and RAM resets

`GET /api/signals` includes provenance/derived/dependencies; `GET /api/presets`
returns six native-generated eight-widget pages: ENGINE, KNOCK/RETARD, MISFIRE,
BOOST, THERMAL and TRIP/PERFORMANCE. Applying one replaces the active Custom
page only; it can be edited normally, or any fixed-screen card can bind the same
metrics through the paged on-device Widget Picker (12 objects, eight pages).
Existing Numeric/Bar/Status/Mini graph types are reused. Focus shows current,
observed history min/max, state and LOCAL/OBD CAN/ECU/CANDIDATE source context.
Compact preset captions support eight cards; physical readability is still a gate.

`common.analytics` stores eight optional event rules and idle/load/warmup/heat-soak/
X–Y context settings. Every new rule/context defaults off. Thresholds are canonical
units, not universal vehicle safety limits. Evidence-only counter widths/wrap flags
are deliberately absent from the Web schema and rejected if injected. Standard
warning rules can bind any of the 92 IDs and select high/low direction.

`GET /api/analytics` exposes at most 32 events and overwrite/rate-limit metadata;
missing snapshot fields are null. `POST /api/reset-trip` and
`POST /api/reset-session` use the same revision/session/origin/driving guards as
settings mutations; they reset RAM statistics, not ECU counters or DTCs, and do
not write NVS. Reconfiguring analytics starts a fresh engine session, including
Preview/Cancel threshold changes. Ordinary UI edits preserve analytics settings.

Schema-1/2 records migrate to default ranges/rules. Both real historical schema-3
variants (12 and 14 gauge ranges) migrate preserving old ranges, layout, rotation,
labels and bindings. Schema-4 stores the range count, then 92 ranges and 177 bytes
of analytics settings; CRC/length validation is transactional. Explicit Save
persists configuration only; no sample/event/trip is written to flash. Browser
import permits 32 KiB pretty exports; compact HTTP bodies remain bounded at 16 KiB
and eight levels. Four-page export/parse size is tested.

Named event direction/sign validation keeps OVERBOOST/UNDERBOOST/RETARD semantics
consistent. Generic trim episodes and UI warning rules retain either direction.
