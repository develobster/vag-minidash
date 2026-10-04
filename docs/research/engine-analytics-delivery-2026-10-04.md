# MiniDash Derived Metrics / Engine Analytics delivery — 2026-10-04

Branch: `feature/engine-analytics`. Integration merge: `6ae2ed4`; first engine/UI
commit: `1a33dc2`; rollover/semantic fix `c1211cf`; shared replay `1198d6a2`.
The final documentation commit follows on this branch;
use `git rev-parse HEAD` or the delivery message for the final full SHA. Main and
existing branches were not modified, deleted or force-pushed.

## Outcome and preserved baseline

This is an implemented ESP32 feature, not a Python-dependent roadmap. Production:
car → OBD-II → MiniDash hardware → ESP32-S3 TWAI/CAN/ISO-TP/UDS → canonical
SignalStore → DerivedSignalEngine → windows/events/session/trip → canonical UI.
Python is an offline oracle only. The catalogue now has 92 stable canonical IDs:
26 physical sources and 66 derived nodes, including the refactored existing
`engine.boost` (65 new derived IDs). No guessed vehicle CAN/DID/scaling was added.

All relevant branches were compared by ancestry/content. Latest standalone
`ab1c09c` was not the complete integration base: real-car-validation `bf7e8ba`
contained seven diverged commits with newer UI/bench/validation work. Merge
`6ae2ed4` preserves both histories. [Full branch audit](analytics-integration-2026-10-04.md)
details conflict decisions. Preserved: MQB knowledge/provenance, CAN/UDS replay,
read-only validation tools, brightness PWM, rotation, eleven themes/CRT startup,
Focus instruments/vehicle profiles, all screens, fixed overrides and Custom pages.
GPIO4 remains the physically accepted TFT backlight. Proposed CAN TX was moved
to GPIO15/RX5 to avoid the electrical conflict; new wiring is not hardware accepted.

## Research and architecture

Studied acquisition/UI SignalStores, native planner/transport/codecs/identity gates,
canonical registry, history/warnings, Performance/Temperatures/Drivetrain/Diagnostics/
Custom/Focus, ECU evidence profiles, MQB candidates and prior CZPA/DQ500/Haldex work.
External primary-source findings and licence boundaries are in the
[design/provenance register](derived-engine-design-2026-10-04.md): Clever,
CANchecked, ScanGauge, Ross-Tech/VCDS measuring concepts, standard CSS PID facts,
python-OBD cross-check, MQB/open-source dashboards, opendbc/OVMS. No proprietary
dataset or third-party implementation was copied. Historical measuring groups
and internet IDs never become production bindings.

The portable C engine consumes only canonical records. Graph validation prevents
cycles/duplicates/invalid IDs. Derived TTL follows the oldest input; observations
follow the newest changed input. ERROR/UNSUPPORTED/UNAVAILABLE/STALE propagate,
and values are hidden when unavailable. The planner recursively expands derived
widget demand to raw dependencies without shifting IDs beyond the 32-bit mask.
Background standard collection maintains observed peaks; speed gets visible
priority, preserving the existing 100 ms global request floor. Nominal speed
interval is 200 ms but bus scheduling may slow it; gaps >1 s invalidate timers.

`window_statistics` has 61 fixed one-second buckets, reusable current/min/max/mean/
delta/rate/count/peak/time-above/below/session/trip primitives and 1/5/10/30/60 s
windows. Averages are sample weighted, edges quantized <1 s. Window sum/extrema
use float storage for bounded memory; current/canonical counters use double and
counter deltas use integer arithmetic. Counts and source counters are not averaged
into a rate. Bucket continuity/event windows are tested across millis rollover.
No allocator calls occur in the sample path. Config/Web JSON allocation occurs
only outside that path. No sample/event/statistic writes NVS.

## Raw sources and acquisition status

Twelve standard OBD sources are codec-ready and supported-PID gated; their
availability/accuracy still needs testing on this car. New standard additions:
01/0F IAT, 01/46 ambient air, 01/06 STFT B1, 01/07 LTFT B1. Existing standard
RPM/speed/load/coolant/oil/MAP/BARO/control-module voltage remain supported.
Manufacturer/candidate sources remain disabled; their registration grants no
acquisition permission. Native manufacturer profile is still NULL.

