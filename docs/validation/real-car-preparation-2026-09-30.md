# Real-car preparation delivery — 2026-09-30

## Executive summary / boundaries

Phase A synced the existing worktree safely to published consolidation; Phase B
adds offline-tested first-session infrastructure on `feature/real-car-validation`.
The **full Windows VNCI session remains BLOCKED**, not READY: installed drivers
expose D-PDU, whose adapter is not implemented; raw CAN capture and identity
probe layouts still need evidence. No car requests, driver DLL execution,
firmware flash or new vehicle validation took place in this delivery.

Runbook: [first-real-car-session.md](first-real-car-session.md). This is the
current gate; earlier runtime runbooks remain useful evidence/history.

## Phase A and preservation

Initial local HEAD `22a346bee87f82c12943fd7a3277ea219d698d03`, branch
`feature/research-replay-reliability`, 18 changed/untracked task files previously
published remotely. Fetched all remote heads; added the full-head fetch refspec
alongside the existing limited one. Local branch `feature/repository-consolidation`
then matched **exactly** `3e5c6ca90ca7a9e0fa421c9b5d41e6fcda92b38a`, clean.
The real-car branch was created only after Phase A acceptance passed.

Preserved stash named `Preserve local published consolidation files before sync
2026-09-30` (18 files); external binary `pre-sync.patch`, ignored-file inventory
and reviewed-task snapshot under the chat's `outputs/consolidation-2026-09-30`.
Ignored `.local`, build outputs/research and firmware backups remained in place.
Other worktrees/branches and original private hardware/OEM artifacts were not
modified. No hard reset, force-push, main merge or branch deletion.
The earlier consolidation report's sync-blocked state is historical, superseded
by this successful Phase A record, rather than rewritten history.

## Actual installed VNCI path (offline inspection)

- Windows, Python64; python-can4.6.1 / can-isotp2.0.7 / udsoncan1.26.1 /
  cantools41.0.2 installed. Artifact permissions/write probe succeeds.
- HKLM32/64 PassThruSupport.04.04/05.00 inventory: no registrations observed.
  This is not proof that no alternate unregistered API exists anywhere.
- D-PDU registry root `C:/ProgramData/D-PDU API/rdf64.xml`, VW OS and OE entries.
  Both PDUAPI_VW.dll are x64 (PE machine8664); PDUConstruct/GetModuleIds/
  GetResourceIds/CreateComLogicalLink/StartComPrimitive/GetEventItem/... exports,
  **none of the required PassThru exports**. Libraries were parsed, not loaded.
- MDF27.2.0/MVCI2.2.0 describes VAS6154/VAS6154A, CAN1 HS OBD6/14 UDS/RAWCAN,
  CAN2 LS OBD8/9, plus generic J2534/Vector/SocketCAN descriptors. Configured
  resource definitions are not connected device/module/channel enumeration.
- Installed `Volkswagen/VAS6154 Driver` and `Rockway/VNCI 6154A Device Manager`
  found under Program Files(x86). No matching connected PnP device was observed
  during this inventory. Process/module/network inventory saved privately using
  existing `tools/Inspect-Vnci.ps1`; absence remains inconclusive for a network VCI.
- `j2534.py` implements normal11bit ISO15765 complete PDUs with flow-control
  filters, single-flight read and bounded cleanup. It does **not** implement raw
  passive CAN channels, generic VNCI D-PDU or DoIP. ISO15765 may transmit FC;
  it must not be advertised as passive receive-only.
- Existing engine route **7E0/7E8,500k,normal11bit** is backed by actual ODIS ECF
  and physical trace2026-09-22 in the exact CZPA profile, not guessed from defaults.
  That evidence does not prove MiniDash driver connection, or DQ500 endpoints.

No proprietary DLL/XML content was committed. Inspection output stays in ignored
`.local/real-car`, sources remain the user's installed provider files. New code
is local project implementation; no external implementation or license was copied.

## Architecture and implementation

Transport → configured ECU discovery → observed VehicleIdentity/EcuIdentity →
IdentityContext → existing VagKnowledgeProvider → SignalRegistry →
AcquisitionPlanner/Scheduler → UDS/DBC → SignalStore → SourceArbitrator → trace.
Production architecture remains unchanged; MCU UI remains separate.

