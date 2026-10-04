# Derived Engine implementation and provenance — 2026-10-04

Integration parents: standalone `ab1c09c`, real-car/UI `bf7e8ba`; merged and pushed
as `6ae2ed4`. See [branch audit](analytics-integration-2026-10-04.md).

## Core architecture

ESP32 TWAI → bounded ISO-TP/standard OBD/evidence-gated UDS → canonical
SignalStore → portable `derived_engine.c` → fixed window statistics, counters,
performance state machines and event ring → canonical publication → existing UI,
history, warning thresholds, picker, Focus and configuration. Production code
does not use Python, PC, VNCI, phone, GPS or an external OBD dongle.

The catalogue has 92 stable signal IDs, 26 physical source slots plus the
previous boost slot, and 66 derived nodes including boost. Existing IDs 0–13 do
not move. The append-only generated catalogue and full dependency list are in
`data/analytics/signals.json`; its JSON is documentation, not an acquisition
allowlist. Graph validation rejects cycles, duplicate outputs and invalid IDs.
Visible derived demand expands to physical dependencies inside the 32-bit mask.

Derived state priority: ERROR > UNSUPPORTED > UNAVAILABLE > STALE > VALID.
Freshness follows the oldest dependency; new observations follow the newest
input, so slow BARO cannot hide MAP peaks. Nonfinite inputs never create valid
zeros. Historical trip/session values retain observed extrema internally but
their widgets inherit lost/stale source state. All quantities are observed
telemetry aggregates, not proof that unsampled peaks/events did not happen.

## Numeric and bounded semantics

Fixed 61 × one-second buckets per tracked stream aggregate every distinct sample
(sum/count/min/max). Windows: 1/5/10/30/60 seconds; boundary quantisation <1 s,
sample-weighted averages, no interpolation across unavailable intervals. Current,
delta/rate, session/trip min/max/average/count and threshold duration primitives
are reusable. Disconnect clears rolling windows and rate baselines, preserves
observed trip/session peaks. There is no allocation on each sample.

Canonical values use double to preserve all 32-bit ECU counter values. Counter
delta is computed from distinct source timestamps, never from absolute counter
as rate. Width and wrapping permission are evidence-only configuration (default
unknown/off), inaccessible through user/Web settings. Noninteger/negative or
out-of-schema counters are ERROR. Decreases normally rebase as reset. Wrapping
requires explicit evidence and an extreme-quarter boundary; even then a reset
near the modulus is fundamentally ambiguous without ECU reset/epoch evidence.
Reconnect starts a new baseline, skipping unobserved gaps. Rate is observed
window increments normalised to available elapsed time, after at least one second.

Retard contract is a non-positive crank-angle correction in degrees; positive
source conventions must be normalised by an evidenced decoder. Worst is minimum,
average is arithmetic mean of all four fresh cylinders, worst-cylinder zero
means no negative correction. `retard_events` count debounced threshold episodes,
not physical knock cycles. Load/idle classification is disabled until configured;
idle requires stationary speed and the configured RPM range, load requires the
configured calculated-load threshold. These are observation contexts, not an
ECU declaration of combustion mode.

MAP–BARO is manifold gauge pressure; manufacturer actual/requested charge pressure
are separate absolute hPa sources. Charge error is actual minus requested;
absolute error uses its magnitude. Fuel trim sum is STFT+LTFT in percentage
points, not an effective correction model or health score. IAT–ambient uses a
temperature-difference unit (F conversion multiplies by 1.8 without +32).
Voltage metrics explicitly concern control-module supply, not battery terminals.

Performance timers have WAIT/ARMED/RUNNING/COMPLETE/INVALID states, 1 km/h
hysteresis, threshold-crossing interpolation, a 1 s maximum sample gap, monotonic
source timestamps and 120 s run limit. Loss/stale/invalid speed, backwards time,
excessive deceleration or gap invalidates a run/result. Standard timers cover
0–50, 0–100, 60–100, 80–120, 100–200; X–Y is configurable and disabled initially.
OBD speed is 1 km/h quantised; request latency and bus scheduling limit timing
accuracy. No GPS accuracy claim is made. Zero-start timestamps begin at the last
observed stationary sample; result is an OBD estimate. Run count counts completed
interval measurements, so one acceleration can complete multiple intervals.