| Raw canonical source | Unit | Acquisition policy |
|---|---|---|
| `engine.rpm` | rpm | 01/0C |
| `vehicle.speed` | km/h | 01/0D |
| `engine.coolant_temperature` | degC | 01/05 |
| `engine.oil_temperature` | degC | 01/5C |
| `engine.load` | % | 01/04 |
| `engine.charge_pressure_actual` | hPa | Candidate / disabled |
| `engine.ambient_pressure` | hPa | 01/33 |
| `transmission.oil_temperature` | degC | Candidate / disabled |
| `transmission.gear` | enum | Candidate / disabled |
| `awd.clutch_temperature` | degC | Candidate / disabled |
| `vehicle.battery_voltage` | V | Candidate / disabled |
| `engine.manifold_absolute_pressure` | hPa | 01/0B |
| `engine.control_module_voltage` | V | 01/42 |
| `engine.intake_air_temperature` | degC | 01/0F |
| `vehicle.ambient_air_temperature` | degC | 01/46 |
| `engine.fuel_trim_short_b1` | % | 01/06 |
| `engine.fuel_trim_long_b1` | % | 01/07 |
| `engine.ignition_retard_cyl1` | deg | Candidate / disabled |
| `engine.ignition_retard_cyl2` | deg | Candidate / disabled |
| `engine.ignition_retard_cyl3` | deg | Candidate / disabled |
| `engine.ignition_retard_cyl4` | deg | Candidate / disabled |
| `engine.misfire_counter_cyl1` | count | Candidate / disabled |
| `engine.misfire_counter_cyl2` | count | Candidate / disabled |
| `engine.misfire_counter_cyl3` | count | Candidate / disabled |
| `engine.misfire_counter_cyl4` | count | Candidate / disabled |
| `engine.charge_pressure_requested` | hPa | Candidate / disabled |

Only exact existing J623 evidence confirms 7E0/7E8 and F405 one-byte A−40 coolant
for 5NA907115D/0003; it is not newly enabled in native production. DQ500 2104 and
3816, Haldex 2BF1/028D/2BE4/pump/status are measuring observations with insufficient
native raw/address evidence. No new passive CAN ID or CZPA retard/misfire/requested
charge DID is claimed. Battery voltage is separate from ECU module voltage.

## Complete derived catalogue

All rows are implemented in embedded C and bindable by any widget, picker,
warning rule, Focus or Web page. **Standard PID gate** means usable now in
software when those physical PIDs are actually supported/fresh; it is not car
acceptance. **Evidence pending** means downstream logic is ready/tested but a
manufacturer source is unavailable. Optional rules and idle/load/warmup/heat-soak/
X–Y contexts additionally need user configuration; they default disabled.

