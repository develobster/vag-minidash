# Focus needles and vehicle scale profiles — 2026-10-01

Branch: `feature/real-car-validation`, continuing the existing ESP32-S3 prototype.
This delivery replaces the decorative Volkswagen/Porsche arcs on ordinary cards
with an actual value-driven instrument in the single-signal Focus screen.

## Behaviour and use

Choose Volkswagen or Porsche in Settings → Theme, then tap a metric card to
open Focus. The selected signal retains its digital value and quality indicator,
with an analogue needle, scale ticks and units above it. Return with BACK/tap.
All four rotations use the existing calibrated touch transform. Portrait layout
rebuilds retain the selected Focus signal instead of reverting it to RPM.

Only Focus receives this instrument: Dashboard, Performance, Temperatures,
Drivetrain and Custom cards have no decorative dial or needle, including a
one-card Custom page. Their numeric/bar/status/history widget settings still
work. Other themes retain the existing Focus digital/history presentation.
Volkswagen uses its blue/white palette and a thinner pointer; Porsche uses its
warm instrument palette and a thicker pointer. Both retain Roboto Condensed
digits and the existing localized body fonts.

The needle maps the fresh canonical value linearly over a 270-degree sweep and
clamps at the endpoints; the digital reading keeps the actual value. A real
zero remains a valid reading. Expired, STALE, UNAVAILABLE, UNSUPPORTED or ERROR
readings hide the pointer instead of inventing a zero. Warning thresholds and
signal acquisition are independent of these presentation ranges.

Auto mode is enabled by default. Without a fresh recognized profile, Focus shows
`Tiguan II TSI *`; the asterisk denotes the fallback. Default speed is 0–280 km/h,
RPM 0–8000, coolant 50–130 °C. These are scale endpoints, not safe operating
limits, engine rev limits or the vehicle's achievable speed. Coolant uses the
actual sensor reading, not a reproduction of OEM buffered needle electronics.

For a different instrument variant, use the local Wi-Fi editor's **Шкалы
автомобиля · Focus** section: choose **Ручные диапазоны**, edit Min/Max and Save.
Ranges are entered in canonical units even when the display uses mph/°F:
km/h, rpm, °C, hPa, percent, volts or gear number. Pressure is converted to the
selected display units by the existing formatter. Custom-widget bar/history
ranges are separate from Focus instrument ranges. Preview/Cancel/Save and local
settings takeover retain their previous semantics.

## Catalogue and research provenance

The shared source is `data/ui/instrument-profiles.json`. It records sources,
edition/page evidence, variant scope, model aliases, supported year bands and
engine fuel family. It covers common VAG models; this is not a sales ranking or
an exhaustive catalogue of every market, engine and trim.

| Profile | Speed scale, km/h | RPM scale | Coolant, °C | Illustrated variant |
|---|---:|---:|---|---|
| Tiguan II TSI | 0–280 | 0–8000 | 50–130 | Metric petrol analogue cluster |
| Golf VII TSI | 0–260 | 0–8000 | Configured fallback | Ordinary petrol Active Info Display |
| Golf VII GTD | 0–280 | 0–6000 | Configured fallback | GTD Active Info Display |
| Passat B8 TSI | 0–280 | 0–8000 | Configured fallback | Petrol Alltrack Active Info Display |
| Octavia III petrol | 0–260 | 0–8000 | 50–130 | Petrol analogue illustration |