Warm-up is elapsed time from first observed running RPM to a user coolant target;
it is unavailable after missing context until trip reset. Heat-soak is an optional
stationary/running/IAT-delta proxy. Both default off. Startup/cranking voltage
minimum and spool time are intentionally omitted: present polling/context cannot
reliably establish their physical events.

Event ring: 32 records, timestamp/type/trigger/value/threshold plus freshness-masked
RPM/load/speed/MAP-boost snapshot. Overwrite and rate-limit counters are bounded.
Threshold rules are off by default; debounce/cooldown are technical filters, not
automotive safety recommendations. Signal loss and CAN errors are observer events.
All statistics are RAM only. Explicit UI configuration saves preserve NVS wear;
no sample or event writes flash.

## Sources and licence boundaries

Accessed 2026-10-04. No proprietary measuring dataset or third-party code was
imported. Maths and C implementation are original. Sources provide concepts or
standard protocol facts; they never authorise vehicle-specific acquisition.

| Primary source | Finding/use | Licence/evidence boundary |
|---|---|---|
| [CSS J1979 PID table](https://www.csselectronics.com/pages/obd2-pid-table-on-board-diagnostics-j1979) | 01/06,07 trim A×100/128−100%; 01/0F,46 temperatures A−40 °C; speed 01/0D 1 km/h; standard units | Published protocol facts, independent implementation; no DBC/dataset import |
| [python-OBD commands/decoders](https://github.com/brendan-w/python-OBD/tree/a378bdd81d58c67d08050e4244173a9a7dbda73d/obd) | Cross-check the same standard command lengths and formulas | GPL-2.0 inspected only; no code copied |
| [Ross-Tech ignition/misfire groups](https://www.ross-tech.com/vag-com/m_blocks/010-019.html), [knock groups](https://www.ross-tech.com/vag-com/m_blocks/020-029.html) | Per-cylinder counters/correction are useful with context and recognition state | Historical concept, not a CZPA UDS/DID/scaling specification |
| [Ross-Tech fuel-trim explanation](https://wiki.ross-tech.com/wiki/index.php/Fuel_Trim_Info) | Additive idle versus multiplicative elevated-speed concepts differ | No universal health score or CZPA binding derived from this |
| [Clever](https://clever-obd2.ru/offer/1367204/) | Per-cylinder retard/misfire, peaks, compact 1–8 values, DSG/Haldex product claims | Commercial description only; its default alarm and control/write functions not adopted |
| [CANchecked OBD](https://www.canchecked.de/downloads/mfd28-mfd32-mfd32s-mfa28-obd2/) | Supported-PID discovery, configurable sensors and distinct internal voltage | Concept only; no settings/DBC/control import |
| [ScanGauge performance monitor](https://www.scangauge.com/using-performance-monitor/) | Bounded saved runs and threshold performance views | Concept, no proprietary algorithms copied |
| [aivs blackCockpit](https://github.com/aivs/blackCockpit) | CAN/UDS dashboard vocabulary | Previous research pin `8175c9e`; Python/RPi runtime and unlicensed mappings not imported |
| [comma opendbc](https://github.com/commaai/opendbc), [OVMS](https://github.com/openvehicles/Open-Vehicle-Monitoring-System-3) | MQB candidate evidence and embedded polling/state separation | Existing pinned source register and MIT component boundaries preserved; no candidate promoted |

Existing exact-ECU evidence remains authoritative: J623 7E0/7E8 and coolant F405
one-byte A−40 on 5NA907115D/0003. DQ500 2104/gear 3816 and Haldex 2BF1 observations
do not establish raw encodings or physical addresses. Native manufacturer profile
stays NULL; signed scalar support adds codec capability, not a binding.

## Final verification

Six native groups PASS; analytics 733 assertions and 20,000 updates with zero
allocator calls. Engine 53,872 B in PSRAM BSS, statistics 1,584 B/stream, event
48 B, canonical store 2,952 B. Twelve C/Python replays, all 191 existing Python
checks, Web editor and three ESP32 modes PASS. See the complete
[delivery and limitations](engine-analytics-delivery-2026-10-04.md) and
[measured verification JSON](engine-analytics-verification-2026-10-04.json).
No hardware/car acceptance is claimed.