| Canonical derived metric | Unit | Operation | Readiness | Dependencies |
|---|---|---|---|---|
| `engine.boost` | hPa | SUB | Standard PID gate | `engine.manifold_absolute_pressure`, `engine.ambient_pressure` |
| `engine.retard_worst` | deg | MIN4 | Evidence pending | `engine.ignition_retard_cyl1`, `engine.ignition_retard_cyl2`, `engine.ignition_retard_cyl3`, `engine.ignition_retard_cyl4` |
| `engine.retard_average` | deg | AVG4 | Evidence pending | `engine.ignition_retard_cyl1`, `engine.ignition_retard_cyl2`, `engine.ignition_retard_cyl3`, `engine.ignition_retard_cyl4` |
| `engine.retard_worst_cylinder` | cyl | WORST4 | Evidence pending | `engine.ignition_retard_cyl1`, `engine.ignition_retard_cyl2`, `engine.ignition_retard_cyl3`, `engine.ignition_retard_cyl4` |
| `session.retard_events` | count | KNOCK_COUNT | Evidence pending | `engine.retard_worst` |
| `engine.retard_events_per_min` | 1/min | KNOCK_RATE | Evidence pending | `engine.retard_worst` |
| `engine.retard_load_event_peak` | deg | LOAD_PEAK | Evidence pending | `engine.retard_worst`, `engine.load` |
| `trip.retard_worst` | deg | TRIP_MIN | Evidence pending | `engine.retard_worst` |
| `session.retard_worst` | deg | SESSION_MIN | Evidence pending | `engine.retard_worst` |
| `engine.misfire_total` | count | SUM4 | Evidence pending | `engine.misfire_counter_cyl1`, `engine.misfire_counter_cyl2`, `engine.misfire_counter_cyl3`, `engine.misfire_counter_cyl4` |
| `engine.misfire_delta` | count | MISFIRE_DELTA | Evidence pending | `engine.misfire_counter_cyl1`, `engine.misfire_counter_cyl2`, `engine.misfire_counter_cyl3`, `engine.misfire_counter_cyl4` |
| `engine.misfires_per_min` | 1/min | MISFIRE_RATE | Evidence pending | `engine.misfire_counter_cyl1`, `engine.misfire_counter_cyl2`, `engine.misfire_counter_cyl3`, `engine.misfire_counter_cyl4` |
| `trip.misfires` | count | MISFIRE_TRIP | Evidence pending | `engine.misfire_counter_cyl1`, `engine.misfire_counter_cyl2`, `engine.misfire_counter_cyl3`, `engine.misfire_counter_cyl4` |
| `session.misfires` | count | MISFIRE_SESSION | Evidence pending | `engine.misfire_counter_cyl1`, `engine.misfire_counter_cyl2`, `engine.misfire_counter_cyl3`, `engine.misfire_counter_cyl4` |
| `engine.misfire_worst_cylinder` | cyl | MISFIRE_CYL | Evidence pending | `engine.misfire_counter_cyl1`, `engine.misfire_counter_cyl2`, `engine.misfire_counter_cyl3`, `engine.misfire_counter_cyl4` |
| `trip.misfires_idle_observed` | count | MISFIRE_IDLE | Evidence pending | `engine.misfire_total`, `engine.rpm`, `vehicle.speed` |
| `trip.misfires_load_observed` | count | MISFIRE_LOAD | Evidence pending | `engine.misfire_total`, `engine.load` |
| `engine.fuel_trim_sum_b1` | % | ADD | Standard PID gate | `engine.fuel_trim_short_b1`, `engine.fuel_trim_long_b1` |
| `engine.fuel_trim_average_10s` | % | ROLL_AVG | Standard PID gate | `engine.fuel_trim_sum_b1` |
| `engine.fuel_trim_min_10s` | % | ROLL_MIN | Standard PID gate | `engine.fuel_trim_sum_b1` |
| `engine.fuel_trim_max_10s` | % | ROLL_MAX | Standard PID gate | `engine.fuel_trim_sum_b1` |
| `engine.fuel_trim_idle` | % | IDLE_VALUE | Standard PID gate | `engine.fuel_trim_sum_b1`, `engine.rpm`, `vehicle.speed` |
| `engine.fuel_trim_load` | % | LOAD_VALUE | Standard PID gate | `engine.fuel_trim_sum_b1`, `engine.load` |
| `engine.charge_pressure_error` | hPa | SUB | Evidence pending | `engine.charge_pressure_actual`, `engine.charge_pressure_requested` |
| `engine.charge_pressure_abs_error` | hPa | ABS | Evidence pending | `engine.charge_pressure_error` |
| `trip.boost_peak` | hPa | TRIP_MAX | Standard PID gate | `engine.boost` |
| `engine.boost_average_5s` | hPa | ROLL_AVG | Standard PID gate | `engine.boost` |
| `session.overboost_events` | count | RULE_COUNT | Evidence pending | `engine.charge_pressure_error` |
| `session.underboost_events` | count | RULE_COUNT | Evidence pending | `engine.charge_pressure_error` |
| `trip.coolant_peak` | degC | TRIP_MAX | Standard PID gate | `engine.coolant_temperature` |
| `trip.oil_peak` | degC | TRIP_MAX | Standard PID gate | `engine.oil_temperature` |
| `trip.iat_peak` | degC | TRIP_MAX | Standard PID gate | `engine.intake_air_temperature` |
| `trip.dsg_oil_peak` | degC | TRIP_MAX | Evidence pending | `transmission.oil_temperature` |
| `trip.haldex_clutch_peak` | degC | TRIP_MAX | Evidence pending | `awd.clutch_temperature` |
| `engine.iat_ambient_delta` | degC | SUB | Standard PID gate | `engine.intake_air_temperature`, `vehicle.ambient_air_temperature` |
| `engine.coolant_rise_rate` | degC/s | RATE | Standard PID gate | `engine.coolant_temperature` |
| `engine.oil_rise_rate` | degC/s | RATE | Standard PID gate | `engine.oil_temperature` |
| `engine.iat_rise_rate` | degC/s | RATE | Standard PID gate | `engine.intake_air_temperature` |
| `trip.coolant_warmup_time` | s | WARMUP | Standard PID gate | `engine.coolant_temperature`, `engine.rpm` |
| `engine.heat_soak_proxy` | status | HEAT_SOAK | Standard PID gate | `engine.iat_ambient_delta`, `vehicle.speed`, `engine.rpm` |
| `trip.coolant_high_seconds` | s | RULE_TIME | Standard PID gate | `engine.coolant_temperature` |
| `trip.oil_high_seconds` | s | RULE_TIME | Standard PID gate | `engine.oil_temperature` |
| `trip.dsg_oil_high_seconds` | s | RULE_TIME | Evidence pending | `transmission.oil_temperature` |
| `trip.ecu_voltage_min` | V | TRIP_MIN | Standard PID gate | `engine.control_module_voltage` |
| `trip.ecu_voltage_max` | V | TRIP_MAX | Standard PID gate | `engine.control_module_voltage` |
| `engine.ecu_voltage_average_5s` | V | ROLL_AVG | Standard PID gate | `engine.control_module_voltage` |
| `session.ecu_voltage_drop_events` | count | RULE_COUNT | Standard PID gate | `engine.control_module_voltage` |
| `trip.ecu_voltage_low_seconds` | s | RULE_TIME | Standard PID gate | `engine.control_module_voltage` |
| `session.rpm_max` | rpm | SESSION_MAX | Standard PID gate | `engine.rpm` |
| `session.speed_max` | km/h | SESSION_MAX | Standard PID gate | `vehicle.speed` |
| `session.boost_max` | hPa | SESSION_MAX | Standard PID gate | `engine.boost` |
| `trip.rpm_max` | rpm | TRIP_MAX | Standard PID gate | `engine.rpm` |
| `trip.speed_max` | km/h | TRIP_MAX | Standard PID gate | `vehicle.speed` |
| `trip.boost_max` | hPa | TRIP_MAX | Standard PID gate | `engine.boost` |
| `performance.accel_0_50` | s | PERFORMANCE | Standard PID gate | `vehicle.speed` |
| `performance.accel_0_100` | s | PERFORMANCE | Standard PID gate | `vehicle.speed` |
| `performance.accel_60_100` | s | PERFORMANCE | Standard PID gate | `vehicle.speed` |
| `performance.accel_80_120` | s | PERFORMANCE | Standard PID gate | `vehicle.speed` |
| `performance.accel_100_200` | s | PERFORMANCE | Standard PID gate | `vehicle.speed` |
| `performance.accel_x_y` | s | PERFORMANCE | Standard PID gate | `vehicle.speed` |
| `performance.acceleration_duration` | s | RUN_DURATION | Standard PID gate | `vehicle.speed` |
| `trip.performance_runs` | count | PERF_COUNT | Standard PID gate | `vehicle.speed` |
| `trip.retard_events` | count | TRIP_KNOCK_COUNT | Evidence pending | `engine.retard_worst` |
| `session.analytics_events` | count | EVENT_COUNT | Observer counters | engine observer state |
| `session.signal_loss_events` | count | LOST_COUNT | Observer counters | engine observer state |
| `session.can_error_events` | count | CAN_ERROR_COUNT | Observer counters | engine observer state |