| Layer / path | Implementation | Offline evidence | First-car readiness / limit |
|---|---|---|---|
| Windows VNCI/D-PDU | NOT IMPLEMENTED adapter | PE/XML inventory only | BLOCKED by ABI/module/resources/ComParams/cleanup evidence |
| J2534 ISO15765 | IMPLEMENTED generic adapter | existing transport/lifecycle tests | BLOCKED on this PC: no evidenced FunctionLibrary or connected interface |
| ISO-TP | IMPLEMENTED host socketcan; provider for J2534 | existing multiframe VirtualBus tests | Driver/physical channel acceptance missing; not MCU implementation |
| Discovery | IMPLEMENTED explicit endpoints, new strict stop-on-error option | pipeline + new mismatch/NRC tests | F190 only can observe VIN; layout evidence required for ECU identity |
| VehicleIdentity/EcuIdentity/Context | IMPLEMENTED | existing identity/evidence tests, new replay | Observed ECU fields only; static selectors do not unlock exact definitions |
| Knowledge/providers/applicability/provenance | IMPLEMENTED; 134 definitions | existing catalog/MQB/knowledge tests | Candidates != validated; exact identities required |
| Acquisition planner/scheduler | IMPLEMENTED | existing pipeline + captured replay integration | finite one-shot first-session jobs; physical bus budget unmeasured |
| UDS read 0x22 | IMPLEMENTED | new strict trace/replay/error tests | BLOCKED by transport/identity; only four evidenced targets allowed |
| SignalStore/TTL/arbitration | IMPLEMENTED | production replay candidate/store/selected-value assertion | No new car evidence; raw-only values stay null |
| Passive CAN recorder | IMPLEMENTED Linux Classical CAN | existing trace/replay tests | NOT IMPLEMENTED Windows VNCI recorder; silent hardware needs evidence |
| CLI/doctor/preflight | IMPLEMENTED | missing transport/D-PDU/bad config tests | BLOCKED reports; offline inventory never claims hardware READY |
| Raw UDS/evidence/replay | IMPLEMENTED new chronological Transport | production discovery/planner/decoder/store/arbitrator test | original private complete trace; malformed/redacted/incomplete rejected |
| Markers/button comparison | IMPLEMENTED sidecar + windows | synthetic candidate-diff test | same-host monotonic clock; Windows VNCI capture missing; no UI mapping |
| Reference annotation/statuses | IMPLEMENTED review artifact | cross-checked/rejected, never validated tests | Operator must supply real same-stage value/unit/tolerance/source |
| DTC read | NOT IMPLEMENTED production live | existing simulator only | 0x19 deliberately blocked by first-session allowlist |
| Clear/coding/adaptation/basic settings/reset/security | NOT IMPLEMENTED live product | simulator/research only | forbidden in milestone; no new write API |
| ESP32 standalone telemetry | NOT IMPLEMENTED acquisition | UI/native/Web/firmware baseline | no embedded CAN/UDS adapter added; firmware build only |

New modules/files:

- `minidash_live/validation.py`: private artifacts, bounded strict JSON, offline
  preflight/D-PDU descriptor inventory, exclusive evidence writer, global stop
  latch and existing-Transport decorator, markers, candidate comparison,
  reference cross-check annotation, review sanitization.
- `minidash_live/validation_session.py`: bounded first-session orchestration;
  reuses production discovery, knowledge/planner, EcuSession, scheduler,
  decoder, store and arbitrator. No parallel diagnostic business logic.
- `minidash_live/replay.py`: RecordedUdsTransport preserves PDU order, repeated
  DID responses, pending78, NRC, explicit timeout and transport faults; optional
  ReplayClock. Shared transport cursor across discovery/session opens.
- `minidash_live/discovery.py`: opt-in `stop_on_error`, default unchanged for
  existing research callers. New workflow stops on first mismatch/NRC.
- `tools/real_car_preflight.py`, `tools/real_car_session.py`: offline preflight;
  record/replay/marker/sanitize/compare-buttons/reference CLI.
- `tests/test_real_car_validation.py`: 23 new tests (synthetic, no hardware).
- This report/runbook and current README/index/status links.

