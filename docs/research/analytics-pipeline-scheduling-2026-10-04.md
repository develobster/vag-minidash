# Analytics acquisition pressure and shared pipeline — 2026-10-04

Follow-up on `feature/engine-analytics`, base `7398f6326919e94fcb65b0d7eb4979e4daec6115`.
Implementation commit: `2a124f78971455f62d2459f5a5372dab09ce993f`.
The [initial delivery](engine-analytics-delivery-2026-10-04.md) remains a historical
snapshot. No new CAN IDs, DIDs, PIDs, source bindings or vehicle evidence were added.

## Reproduced problem

Previously separate acquisition and derived tests did not exercise simultaneous
collection of all twelve supported standard sources through the production
canonical bridge. A new native test drives supported-PID discovery, synthetic
standard OBD replies, real codecs, canonical publication and the embedded engine.
With the initial round-robin planner and full source demand, a speed gap reached
1,200 ms even with 10 ms responses. This exceeds the performance timer's existing
1 s gap limit and can invalidate a run although the last speed is still inside
its 1,500 ms source TTL. RPM/speed compete with thermal/trim/pressure background
collection; displayed priority previously changed requested periods only.

## Fix and boundaries

`acquisition.c` now examines all bounded eligible/due jobs before selecting one.
Normal jobs retain round-robin fairness. Jobs under freshness pressure take
precedence; the most overdue wins. Technical rescue budgets: speed 500 ms,
RPM 1,000 ms, other sources 75% of their source TTL. Missing sources use acquisition
start time until their first sample. This headroom is an engineering scheduling
policy, not a vehicle safety threshold or guaranteed physical update interval.

Demand, supported bitmap, exact manufacturer identity/evidence gate, fresh
passive/UDS arbitration, nominal periods, retry bounds, one-active-request rule,
QUIET behaviour and 100 ms application-request floor all remain in place. The
planner does not poll a job before it is due or enable an unsupported source.
No allocation, new queue or new per-job state was added. If real response latency
exceeds the available budget, stale/gap invalidation still wins over displaying
a timing result. No GPS or timing-accuracy claim is introduced.

`md_acq_publish_canonical()` is the shared bounded raw-source bridge used by
`vehicle_app.c` and the native integration test. It preserves arbitration,
source timestamp, TTL and quality, and skips the legacy boost slot: only the
DerivedSignalEngine produces boost in the production canonical store. Optional
source-output array has the documented raw-count capacity; null inputs are safe.
No CAN read/request is added to the derived engine itself.

## Measured synthetic results

| Native case | Maximum speed gap | Maximum RPM gap | Requests in first 20 s | Result |
|---|---:|---:|---:|---|
| Prior planner, 10 ms replies, full background collection | 1,200 ms | Not recorded | 195 | New regression test fails speed bound |
| Freshness rescue, RPM/speed visible, all physical sources background | 600 ms | 1,100 ms | 195 | All 12 sources fresh; timing/loss/reconnect PASS |
| Freshness rescue, all sources visible | 600 ms | 1,100 ms | 195 | All 12 sources fresh; timing/loss/reconnect PASS |
| All sources visible, 300 ms replies | 1,800 ms | 2,400 ms | 66 | Overloaded budget: timing result unavailable, PASS |

The first two corrected cases yield an **OBD estimate of 10.500 s** for a
synthetic linear speed profile whose underlying 0–100 transition is 10 s.
This explicitly demonstrates sampling uncertainty rather than proving accuracy.
The tests check request spacing, supported new-PID sampling, all source states,
MAP−BARO, fuel-trim sum, unsupported retard, bus loss and clean reconnect without
resurrecting the previous performance result. No real transport is opened.

## Verification and compatibility

The common runner now requires `analytics-acquisition-pipeline` as well as the
previous six native groups. Same C sources compile natively and in ESP-IDF.
Schema 4, canonical IDs, presets, threshold defaults, counter evidence policy,
NVS layout and flash-write policy are unchanged. Manufacturer profile remains NULL.

Verification: **191 Python tests, 7/7 native groups, 733 analytics assertions,
VirtualBus (27 TX), twelve C/Python replay traces, Web editor and ASan/UBSan PASS**.
All three ESP32-S3 modes build on ESP-IDF 5.5.5 with zero compiler warnings;
locked components/config remain unchanged. Standalone app is **1,832,960 B**,
**+256 B** versus 7398f63; static internal DIRAM delta **0 B**.
Engine remains 53,872 B in PSRAM. No new per-job RAM, sample allocation or NVS
write was introduced. Runtime heap/FPS/stack watermarks were not measured.

Final acceptance/build hashes and size delta are recorded in
[follow-up verification JSON](analytics-pipeline-verification-2026-10-04.json).
Repeat with the existing `tools/verify_repository.py` command in
[development](../development.md#derived-analytics-offline-gates). Pipeline executable:
`core/build-analytics/minidash-analytics-pipeline-tests`. Sanitizer checks use the
previous documented `ASAN_OPTIONS=detect_leaks=0` environment limitation.

## Remaining physical acceptance

Use the existing [read-only car runbook](../validation/engine-analytics-car-evidence-2026-10-04.md).
Record actual speed/RPM sample intervals under each screen's demand, supported
PID bitmap, response latency, bus load, timeouts, CPU/FPS and task watermarks.
Sampling pressure/slow replies may legitimately make timing unavailable; do not
raise the 1 s invalidation limit merely to produce a result. Compare timing at an
appropriate safe facility before claiming accuracy. GPIO15 TX/RX5, PSRAM, power,
UI/NVS reboot and manufacturer raw evidence still need their existing gates.
No hardware flashing or car acceptance occurred in this follow-up.