Retard uses non-positive correction degrees; worst is the minimum, cylinder zero
means no negative correction. Debounced retard events are a useful knock-related
proxy, not counted physical knock cycles. Misfire width/reset/wrap metadata is
unknown by default, unavailable to Web edits and only supplied in the synthetic
C harness for tests. Decreases rebase; explicitly evidenced extreme-boundary wrap
is supported, but reset near modulus still requires ECU epoch evidence. Loss/
reconnect starts a new counter/rate baseline, skipping unobserved increments.
Trip totals are observed increments, not a claim of full lifetime coverage.

MAP−BARO is manifold gauge pressure; requested/actual charge pressure is separate
absolute hPa. Error is actual−requested. STFT+LTFT is percentage-point sum, not
universal effective correction/health. Temperature delta has correct °F-difference
conversion. Heat soak is an optional stationary/running/IAT-delta proxy. Warm-up
starts at first observed running RPM and a user target, not assumed ignition time.
It becomes unavailable after missing context until reset. Cranking voltage minimum,
spool time, Haldex torque/pressure/activity score, GPS and engine-health scores are
intentionally not supplied without reliable event/source semantics.

## UI, events, session/trip and use

Six eight-card presets: ENGINE, KNOCK/RETARD, MISFIRE, BOOST, THERMAL,
TRIP/PERFORMANCE. In Settings → Wi-Fi configuration → CHANGE, open the existing
local editor; choose a preset to replace the active Custom page, then edit any
widget and Preview/Save. Candidate fields display state placeholders, not zeros.
Existing Numeric/Bar/Status/Mini graph types are reused; compact labels fit the
8-card layout. Fixed Performance/Temperatures/Drivetrain/Diagnostics screens keep
independent overrides. Picker uses only 12 LVGL slots paged over all 92 IDs.
Focus shows value/history/min/max and state/source. New labels currently use
English canonical captions in the RU font-capable UI; full translation remains
polish. Physical 320×240 readability/FPS is not claimed from native logic tests.

