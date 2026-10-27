# VAG platform database, drag timer and automotive screens — 27 October 2026

Base: `9397342684ffb36a125be14b001a580f2c1e05ea`, branch
`integration/standalone`. This extends the [earlier modernization](embedded-modernization-2026-10-27.md).
The request floor is now **200 ms (5 Hz aggregate)**, 500 ms under AP/backlog
pressure; ordinary CAN silence is **20 seconds**. These supersede the earlier
100 ms/30 second defaults. Sleep remains an explicit hardware-dependent opt-in.

## Implementation order and file map

| Step | Files | Result |
|---|---|---|
| 1. Shared catalogue | `data/can/vag_signals.json`, `scripts/generate_vag_can_db.py`, `vag_can_db_generated.inc`, `minidash_live/vag_can_data.py` | 33 source/experimental definitions, 19 telemetry fields, per-field endian/scale/quality/multiplexer/provenance. |
| 2. Portable decoder | `firmware/main/vag_can_db.[ch]` | PQ35/46 and MQB, Intel and DBC Motorola sawtooth extraction, signed and sign-magnitude, validity bits, ranges, stale/error states, trusted canonical merge. |
| 3. Configuration | `vag_can_db_nvs.[ch]`, `vehicle_app.[ch]`, `main.c` | Separate versioned NVS record; asynchronous stationary configuration, platform/layout, shift threshold, optional soot threshold. |
| 4. Drag timer | `drag_timer.[ch]` | IDLE → READY → RUNNING → FINISHED, 0–60/0–100/60–100, debouncing, timestamp interpolation, trusted speed, gap/source/spike/stop rejection, peaks. Reuses `performance_timer.c`. |
| 5. Acquisition | `acquisition.[ch]`, `vehicle_app.c`, `can_pipeline.[ch]` | Decode outside RX, prioritize freshness of speed/RPM and boost pressure, transfer canonical+vehicle+drag+config together through the existing atomic triple buffer. |
| 6. LVGL UI | `automotive_screens.[ch]`, `ui_screens.c`, `display_app.[ch]`, `ui_core.[ch]` | Five actual LVGL pages, cached labels/bar/shift lights, partial dirty updates, AUTO headlights/night mode, tap navigation/reset. |
| 7. Host replay | `minidash_live/vag_can_db.py`, `vag_replay.py`, `__main__.py`, `core/vag_native_api.c` | Bounded candump/schema-1 JSON replay; optional same native C drag FSM, explicitly offline estimate. |
| 8. Compatibility and checks | old generator, generated legacy adapters, `validation_json.c`, core/tests, Python tests, CMake/workflow | Single bit-coordinate source, existing canonical IDs/UI schema preserved, machine-readable quality/drag status; native, parity, NVS, concurrency, actual LVGL render tests. |

All names in the table are repository-relative, firmware files reside under
`firmware/main/`. The four requested public C modules are complete source files,
not snippets. They depend on the existing portable core and generated include;
build the repository rather than copying only the four files into another IDF project.

## Signal evidence and deliberate corrections

There is no universal verified PQ/MQB CAN matrix. The request conflicts with
pinned primary DBCs. `SOURCED` is the default; `requested_experimental` retains
all requested formulas **as candidate values** for controlled replay/comparison.
Choosing the experimental layout never promotes it to trusted telemetry.

| Signal | Requested mapping | Default sourced mapping / treatment |
|---|---|---|
| PQ RPM | 0x280, bytes 2/3 × .25 | Same little-endian field; platform name does not imply Motorola. |
| PQ pedal | 0x280 byte 5 | 0x380 bits 16..23 × .4; requested byte 5 remains experimental. |
| PQ ABS speed | 0x320 bytes 3/4 | 0x1A0 bits 17..31 × .01, replacement qualifier bit 63. |
| PQ coolant/IAT | 0x288/0x380 byte 1 | Candidate source fields, coolant status qualifier retained. |
| PQ DPF | 0x480 `(b0 & 0x21)==0x21` | Bit 5 only when mux bits 6..7 equal 3; requested predicate retained separately. |
| PQ lamps | 0x390 byte 6 bit 0 | Source matches request; candidate cosmetic AUTO theme input. |
| PQ charge pressure | 0x588 byte 4 × .01 | Reference is unknown: separate `charge_pressure_bar`, never silently gauge boost. |
| PQ gear/oil | 0x48A byte 6 / 0x588 byte 7−60 | Experimental only; no default invented oil offset or current-gear evidence. |
| MQB speed | 0x0B2 bytes 4/5 × .0075 | 0x0FD ESP_21 bits 32..47 × .01. 0x0B2 is ESP_19 wheel speeds. |
| MQB RPM/torque | 0x0FD | RPM 0x0A8 bits 48..63 × .25 with qualifier; requested 0xFD RPM/torque remain experimental. |
| MQB coolant/oil | 0x126 | 0x640 coolant byte 3 × .75−48, oil byte 2−60, with qualifiers. 0x126 is HCA_01. |
| MQB pedal | 0x121 byte 2 | Bits 12..19 × .4 with qualifier bit 20. |
| MQB gear | 0x0AD byte 1 | Current gear 0x3BE bits 20..23 with qualifier 50; 0x0AD bits 42..45 is separately named selector. |
| MQB steering | bytes 1/2 with offset | 0x086 bits 16..28 × .1, sign-magnitude bit 29 and qualifier bit 15. |