No migration of legacy CAN schema or replay config required. CAN markers live
in a separate file. Recorded UDS format `recorded_uds_v1` is private JSONL:
metadata(schema/config/signal order/endpoint transport), timestamped engine PDUs,
discovery/plan/acquisition/evidence/summary/end. Evidence links each sample to
raw exchange, observed identities, transport IDs/address evidence, definition,
decoder and provenance. Statuses include candidate/observed/decoded/cross-checked/
validated/rejected; automatic tool output never emits validated. Existing codec
validation metadata is retained, not retroactively overwritten by a new capture.

## Safety / privacy / limitations

Default request gate is single-DID22 only. Session10/tester-present3E remain
outside this milestone despite their availability elsewhere. DTC19 is a live
gap, not a reason to expose simulator paths. All dangerous services and arbitrary
requests are rejected before wire I/O. Any NRC except pending78, malformed echo,
timeout, identity mismatch or exception halts subsequent active requests through
a shared latch; first-session jobs do not retry or auto reconnect.

Global STOP is scoped to a command. Other legacy `probe/watch` commands retain
their existing research/backoff behavior and **must not be used instead** for the
first session. Separate passive silent recorder may continue safely.

Raw VIN/identity included only in ignored `.local` original capture for replay.
The default redacted legacy CLI trace remains unchanged. Sanitize exports only
PDUs, redacts F180..F1FF positive payloads, removes metadata and exact timestamps;
manual privacy review is still required for other DIDs/raw CAN. Redacted identity
is explicitly non-replayable. Incomplete/malformed raw trace cannot be treated as
a successful replay; preserve for fault analysis. Captured transport fault replay
raises a generic recorded OSError; it does not reproduce the original driver.
Replay uses captured timestamp/clock for store/scheduler, not real bus latency
simulation; outputs have offline flags and never create new car-validation proof.

No runtime wiring from car-button byte differences to UI. OBD visibility, raw
capture adapter, steering/stalk correlation across repeats and illumination
evidence remain physical blockers. No automatic coding/candidate promotion.

## Verification

Before changes, Phase A: Python164 tests/1 Windows AF_CAN skip, native build PASS,
CTest2/2 PASS, Web PASS, repository runner PASS. Separate ESP-IDF5.5.5
reconfigure/build PASS; no flashing. Logs ignored at
`.local/acceptance/real-car-phase-a`, `firmware/.local/phase-a-build.log`.

Focused new suite: `python -m unittest tests.test_real_car_validation -v`,
23 PASS. Includes missing transport, D-PDU not ready, malformed config/JSON/trace,
unknown ECU, write rejection, sequential/pending/repeated-DID replay, NRC/timeout/
wrong echo, stop-on-identity mismatch, private-only/exclusive trace, sanitizer,
candidate button comparison and production candidate decode/store/arbitration.
The four-target integration test also uses the actual VagKnowledgeProvider and
exact publicly documented ECU identity with synthetic identification layouts;
it verifies F405/F433/16F3/F40D request order and decoded/raw-only separation.
Those fixture-only probe DIDs must never be copied into a vehicle config.

Full final acceptance and separate firmware build results are recorded in the
delivery verification section below after execution. Commands use the existing
research venv, CMake3.30.2, CTest, scoped Zig0.13 toolchain, Node and IDF5.5.5:

```powershell
& $py tools/verify_repository.py --native-build-dir core/build-consolidation --cmake C:/Espressif/tools/cmake/3.30.2/bin/cmake.exe --ctest C:/Espressif/tools/cmake/3.30.2/bin/ctest.exe --output-dir .local/acceptance/real-car-final
```

Firmware: existing Microsoft.v5.5.5.PowerShell_profile.ps1; `PYTHONUTF8=1`,
`IDF_CCACHE_ENABLE=0`, `IDF_COMPONENT_MANAGER=1`, local writable TMP/TEMP;
IDF Python `idf.py -B build-ui-platform-managed -DCCACHE_ENABLE=0 reconfigure build`.

## Remaining roadmap / readiness criteria

**P0 / driver gate:** existing Transport adapter for locally installed D-PDU only
after SDK/ABI/module/resource/ComParams evidence. Tests for ownership/callback/
cancel/cleanup/timeouts; bench enumerate/open/close without state-changing UDS.
Actual driver/library bitness and device connection must be proven.