Standard warning thresholds can choose any metric and high/low direction, e.g.
retard low, misfires/min high or ECU voltage low. Eight optional event rules cover
charge over/under, coolant/oil/DSG high, module voltage low, retard and trim
threshold episodes. All new defaults are off; no universal safety limits were
invented. Configurable idle/load/warm-up/heat-soak/X–Y settings are also off.

Events: RETARD, MISFIRE, OVERBOOST, UNDERBOOST, HIGH_COOLANT, HIGH_OIL, HIGH_DSG,
LOW_VOLTAGE, SIGNAL_LOST, CAN_ERROR, ACCELERATION_RUN, TRIM. Ring capacity 32;
timestamp/type/trigger/value/threshold plus fresh RPM/load/speed/MAP-boost snapshot.
Overwrite/rate-limit counts are exposed; generic per-signal record spacing is
100 ms, CAN errors 1 s, threshold rules have configurable debounce/cooldown.
Named event directions/signs are validated: overboost high with nonnegative
threshold; underboost low with nonpositive threshold; retard low/nonpositive.
Generic trim episodes and UI warning rules can choose either direction.
`GET /api/analytics` returns the bounded ring with null missing context. Counts
of observed errors/episodes and retained records have different meanings.

Session/trip offer RPM/speed/boost peaks, thermal peaks, ECU voltage minimum,
worst retard, observed misfires/retard episodes and completed timing intervals.
Reset trip or session in the editor resets RAM only; no ECU operation or flash
write. Reset session also resets trip and event ring. Changing analytics settings
starts a new engine session (including Preview/Cancel changes); ordinary UI edits
preserve settings. Persistence is limited to explicit configuration Save.

Schema 4 keeps old IDs and reads old settings/schema 1/2 plus both historical
schema-3 12/14 range variants. CRC/length rejection is transactional. Four pages,
92 gauge ranges and optional analytics settings fit the bounded binary format;
compact HTTP is ≤16 KiB, browser pretty-import limit 32 KiB. Existing calibration,
NVS keys, AP guards/revision/driving lockout and themes remain intact.

Performance state machines: WAIT/ARMED/RUNNING/COMPLETE/INVALID, 1 km/h hysteresis,
interpolated crossings, monotonic samples, maximum gap 1 s/run 120 s, loss/stale/
deceleration invalidation. No result is available with stale/lost speed. Zero
starts at last observed stationary sample; 1 km/h quantization/OBD latency limits
accuracy. A single acceleration may complete several interval timers, so
`performance_runs` counts interval measurements, not unique journeys/launches.

## Verification actually performed

- Full existing Python suite: **191 PASS**, no skips.
- Native shared C: **6/6 groups PASS**: derived, acquisition, portable UI/config,
  JSON, instrument profiles and backlight contract. **733 analytics assertions**;
  20,000 ticks with malloc/calloc/realloc/free wrappers: **zero calls**.