- Tiguan: [Volkswagen user manual, edition 11.2021](https://www.volkswagen.fr/idhub/content/dam/onehub_pkw/importers/in/pdf/Tiguan-User-Manual.pdf),
  PDF pages 28 and 31, instrument/coolant figures. The English manual is delivered
  from the India importer path on the Volkswagen France host. Its metric
  illustration is the reference; it does not prove the exact cluster fitted to
  the owner's 2017 CZPA/DQ500 car. The 2016 UK brochure was also examined and
  contains a different mph presentation; its speed labels were not substituted
  for the metric scale.
- Golf: [Volkswagen Sweden brochure, March 2017](https://www.volkswagen.se/idhub/content/dam/onehub_pkw/importers/se/hjalp-och-support/broschyrer/golf/tidigare-golf/nya_golf_modellbroschyr_web_30mars.pdf),
  printed page 25 for ordinary petrol 260/8000, page 17 for GTD 280/6000.
- Passat: [Volkswagen Sweden brochure, October 2015](https://www.volkswagen.se/idhub/content/dam/onehub_pkw/importers/se/hjalp-och-support/broschyrer/passat/before-nbd/002.vw-1189-passat_21_gte_alltrack_modellbroschyrny_web.download.pdf),
  printed page 17 Alltrack cockpit photograph. Coolant endpoint numerals could
  not be confirmed, so this profile leaves them to the configured fallback.
- Octavia: [Škoda Octavia 5E owner's manual, May 2014](https://ownersmanuals.blob.core.windows.net/ownersmanuals/en/Octavia_5E/05-2014/OwnersManual.pdf),
  printed pages 29/30, figures 21/23.

These manufacturer PDFs were downloaded only into the chat's scratch directory,
rendered with pypdfium2, and the instrument illustrations inspected visually.
No manufacturer graphics, PDFs, logos or fonts were copied into the repository;
the committed catalogue contains numeric facts, source links and our metadata.
Existing OFL font licences remain unchanged.

Oil, transmission/ AWD temperature, pressure, load, gear and battery profiles
use general presentation defaults where a stock analogue scale was not evidenced.
For example, oil temperatures use −40–160 °C. These are explicitly not factory
instrument specifications. Porsche is a visual theme, not an assumption that
the connected vehicle is a Porsche. Porsche vehicle-specific ranges remain
outside this verified catalogue.

## Automatic selection and architectural boundary

`minidash_live watch --display-port COM5` resolves a profile after the existing
read-only discovery. Selection requires an explicit model generation/year,
matching manufacturer and an engine ASAM fuel family actually read by discovery.
Seed-only ECU identity, conflicting/failed engine discovery, unknown generation,
unknown fuel or unsupported market produce `default`. Generic `Golf` or `Tiguan`
without generation information do not silently select a specific variant.

Vehicle model/year may come from the existing configured/discovered identity;
this change does not introduce a universal VIN decoder or prove exact cluster
trim from an engine ASAM. European catalogue matching is the default host market;
use `--display-market other` to disable it. A known instrument variant can be
chosen explicitly with `--display-instrument-profile <catalogue-id>`; `auto`
and `default` are also accepted. This selection never chooses a diagnostic
decoder or changes vehicle requests.

Each retained serial batch begins with `MDV1|<profile-id>|<ttl-ms>`, followed by
the existing MDS1 sample frames. The allowlist and TTL (1–60000 ms) are checked on
the host and MCU. Default keepalive TTL is 3000 ms and grows for a slower update
interval, bounded to 60000 ms. Profile frames are repeated in every batch so
latest-snapshot queue replacement cannot discard startup identity. Accepted
keepalives are silent, avoiding output-buffer backpressure on the write-only
host bridge. Only rejected frames receive a console diagnostic.

The UI task applies the selection under the LVGL lock. It is volatile, survives
screen rotations, writes no NVS and expires using wrap-safe elapsed time. On
disconnect/expiry the scale returns to Tiguan; sample TTL independently hides
the needle. In manual mode the stored ranges take precedence over all profiles.
Unknown profile IDs are rejected, never parsed as arbitrary UI or ECU commands.
VIN, ECU payloads and credentials are not transmitted in this display frame.

The ESP32's CAN acquisition remains disabled. Automatic selection is implemented
through the existing **PC runtime → USB display** path, tested with synthetic
discovery and injected profiles. Direct connection of the ESP32 to a car is
still a future acquisition/identification integration; no real-car auto-detection
acceptance is claimed here.

## Modules, configuration and compatibility

- `ui_screens.c`: Focus-only draw callback, fresh-value needle, tick formatting,
  digital placement, graph visibility and selected-signal restoration on resize.
- `ui_instrument_profiles.{h,c,inc}`: bounded catalogue lookup, MDV1 parser and
  volatile profile expiry. `scripts/generate_instrument_profiles.py --check`
  detects generated-table drift against the JSON used by Python.
- `ui_model.c`: common presentation defaults derive the three instrument ranges
  from the generated default profile; all 12 canonical metrics have a range.
- `display_app.{h,c}`, `main.c`: queued profile events and read-only bench routes
  `focus <canonical-signal>` / `gauge-status`. These use the existing semantic
  Focus gate and LVGL lock rather than bypassing touch/navigation state.
- `minidash_live/instruments.py`, `display_bridge.py`, `__main__.py`: identity
  matching, profile override and repeated bounded serial frames.
- Configuration/JSON/Web editor: schema **3**, `common.gauge_auto` plus 12
  `{minimum,maximum}` pairs in canonical registry order. The binary blob appends
  one flag byte and 96 float bytes before CRC; the existing 2304-byte maximum
  still fits four pages of eight widgets. Finite bounded ranges with max > min
  are mandatory. Unknown fields and duplicate names remain rejected.

Schema 1/2 JSON and binary blobs migrate to Auto with Tiguan/default ranges;
schema 2 keeps relative rotation, while schema 1 still normalizes its old
build-only rotation to zero. Legacy settings wire v1–v4 and separate touch
calibration are unchanged. Local settings updates preserve all Focus ranges,
the mode, pages and labels. Theme/page resets preserve ranges; Reset UI resets
the mode/ranges along with common UI configuration.

Recovery: export the current JSON before downgrading. An older schema-2 firmware
cannot read a schema-3 canonical blob and will use its retained legacy/default
fallback; it cannot preserve new range fields. Reflash only the app at 0x10000;
do not erase NVS or touch calibration. The prior owner configuration was
captured before flashing and compared after migration/restoration/reboot.

## Verification

Final firmware built with the existing ESP-IDF 5.5.5 environment, ST7789 profile,
GPIO4 PWM and disabled CAN. App size: **1,892,976 bytes** in the 3 MiB partition
(40% free); SHA-256:
`ee1f8a5921e3d8fbc9c48e91d43ef98433f814f6743a8e385f4aee2ec34aa838`.
Flashed on COM5 with esptool 4.12.0 using app-only address 0x10000; hash verified.
NVS, bootloader, partition table and calibration were retained.

| Check | Actual result |
|---|---|
| Native Zig/CMake/Ninja core, JSON, backlight, instrument contracts | CTest 4/4 PASS |
| Python full suite in ESP-IDF Python environment | 191 run, 173 passed, 18 optional/platform skips |
| Instrument matcher + display bridge tests | 9/9 PASS within that suite |
| Generated catalogue table consistency | `--check` PASS |
| Node Web editor logic with generated schema-3 fixture | PASS; manual ranges, Preview/Cancel and mode checked |
| ESP32 `ui-test` | 27/27 PASS |
| Actual LVGL isolated-object selftest | PASS; fresh/stale needle, manual override, dimensions, fonts and theme restoration |
| ESP32 AP/HTTP `web-test` | 38/38 PASS, including invalid range rejection, manual range save/NVS and local edit preservation |
| Active Focus scene inspected through `gauge-status` | 44/44 checks PASS |
| Original schema-2 owner configuration migration | All existing common/platform fields preserved |
| Restore, actual reboot and NVS persistence | Full common/platform configuration comparison PASS |

Native commands: `cmake --build core/build-consolidation` and
`ctest --test-dir core/build-consolidation --output-on-failure`, with the existing
Zig 0.13 toolchain and ESP-IDF cJSON source. Host commands:
`python -m unittest discover -s tests`,
`python scripts/generate_instrument_profiles.py --check`, and
`node core/test_webui.cjs <native-generated-fixture.json>`.
Firmware command: from `firmware/`, `idf.py -B build-backlight build`.

The 44 Focus checks include RPM/speed/coolant in Volkswagen/Porsche at each of
0/90/180/270 degrees (24 cases), the other nine canonical metrics, five profile
updates, profile expiry, valid zero/sample expiry and four non-valid states.
Fresh midpoint needles reported level 500/1000 and cards 212×156 landscape or
159×208 portrait. Ordinary card views reported zero instruments. Golf GTD
changed the live Focus RPM endpoint to 6000; profile expiry returned it to 8000
and `Tiguan II TSI *`. These are **synthetic serial bench samples**, not live-car
telemetry. LVGL scene state, geometry and display stability passed; physical
readability and true vehicle recognition still require the acceptance below.

The bench runner opens pySerial with DTR/RTS false before opening COM5. It uses
the existing Settings actions to change themes/rotations, `home` before the
semantic `focus <signal>` route, then injects bounded MDS1/MDV1 frames and reads
`gauge-status`. Console requests wait for complete responses; the original
short waits were corrected after harness failures. The final complete run
passed without firmware assertion, stack canary or Guru Meditation messages.
The captured owner baseline was Terminal, rotation 0, brightness 80%, Russian,
OEM dashboard with the existing six-card Custom page, warnings and overrides;
it was restored after the tests and compared again after reboot. No synthetic
pages or manual test ranges remained saved.

## Remaining work and acceptance

1. Physical Focus readability: inspect Volkswagen/Porsche speed/RPM/coolant in
   0/90/180/270 orientations, digital labels, endpoints and pointer visibility.
   The LVGL object/geometry tests cannot prove small-screen readability. Required:
   owner visual acceptance on the TFT; adjust `ui_screens.c` only if needed.
2. Exact vehicle variant selection: record the owner's cluster identifiers,
   regional display and scale labels together with observed read-only ECU
   identity. Extend catalogue matching only with evidence that distinguishes
   petrol/diesel, GTI/R/GTD, market and generation. Acceptance: reconnecting the
   same known car selects the intended reference; unsupported variants remain
   explicit fallback/manual. Risk: model/year alone cannot determine a trim.
3. Real-car host bridge: the existing VNCI/transport and identity-probe blockers
   are described in the first-session runbook. Once resolved, capture one parked
   read-only session showing profile selection, values, sample/profile expiry
   and reconnect without changing NVS. Acceptance requires actual vehicle data,
   not these bench frames.
4. Autonomous MCU integration: a future evidenced vehicle provider should
   publish the same profile and canonical values. Keep decoding/acquisition
   separate from the UI, preserve driving lockout and TTL behaviour. Required:
   verified hardware/transport and the corresponding vehicle evidence.
5. Catalogue expansion: add Audi/Porsche/other generations and more coolant/oil
   variants when manufacturer instrument illustrations and identity criteria
   are verified. Acceptance includes provenance, generated-table check and
   positive/negative matching tests. Avoid extrapolating a rev limiter from the
   last tachometer numeral or maximum road speed from a vehicle specification.