**P0 / passive gate:** VNCI RAWCAN receive-only adapter or separate approved silent
hardware; keep Classical CAN schema1, timestamps and flags. Verify no diagnostic
requests, resource conflicts and connection-point frame visibility. Needed for
full-session READY, button correlation and passive arbitration acceptance.

**P0 / identity gate:** ODIS identity request/response layouts (not guessed DIDs),
expected exact fingerprint and transport/channel provenance. Tests for every
layout and mismatch. F190-only is initial-contact evidence, never telemetry unlock.

**P1 / engine evidence:** F405 one shot then F433/16F3/F40D raw at OFF/idle/RPM
stages. Compare ODIS reference, add formulas only from raw evidence, sanitized
regression through production registry/store/arbitrator. Acceptance is exact
identity-scoped manually reviewed evidence, not a candidate label.

**P1 / workflow:** physical acceptance of trace/markers/STOP/cleanup and capture
replay, measured request latency/bus budget; telemetry subscriptions only after
stationary success. DTC read19 requires its own production implementation/test
and review; no DTC clear in this milestone.

**P2:** DQ500/Haldex endpoints/identities; verified steering/stalk adapter to
semantic UI actions; standalone ESP32 CAN/logging; illumination evidence and
safe power hardware. Coding/adaptation requires a separately scoped milestone.

## Delivery verification (final)

Implementation commit: `d34d02a` — `feat: prepare strict read-only car evidence
and production UDS replay`. Documentation is delivered in the following scoped
commit on the same `feature/real-car-validation` branch; the final pushed SHA is
the branch tip, obtainable with `git ls-remote origin refs/heads/feature/real-car-validation`.

Final runner `.local/acceptance/real-car-final-rerun/summary.json`: **PASS**.
Python **187 total, 186 passed, 1 Windows AF_CAN skip**; native build PASS;
CTest **2/2** PASS (`portable-core`, `configuration-json`); Web checks PASS.
Separate ESP-IDF reconfigure/build **PASS**, image `0x1a36e0` bytes with 45%
of the 3 MiB app partition free; log `firmware/.local/real-car-final-build.log`.
No firmware sources changed, no device flashing or car test.

The first full run exposed a new test's assumption about TEMP: the acceptance
runner deliberately locates TEMP under `.local`, so that supposed non-private
test path was genuinely private. The test now injects an explicit outside path;
the rerun passed. An additional four-target test fixed and guards one-shot
request ordering: future jobs remain blocked until their own turn, so production
planner reconciliation cannot start another target early. Existing suites were
retained without reduced assertions/coverage.

`git diff --check` PASS. Preflight with the unfilled template: **BLOCKED**, exit1
as intended, no DLL/bus I/O; reports missing/incompatible J2534 DLL, missing
Windows passive VNCI and missing identity layouts. Inventory outputs remain
ignored. Stash remains recoverable. Factory backup SHA256 rechecked unchanged:
`CD0F30AC9491DA499EAF2167F07B5ACCF23CF6779C506110431AA766F346273E`.

## Physical actions and expected captures after blockers are resolved

Use the linked runbook's exact ordered commands. First VNCI→PC inventory, then
car parked/P/brake/ignition OFF→OBD connection→ignition ON/engine OFF identity
only→offline replay→full observed ECU identity→F405 one-shot/reference→four-target
raw/decoded capture. Next engine start/idle and controlled1500/2000/2500rpm stages;
mark brake/accelerator and steering/stalk baseline/press/release repeats only with
silent passive capture available. Optional short drive needs a separate operator;
end stopped/ignition OFF/capture end/replay/private archive. STOP on any listed
fault. Exact Windows passive start command is intentionally unavailable until
its adapter is implemented and accepted; a Linux alternative is clearly labeled.

Files: private config/inventory, identity and engine UDS JSONL+replay, reference
annotation, CAN JSONL+marker sidecar+candidate comparison, summary/errors/end.
Review VIN/serial/location privacy before making sanitized public test fixtures.
Next capture analysis: match endpoint/identity/source, compare reference and raw
samples, derive only evidenced decoder formulas, add production regression tests,
review validation scope manually, then expand to DQ500/Haldex/buttons. Forbidden:
clear/reset/security/writes/routines/actuators/flashing/coding/adaptation/basic
settings; no simulated successful automotive acceptance.