- Nine acquisition scenarios plus **50,000 malformed CAN frames**. New standard
  PID codecs and signed UDS scalar are tested on a clearly synthetic profile.
- Existing firmware-C VirtualBus/replay integration: **PASS, 27 TX frames**;
  VIN/DTC ISO-TP/loss/quiet/fault/reconnect and exact replay preserved.
- Twelve canonical synthetic traces: normal, knock burst, repeated cylinder
  retard, misfire, reset/overflow, overboost, underboost, heat soak/warm-up,
  voltage sag, loss/reconnect, stale/unsupported and acceleration/loss. **66 C
  result assertions, 47 independent Python comparisons, nine malformed inputs**.
- New editor logic: six presets, new bindings, rule/warning direction, imports
  from both schema-3 variants and trip/session reset dispatch **PASS**.
- ASan/UBSan: **6 native groups and 12 replays PASS**. LeakSanitizer initially
  failed to enumerate sandbox /proc threads; it was disabled for the final
  address/undefined checks, not misreported as a leak-check PASS.
- Cycle/duplicate/malformed graph, state propagation, invalid numbers, 32-bit
  counter precision/reset/overflow, rolling windows/millis rollover, event
  debounce/cooldown/rate limiting/ring overwrite, trip reset/reconnect, timer
  hysteresis/interpolation/loss, UI warnings/delta units, old CRC records and
  forbidden evidence-only Web fields are checked automatically.
- ESP-IDF **5.5.5 / ESP32-S3**, hash-pinned LVGL/port/LCD components: **three modes
  PASS with zero compiler warnings**, lockfile unchanged. Standalone/listen
  disable host injection; the preserved display-development default uses it.
- `git diff --check` PASS. No hardware flashing, car test, live HTTP/NVS reboot,
  speed accuracy, runtime heap/FPS/task watermark or physical glyph acceptance
  was performed. Compiled device selftests remain for the bench gate.

Exact build hashes/config hashes/linker measurements and test summaries:
[verification JSON](engine-analytics-verification-2026-10-04.json). Commands:
[developer guide](../development.md#derived-analytics-offline-gates).

## Memory / flash impact

Comparison is against the **same merged standalone baseline 6ae2ed4**, not an
older UI build. Final standalone app: **1,832,704 bytes**, baseline 1,802,384:
**+30,320 bytes (29.61 KiB)**. App partition 3 MiB, remaining **1,313,024 bytes**.
Internal linked DIRAM increases **11,872 bytes** (data +64, BSS +11,808);
linked DIRAM remaining 70,393 bytes is not a measured free runtime heap.
Engine PSRAM BSS: **53,872 bytes**; total external BSS 67,884, including 14,012
SDK bytes moved by the enabled external-BSS option. Thirty statistic streams
use 1,584 bytes each; 32 × 48-byte events; canonical store 2,952 bytes. Vehicle
RTOS stack increases 6,144→8,192 bytes (+2 KiB). Every structure stays bounded;
no allocation/sample or NVS/sample. Existing PSRAM board setup is required.

## Next car/bench work and remaining gaps

The [exact read-only evidence procedure](../validation/engine-analytics-car-evidence-2026-10-04.md)
includes ECU fingerprints, supported standard PIDs, separate ODIS groups, recording
stages/markers, raw/scaling/counter semantics and promotion acceptance criteria.
First verify revised transceiver pins and current PWM/PSRAM/UI/NVS on bench;
then listen-only TX=0 and standard capability/freshness/reference collection.
Only afterwards capture licensed ODIS/VNCI evidence for retard/misfires, requested/
actual charge, IAT, DSG temperature/selected-current gear and Haldex values.

No coding/adaptation/security access/RoutineControl/ECU writes/DTC clear or
unknown-address raw probes were added. Manufacturer polling remains identity+
evidence gated. Standalone production never relies on PC/Python/VNCI/ODIS/phone.
The concrete outstanding blockers are physical power/CAN/UI acceptance, actual
speed sampling/accuracy, licensed VNCI transport/logging, and raw source evidence;
they cannot be replaced by synthetic replay.