Primary source: [opendbc pinned commit c446569](https://github.com/commaai/opendbc/tree/c4465696dc2ef0d1340ae188810ac982586999a4/opendbc/dbc),
`vw_mqb.dbc` and `vw_pq.dbc`, MIT, attribution in `data/can/NOTICE`.
Record-level source/provenance lives in the JSON. Applicability to the actual
vehicle, gateway visibility, validity semantics and integrity counters/CRC still
need vehicle evidence. No checksum verification or vehicle validation is implied
by a successfully unpacked scalar.

Every new passive definition starts candidate (`~` in the UI). The existing
identity-gated, compiled `md_vehicle_profile_t` remains `.profile=NULL` by default.
Only exact matching trusted profile bindings may promote sourced passive fields;
existing verified standard OBD values can supply canonical fallback. Corrupt
qualified passive values revoke that source so active fallback is possible.
Candidate fields cannot arm drag or authorize Wi-Fi. Lamp candidates affect
cosmetics only. Fresh trusted passive samples suppress matching diagnostic polls.

No EGT/soot/DSG proprietary DIDs were invented. The telemetry API and screen slots
exist, but missing fields show `--` / `NO DPF DATA` until an evidence-backed profile
is provisioned. A petrol CZPA does not acquire a fictitious diesel DPF.
`soot-limit` computes a configurable mass headroom percentage, **not kilometers
until regeneration**. Boost uses validated MAP minus ambient; ambiguous raw
charge pressure is never relabeled as gauge boost. Shift RPM is a presentation
setting, not a detected ECU rev limiter.

## Ownership, recovery and timing

RX still runs on CPU1/priority18, owning the TWAI handle and mode commands. It
copies timestamped frames into a fixed C11 SPSC ring (512 entries), without
NVS/decoding/UI locks. Processing runs on CPU0/priority5; status/config access
retains a mutex but cannot stall physical RX. C11 atomic triple-slot ownership
publishes a coherent dashboard frame; metadata stays in internal RAM. The old
signal-only mailbox API is preserved. Five new screens use small dedicated
views instead of reserving five unused legacy field arrays.

A fixed ring is not a measured zero-loss guarantee. The 128-frame driver queue,
flash stalls, mode-change blind intervals, saturation and physical faults still
matter. `rx_driver_dropped`, `rx_ring_dropped`, `rx_epoch_dropped`, high-water and
aggregate loss counters remain observable. Verify them under full-load hardware
traffic while using UI, Wi-Fi and NVS before claiming loss-free reception.

The bus-off algorithm in this repository belongs in `vehicle_app.c`; the exact
IDF HAL observer `can_error_probe.c` remains read-only. Existing exponential
1/2/4/8/16/30 s backoff, hardware recovery to STOPPED, Listen-Only validation
(one clean second, ten fresh frames, no new errors) are preserved. Manual scan
cannot bypass it. No instant blind controller restart was added.

The **aggregate application request floor** is 200 ms, rising to 500 ms under
pressure; ISO-TP FC continues within its own transport deadline. Critical speed
and RPM receive bounded freshness priority; candidates do not suppress verified
OBD acquisition. External tester traffic still suspends polling. Existing
N_As/N_Cr and bounded NRC 0x78/0x31 behavior remain. Application requests fit
single frames; this is not a new segmented ISO-TP sender or conformance claim.

Drag requires at least three trusted <0.5 km/h samples over 500 ms; launch needs
0.8 km/h followed by an increasing sample ≥1 km/h. Crossings are interpolated
from sample timestamps; gaps >1 s, source changes, backwards time, implausible
jumps and premature stopping cancel a run. Finished results survive until the
next run/reset **in RAM**, not across reboot. Speed-source sample interval is
shown on screen and in validation JSON. Results are interpolated telemetry
estimates; 2–5 Hz diagnostic polling is not a precision motorsport timer.

## Screens and operation

New screen IDs are appended (11..15); legacy IDs 0..10, canonical signal IDs,
configuration JSON, NVS settings schema4 and saved widget bindings stay stable.
NEXT/PREV cycles six existing content pages followed by Sport, Thermals, DPF,
Drag and Night. A central tap advances; edge taps navigate; long press opens
settings. On Drag, tap RESET or use SELECT/`drag-reset`. New pages currently use
English labels; existing localized screens remain unchanged.

Sport has gauge-boost bar, eight shift lights, speed/gear/RPM/oil. Thermals uses
blue <75, green 75..110, amber >110..120, red >120 °C for trusted temperatures.
Candidates remain amber and prefixed `~`. DPF blinks REGEN only while its value
is fresh. Night keeps a black background. AUTO day/night follows fresh lamp
status, including darkening custom palettes; explicit DAY/NIGHT remains manual.
The existing esp_lvgl_port SPI DMA path uses two 16-line partial buffers; no
second full 320×240 framebuffer is allocated. Label/bar/light caches leave
unchanged content clean. The renderer interface continues to support other
backends and widgets.

Console (stationary; saves execute in processing task):

```text
vag-config
vag-platform mqb
vag-platform pq35_46
vag-layout sourced
vag-layout experimental
shift-rpm 6500
soot-limit 0
screen sport
screen thermals
screen diesel
screen drag
screen night
drag-reset
```

`soot-limit` accepts grams 0..200 (0 disables), `shift-rpm` 1000..12000.
Check the worker's `VAG profile SAVED` or `REJECTED` log; command acceptance means
queued, not committed. New settings live in independent `minidash_vag/profile`,
a versioned/checksummed 12-byte blob. Unsupported/corrupt records restore defaults
and report failure; failed saves do not apply the new running configuration.
Motion and sleep block changes. No per-frame NVS writes were added.

## Build and replay

Use Python 3.11+, C11 compiler/CMake, and ESP-IDF **5.5.5** for this pinned HAL.

```sh
python -m pip install -r requirements-transport.txt -r requirements-data.txt
python scripts/generate_vag_can_db.py --check
python scripts/generate_can_signals.py --check
cmake -S core -B .local/core -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build .local/core
ctest --test-dir .local/core --output-on-failure
python -m unittest discover -s tests -v
node --test tests/frontend/model.test.js tests/frontend/units.test.js
python -m minidash_live replay-vag run.log --platform MQB --output decoded.jsonl
python -m minidash_live replay-vag run.log --platform PQ35_46 --layout experimental
python -m minidash_live replay-vag run.log --platform MQB --native-lib .local/core/libminidash-vag-native.so --estimate-drag
```

On macOS use `.dylib` instead of `.so`. `.log` accepts compact timestamped
`candump -L`: `(1.000000) can0 0FD#0000000070170000` or the existing schema-1 JSON
CAN trace. Other log dialects must be converted explicitly. Input bounds: 64 MiB,
100000 frames by default (max1000000), 1024 chars/line, monotonic duration ≤24 h.
Replay opens no transport. Output is candidate/offline, never vehicle validation;
`--estimate-drag` requires the host-only C library and labels every result as an
offline estimate. It never links into ESP firmware. Native C/Python decoders use
the same generated matrix, with independent golden-vector tests.

`core.py` is the old simulator/recorder, not a CAN parser, so decoding belongs in
`minidash_live` and is not duplicated there. The old generated adapter is derived
from the new JSON; `platforms.json` is now a compatibility manifest only. After
editing the catalogue, run both generators without `--check` and commit outputs.
`minidash_live.configurator` continues to serve the existing host UI; this task
adds CAN replay and embedded pages, not five mirrored browser dashboard screens.

For actual LVGL software-render checks, checkout LVGL v9.5.0 and pass
`-DMD_LVGL_SOURCE_DIR=/absolute/path/to/lvgl`; optional cJSON v1.7.19 uses
`-DMD_JSON_SOURCE_DIR=/absolute/path/to/cJSON`. CI renders all five pages at
320×240 and 240×320, checks bounds, black Night background, zero flush for
unchanged frames and less than half-screen invalidation for one changed metric.
It uploads only synthetic PNG previews, never firmware binaries or vehicle logs.

Inside ESP-IDF 5.5.5:

```sh
cd firmware
idf.py -B build-vag -D SDKCONFIG=sdkconfig.vag.generated -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.standalone' build
idf.py -B build-vag -D SDKCONFIG=sdkconfig.vag.generated -p COM3 flash monitor
```

Replace COM3 with the actual device port. Use `sdkconfig.listen-only` for passive
acquisition without diagnostic TX. Preserve the factory backup and NVS; do not
run erase-flash. No physical flash is performed by the CI or this task.

Sleep overlay (append its filename to SDKCONFIG_DEFAULTS and use fresh SDKCONFIG):

```ini
CONFIG_MINIDASH_DEEP_SLEEP=y
CONFIG_MINIDASH_CAN_SLEEP_SECONDS=20
CONFIG_MINIDASH_WAKE_GPIO=-1
CONFIG_MINIDASH_WAKE_SECONDS=300
```

Timer wake is the fallback. Provision a free RTC-capable GPIO and correct wake
level only after checking the actual ignition/transceiver wiring. TWAI cannot
receive in deep sleep; raw RX low-level wake needs a powered transceiver and
may lose initial frames. Shutdown finishes configuration saves, blanks PWM,
sends SLPIN and stops the controller; active wake/resumed traffic aborts entry.
Wi-Fi remains manually opened only at fresh trusted zero speed or explicit
service boot, and RF shuts down on movement or loss of parked evidence.

## Validation and remaining acceptance

Local checks and CI results are recorded below after completion. Local runtime
has Apple Clang and Python 3.11; ESP-IDF/CMake are unavailable and HTTP socket bind
is sandbox-restricted. Target builds and full HTTP tests therefore run in CI.

Remaining priorities:

1. Vehicle identity and synchronized reference traces for each platform/ECU:
   validate IDs, qualifiers/scales/CRC, OBD gateway availability, petrol/diesel
   applicability. Acceptance: reference agreement and correct stale/error
   behavior, then provision only evidenced passive bindings. No universal matrix
   activation before this evidence.
2. Physical traffic generator + ESP32-S3 under full load, NVS and UI/AP activity:
   acceptance zero reported RX losses in a defined bounded soak, measured queue
   headroom, no bus-off recovery transmit before listen validation, no UI stalls.
3. Hardware sleep/wake and supply-current test: measure 20 s policy, wake input
   stability, transceiver behavior, backlight/SLPIN, restart on CAN, sleep current,
   Wi-Fi RF stop and temperature. Risk: board/regulator/transceiver power dominates
   ESP deep-sleep current; wiring is required evidence.
4. Parked display/touch test for both controllers/orientations and long strings;
   confirm page cycling, RESET, NVS return page, AUTO headlights, threshold bands
   and DMA completion. Software previews do not prove physical touch or SPI timing.
5. Trusted high-rate speed vs independent reference for drag: confirm timestamp
   latency, launch/stop behavior and uncertainty before any real acceleration
   evaluation. No road test or precision-timing certification is claimed.

### Local verification before target CI

- Apple Clang: 16 native executable groups passed (portable sources compiled
  with `-std=c11 -Wall -Wextra -Werror`), including 100000 concurrent ring and
  coherent whole-dashboard mailbox transfers, new NVS corruption/failure tests,
  qualified decoding, drag edge cases, acquisition/transport and existing power.
- Python 3.11.14: 241 tests discovered; 213 passed, 20 optional/platform skips,
  eight HTTP-server tests blocked by sandbox socket-bind permissions. Those are
  environment errors, not reported as passing tests; full run is delegated to CI.
- New parity tests passed: 4000 randomized frames across two platforms × two
  layouts, all 19 field qualities/values compared against native C, plus
  independent golden vectors, strict candump and native synthetic drag replay.
- Both generated-source consistency checks passed; ten existing JS contracts passed.
- Synthetic scheduler: 98 application requests/20 s, speed max gap600 ms and
  RPM1200 ms with 10 ms ECU response; at 300 ms response, speed900/RPM1500 ms.
  An ECU exceeding its deadline cannot produce a timing result. These are
  bounded simulated workloads, not CAN hardware throughput measurements.

Target/actual LVGL CI results will be added after the remote run completes.

### First remote target pass

Run [40542662692](https://github.com/develobster/vag-minidash/actions/runs/40542662692)
built all three ESP-IDF 5.5.5 variants successfully. Eighteen of nineteen native
CTest groups passed. The new LVGL unchanged-frame assertion failed after the
first screen-load refresh; startup layout settling and diagnostic output were
added before measuring application invalidation, with a bounded quiescence
assertion. A follow-up run must confirm the cause and full host suite. The full
Drag RESET hit box now takes precedence over the generic right-edge navigation
zone; stationary debounce counters saturate instead of growing indefinitely.
Local clang-tidy on decoder/drag/acquisition/mailbox and six ASan/UBSan groups
also passed before the local execution environment became unavailable.

### Dirty-update regression found by real LVGL

The second run's diagnostics isolated the unchanged-frame flush to Drag, rather
than startup layout: it wrote the generic source header and then the drag status
on every tick. Updating only the page's own source line removes both invalidations.
The bounded startup-settle check is retained; it also requires quiescence without
application updates. The new renderer test therefore verifies both text caches
and the absence of repeated writes to a shared label.

RPM/boost peak tracking also observes trusted updates between speed frames; a
regression test checks an intermediate peak and rejects a larger candidate.
This matters when speed comes from slower 2–5 Hz diagnostic polling.
