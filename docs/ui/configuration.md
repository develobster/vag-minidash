# Device configuration and local editor

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
   publishes it. Cancel restores the last saved configuration. AP shutdown or
   its absolute ten-minute timeout cancels any unsaved preview.
5. Revisit Wi-Fi configuration and CHANGE to stop the AP. Ordinary BACK returns
   to the UI without stopping the session. Wi-Fi is off at normal startup.

The editor uses firmware endpoints for signal definitions, layouts and theme
presets. It does not contain a separate fake signal registry. Its browser
preview is schematic and shows `NO DATA`, never invented live readings.
Import is only staged in the browser until device validation and Preview/Save.
Export is versioned JSON and contains no calibration or Wi-Fi credentials.

## Contract

- `GET /api/config`, `/api/defaults`, `/api/device`, `/api/signals`,
  `/api/layouts`, `/api/themes`; `/api/session` returns a volatile session token.
- `POST /api/preview`, `POST /api/cancel`, `PUT /api/config` accept the complete
  schema-1 configuration with its current `revision`.
- Mutations require `X-MiniDash-Session`, JSON content type and same AP Origin
  when Origin is present. No CORS access is granted. Request bodies are bounded
  to 16 KiB, nesting to eight levels, and duplicate/unknown keys are rejected.
- 400: invalid settings, capability mismatch or unsupported rotation change;
  403: missing session, invalid origin or driving lockout;
  409: stale revision; 500: allocation/storage/availability failure.
- No DTC clear, coding, adaptation, arbitrary UDS, ECU identity modification,
  remote shell or diagnostic write endpoint exists.

The HTTP task runs below UI/touch priority. Two AP clients, three HTTP sockets,
bounded buffers and receive/send timeouts limit work. There is one editor
revision: local setting writes and card rebinding are blocked during an AP
session, while display navigation remains available. Preview cannot be saved
by the asynchronous local-settings writer. Before opening AP, pending local
changes are checkpointed. On commit failure the current configuration and
revision remain unchanged.

## Storage / migration

Legacy settings v1/v2/v3 remain readable through the existing migration adapter.
The canonical schema-1 `MDC1` blob is little-endian, length-bounded and CRC32
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

`md_brightness_t` prepares bounded 10–100% manual PWM with 1% steps every 20 ms,
and zero for screen-off. It uses the existing display backend LEDC PWM only if
the board profile enables dimming. Current BL is directly powered from 3.3 V:
FIXED, no brightness slider or invented GPIO. AUTO cannot be enabled without
both dimming hardware and a verified ambient source. Day/night AUTO remains
night fallback without a confirmed vehicle lighting signal.

For later hardware dimming, first establish whether BL is an enable input or
LED power. For LED power use a correctly rated logic-level MOSFET/transistor
and the module's proper current limiting, with common ground, gate resistor
and pulldown; do not connect the power load to GPIO. No such wiring is claimed
verified by the current software.

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

AUTO brightness currently has capability gates only: no ambient provider or
adaptive algorithm is connected. Actual BL remains FIXED. For a verified LED
power input, a possible external high-side arrangement is P-MOS source to the
rated 3.3 V supply, drain to BL, gate pulled up to source with 10 kOhm; an NPN
collector pulls the gate down through 100 Ohm, emitter to common ground, base
via 1 kOhm from a subsequently approved GPIO and base pulldown 100 kOhm. GPIO
high enables the load; GPIO never supplies LED current. Select a P-MOS with
Rds(on) specified at Vgs -2.5 V, current rating above measured load/inrush,
appropriate supply protection and the module's verified LED current limiting.
Confirm BL topology, voltage/current, gate switching, thermal behavior and PWM
flicker on a bench before enabling the existing 5 kHz/10-bit LEDC backend.
No component part number, new GPIO or hardware approval is assumed here.
