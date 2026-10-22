# VAG diagnostics: evidence, transport fixes and discovery

Snapshot: **2026-10-22**. Base: `integration/standalone` at `d9cca9d`.
Work branch: `feature/vag-diagnostic-discovery`, explicitly requested for this task;
this overrides the repository's routine-continuation branch instruction.
Target: Tiguan II 2017 / CZPA / DQ500 / Gen5, ESP32-S3, OBD 6/14,
GPIO15 TX / GPIO5 RX, classical CAN 500 kbit/s.

## Result and root-cause boundary

The exact electrical cause of **canerr18** remains **unproven**. The recorded
TX=3 / failed=3 / BUS=115 / ECU=0 cannot establish gateway filtering, wrong DID,
a missing ACK, or a broken transceiver by itself. In that revision TX included
queue acceptance. A CAN ACK is independent of whether the target ECU accepts a
UDS service, and can come from another node. RX proves reception, not transmission.
The baseline already corrected queue-vs-controller TX success, made 87.5% sample
point explicit, added error-class capture, bounded startup retries and NVS incident
storage. Those changes are not newly attributed to this task.

Historical **canfix19** USB loopback 20/20, injected ACK failure and NVS roundtrip
are documented in [the original report](../hardware/can-deep-analysis-2026-10-19.md).
They were not repeated here and do not validate operation on the loaded car bus.
No new car session or trace was available. The GitHub baseline and issue/PR list
were checked; no later vehicle acceptance was found in the inspected sources.

Plausible remaining physical causes include wiring/contact/ground or supply under
load, bus timing/edge quality, or transceiver TX behavior. These are hypotheses,
not instructions to replace hardware. Padding, DID and gateway hypotheses become
relevant only after a controller-confirmed transmission. The next test therefore
records independent RX, controller TX result, error classes and bounded responses
in that order. [Car test plan](../validation/vag-diagnostics-car-test-plan.md).

## Evidence method and source register

Inspected pinned code, tests where present, recent commit metadata and sampled
issues/PRs. External firmware was not executed. This is not an exhaustive review
of every commit or discussion. The first five projects below had GitHub Discussions
disabled in the fetched metadata. Empty issue lists are not a reliability claim.
No external source, OEM database, private trace, VIN or binary was imported.
Source IDs below identify evidence, not vehicle approval.

| ID / project / pinned revision | Implemented behavior and applicability | License / reuse decision |
|---|---|---|
| S01 [vagcan](https://github.com/pashokitsme/vagcan/tree/390de67c8e4f4737aabc05428e55ec860d35c545) | Rust UDS, slcan/ESP32, real gateway bitmap reader and identity walk. Source tests plus author capture-derived claims for Octavia III FL 2017; no Tiguan confirmation. | No root LICENSE found. Algorithm/protocol comparison only; no code, recorded bitmap or OEM data copied. |
| S02 [esp32_uds_gateway](https://github.com/ayyaris/esp32_uds_gateway/tree/b472e8ed9c32ef7ed5a50737ecfbf1867dc97e4f) | ESP-IDF TWAI queues, ISO-TP RX/TX, WebSocket diagnostics, sessions and flashing. No J533 inventory; generic configurable pair. No exact vehicle acceptance established. | No root LICENSE. Do not copy. Its write/security/download functions are outside scope. |
| S03 [dpf-load-monitor-wide](https://github.com/v-cu/dpf-load-monitor-wide/tree/f902045bfedbd54d6e3c7fa0d69ec55e868ed1a8) | ESP32-P4 display, fixed 7E0/22 engine DIDs, fast/slow scheduling; diesel EDC17-oriented claims, not CZPA. | LICENSE.txt is CC BY-NC-SA 4.0; not imported into MIT MiniDash. |
| S04 [MQB-sniffer](https://github.com/mrfixpl/MQB-sniffer/tree/b3df8f0fc9b82b44e2de0d3b8b3a8696f66556a4) | Arduino/MCP2515 receive logger, NORMAL mode at 500 kbit, simulated RPM. `getRPM(true)` is TODO; no UDS/ISO-TP discovery implementation. Golf Mk7 connection notes are not functional diagnostic evidence. | No root LICENSE. Comparison only. |
| S05 [simos-suite](https://github.com/dspl1236/simos-suite/tree/301add2595dac7803200aabf0cdc1e7378d7fba7) | Python diagnostics/flashing, static C7 A6/A7 module map, udsoncan identification. Its so-called installed list probes known modules rather than reading a J533 bitmap. | MIT at this revision; last commits changed licensing. No code or bundled OEM-derived content imported. |
| S06 [cps3-dashboard](https://github.com/lilsteelan/cps3-dashboard/tree/3c8780621ff568de55a3d042da56e14046e883ff) | ESP32 autonomous dashboard, static Audi/Skoda OBD/UDS tables and passive CAN. Source comments describe 2017 S3/OBDeleven observations; no exact Tiguan proof. | No root LICENSE. Do not copy source/assets. |
| S07 [opendbc](https://github.com/commaai/opendbc/tree/c3955753ae7258412814e2cdca3ee8af979e4679) | MIT MQB DBC, firmware identity queries and full ISO-TP/UDS. Tiguan Mk2 fingerprints exist; not proof of this ECU software or OBD visibility. | MIT; use existing MiniDash provenance path, no new definitions promoted. |
| S08 [arduino-mcp2515](https://github.com/autowp/arduino-mcp2515/tree/737f0d536770dc7182bc18e0168f38a810f710a8) | MCP2515 CAN controller API, standard/extended frames, filters, modes/error registers; no ISO-TP, UDS or ECU inventory. | MIT LICENSE.md. Hardware-specific comparison only; not a replacement for S3 TWAI. |
| S09 [OpenHaldex-S3](https://github.com/meatro/OpenHaldex-S3/tree/efc6f239d755ec24213406ae34f03f681df10dfa) | Gen5 diagnostic route candidates, ISO-TP and F19E identity read on inline Haldex/chassis bus. Not the same connection point as MiniDash OBD. | Root MIT, separate data license; no write/control behavior or assets imported. |
| S10 [python-can-isotp](https://github.com/pylessard/python-can-isotp/tree/5593c219f35e739554d743a98d3af68c9332da31) | Address modes, transport bounds, BS/STmin and extensive synthetic transport tests. Existing host dependency, vehicle-neutral. | MIT; runtime tests here use pinned 2.0.7, not this research HEAD. |
| S11 [python-udsoncan](https://github.com/pylessard/python-udsoncan/tree/78e85f1b5dac968b6a2d478fce9c0cc5adba2b50) | P2/P2*, service validation and simulated client/server tests; no universal VAG DID catalogue. | MIT; tests here use pinned 1.26.1. Full service API is broader than our read-only policy. |

### Implementation details that matter

**S01:** `crates/uds/vag-uds-client/src/gateway.rs` reads 2A26 (alternate 04A3)
from 710/77A; the source reports a 32-byte LSB-first bitmap. Its suggested
`700 + bit` interpretation has acknowledged ambiguities (776/777 also overlap
response IDs; adding 6A can collide with engine requests). 2A28 is an unresolved
subset, not established reachability. Do not copy the walk onto Tiguan.
The current asynchronous `vag-uds-can/src/isotp.rs` binds response IDs, handles
BS/STmin/SN and bounded FC waits; the older synchronous client ISO-TP explicitly
ignores BS/STmin in its P0 sender. `pdu.rs` allows 10/19/22/3E and up to 30 pending
responses at 2 s per response; it classifies NRC78 before checking echoed SID.
MiniDash keeps stricter SID matching and a shorter absolute bound. PR59 concerns
ODIS/VCDS source-specific unit naming: logical names/numbers are data, not an
arithmetic CAN-address conversion. The source's recorded-car tests were inspected,
not executed or reproduced here.

**S02:** `main/isotp_layer.c` has one TX and RX state, 4095-byte payloads,
FC CTS/WAIT/overflow, BS and SN, 1 s N_Bs/N_Cr, eight FC WAIT limit, padding AA;
100–900 us STmin rounds up to 1 ms. But SF sends do not establish a retained
transaction pair; incoming FF may fall back to response-ID minus 8. CF processing
does not bind incoming CAN ID to the active RX state, and short DLC validation
is insufficient. Queue success is not hardware confirmation. `uds_service.c`
resets the 5 s deadline on each NRC78 without a total budget. Use its separation
of transport/service/UI as an idea, not as trusted code. Open issue 1 requests
CAN FD; classical S3 TWAI does not gain CAN FD through software.

**S03:** `can_driver.c` uses the newer esp_twai node API, 500 kbit, GPIO6/4,
fixed standard 7E0 service22 padded SF; those pins/API are not our hardware.
The receive callback passes only the byte buffer to `can_dpf_handle_rx`, which
matches DID bytes without CAN ID, SID, DLC or outstanding-transaction validation.
No multi-frame reassembly, gateway discovery or bounded NRC78 logic was found
in these paths. Fast/slow cycling is useful but already present in MiniDash.
A diesel DPF table is not evidence for gasoline CZPA or DQ500 telemetry.

**S04:** `shiftlight_with_CAN-BUS_for_MQB.ino` initializes MCP_NORMAL (can ACK),
prints frames, calls `getRPM(false)` simulation, and leaves real RPM TODO.
A commented suggestion of 714/22/F40C and 77E is a research guess, not a verified
route or reason to transmit. No FC/timeouts/session management to adapt.

**S05:** `core/module_scan.py` walks a fixed C7 map, enters extended session 1003,
then reads F187/F197. It labels NRC/timeout absent, which MiniDash must not do:
sleeping/wrong-bus/unsupported/session-restricted are distinct unknowns. Its
+8 powertrain / +6A other rule is contextual, not a global mapping algorithm.
Transport framing delegates to udsoncan and adapter-specific connections.
Its MIT codebase also includes flashing and component-protection work, excluded
from this task. Recent PR18 is adapter mode control, not Tiguan discovery proof.

**S08:** `mcp2515.cpp::sendMessage` sets TXREQ then immediately samples
ABTF/MLOA/TXERR. ERROR_OK is not equivalent to waiting for completed transmission.
Mode/filter/overflow APIs are useful comparisons, but bit timing depends on the
MCP2515 oscillator and cannot be copied into TWAI. There is no ECU-specific
vehicle compatibility claim to transfer. Recent issue history was not exhaustively
reviewed; implementation is the basis of this conclusion.

Detailed S06/S07/S09 implementation and non-GitHub comparisons appear in the
[companion source notes](vag-diagnostics-source-notes.md).

### Primary and non-GitHub references

- S12 [Espressif legacy TWAI, ESP32-S3](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/api-reference/peripherals/twai.html), plus the fetched SDK 5.5.5 sources (`driver/twai/twai.c`, HAL/LL). `twai_transmit_v2` acceptance and TX_SUCCESS are different events. LISTEN_ONLY cannot ACK/transmit; NORMAL is required for active diagnostics. S3 controller needs an external transceiver and is classical CAN.
- S13 [Linux ISO-TP documentation](https://docs.kernel.org/networking/iso15765-2.html): addressing, SF/FF/CF/FC, BS/STmin, padding and transport errors. Public implementation guidance, not a purchased ISO conformance suite. ISO 15765-2 and ISO 14229 normative text was not independently obtained in full.
- S14 [Ross-Tech installation list](https://www.ross-tech.com/vcds/tour/installation-list.php): logical address19 and gateway inventory concept; no universal DID or CAN pair published there.
- S15 [Ross-Tech measuring values](https://www.ross-tech.com/vcds/tour/adv-meas-blocks.php): ASAM/ODX and IDE labels; IDE names do not themselves encode wire DIDs.
- S16 [Ross-Tech DQ500](https://wiki.ross-tech.com/wiki/index.php/7-Speed_Direct_Shift_Gearbox_%28DSG/0BH/0BT%29) describes UDS measuring values but not an exact public DID/scale map. No basic settings executed.
- S17 [Gen5 specialist guide](https://www.haldexrepairs.co.uk/generation-5-haldex-fault-finding-repair-guide/) is secondary evidence that displayed Gen5 pressure can be calculated, not sensor-measured. Do not label an estimate measured pressure.

## MiniDash audit and changes

TWAI → `vehicle_app.c` → `can_link_tx.c` confirmed send → `acquisition.c`
(single owner) → `diagnostic_transport.c` → supported OBD/identity-gated UDS →
canonical store → derived analytics → display and locked Web snapshot.

| Finding | Disposition and evidence |
|---|---|
| Baseline already has NORMAL/listen transition, GPIO conflict checks, accept-all RX, single-shot/no TX queue, 20 ms confirmed TX, bounded bus-off recovery and 128-frame RX queue | Preserved. 15/5 does not collide with tracked display/touch pins; actual wiring still requires car evidence. 87.5% sample point is a choice, not root-cause proof. |
| Pending BUS_OFF could be consumed before submitting another frame; current TX_FAILED alert was swallowed from deferred diagnostics | P0: refuse send on already observed BUS_OFF, preserve current TX_FAILED for existing vehicle logger. Host stub tests verify no submission and priority. |
| Only DLC8 accepted for all ISO-TP RX; valid unpadded SF/final CF rejected | P1: accept actual complete payload lengths, retain full FF/intermediate-CF requirements and 128-byte cap. Prefix/SID/DID and response-length checks remain before FC. |
| Scheduler TX readiness did not gate FC callback | P0/P1: central send gate now checks `tx_ready`; no callback invocation when listen-only/stopped readiness is revoked. Hardware adapter's independent guard remains. |
| First-response 500 ms timer expired during valid multi-frame progress | P1: ongoing reassembly uses existing 250 ms N_Cr and absolute exchange cap, not first-response P2. |
| NRC78 on UDS received only another 500 ms, total 2 s | P1: UDS-only P2*=5 s, total=10 s, max3 accepted pending; no retransmit on pending. OBD policy remains 500 ms / 2 s. These are bounded local policies, not negotiated ECU timing. |
| RX drained before tick could accept a reply after deadline | P1: same deadline predicate checked on matching RX and tick. Tests cover arrival exactly at expiry; codec-only fixtures now set both request/progress clocks consistently. |
| Existing status exposed counters but no layer interpretation | P0/P4: additive `connection_doctor` in `/api/validation` and read-only Web button. Boot-cumulative errors do not establish per-exchange root cause. No extra CAN task. |
| Gateway bitmap and route identity are not equivalent | P2 partial: pure experimental host helpers return bit positions and explicit-map hints; collision checks, no built-in Tiguan pairs, no I/O or automatic registry promotion. |
| Native registry supports one verified manufacturer ECU, host registry is richer | Preserve it. Multi-ECU native registry, active J533 inventory and new DQ500/Haldex DIDs deferred pending evidence; no duplicate scheduler or unverified telemetry. |

### Remaining transport boundaries

Native firmware transmits short allowlisted requests and FC, not arbitrary
multi-frame UDS requests. FC uses BS=0/STmin10 ms, response buffer128 bytes,
SN modulo16 and N_Cr250 ms. No CAN FD, mixed/extended ISO-TP addressing or
29-bit diagnostic TX. Oversize responses fail closed. No new session-control,
TesterPresent, clear, coding, adaptation, routines, SecurityAccess or download.
Host ReadClient already has a mutex, P2/P2*/total bounds, rate floor and
reconnect requirement after desynchronization; no replacement client added.
NRC31 does not prove a DID absent across all sessions/ECU variants.
Late replies to an identical repeated service/parameter cannot be perfectly
correlated without protocol transaction IDs; bounded single-flight and exact
address/echo checks reduce but cannot eliminate that inherent ambiguity.

### Changed files

- `firmware/main/can_link_tx.c`, `diagnostic_transport.c`, `acquisition.c`: P0/P1 fixes.
- `firmware/main/validation_json.c`, `webui.html`: observation-based doctor export/UI.
- `core/test_can_link.c`, `test_diagnostic_transport.c`, `test_acquisition.c`, `test_validation.c`, `test_webui.cjs`, `CMakeLists.txt`: regressions.
- `minidash_live/discovery.py`, `tests/test_gateway_discovery.py`: offline hints and cache/identity contracts.
- `tools/verify_repository.py`: acceptance now requires both CAN confirmation and ISO-TP test groups.
- This report, source notes, architecture, car plan, README and docs indexes: delivery and remaining work.

## Verification

Host environment: macOS arm64, Python3.11.14, AppleClang21, CMake4.4.4,
cJSON1.7.19 (`c859b25`), declared Python dependency pins installed.

| Gate | Actual result |
|---|---|
| `python -m unittest discover -s tests -v` | **234 tests, 232 passed, 2 skipped**: Linux SocketCAN and Windows DPAPI unavailable on macOS. Includes7 new gateway tests. |
| Native CMake build with cJSON + CTest | **10/10 passed**, including new diagnostic-transport group and existing UI/analytics/configuration/backlight groups. |
| `tools/verify_repository.py` | **PASS**: dependencies, Python, fresh native build, inventory, CTest, Web logic, standalone VirtualBus, analytics replay. |
| `core/test_webui.cjs` against generated fixture | **PASS**; doctor uses GET and creates no settings writes. Fake DOM, not physical rendering acceptance. |
| `tools/verify_standalone.py` | **PASS**, actual C firmware acquisition over VirtualBus: VIN/DTC multi-frame, ECU loss, QUIET, fault/reconnect and exact synthetic replay;27 TX frames. |
| `tools/verify_analytics.py` | **PASS**,12 scenarios,66 embedded assertions,47 oracle comparisons,9 malformed inputs. |
| ASan/UBSan full native build | **10/10 passed**; includes50000 malformed acquisition frames. Leak detection disabled; not MCU heap/stack acceptance. |
| Baseline counterchecks | New transport test fails against d9cca9d on unpadded SF; new CAN test fails against d9cca9d on lost TX_FAILED notification. Both pass against changed code. |
| `git diff --check` | **PASS**. |

Reproduction (activate a local environment with the pinned requirement files):

```sh
cmake -S core -B core/build-acceptance -DMD_JSON_SOURCE_DIR=/path/to/cJSON
cmake --build core/build-acceptance
python tools/verify_repository.py --native-build-dir core/build-acceptance --output-dir .local/diagnostic-acceptance
cmake -S core -B core/build-sanitized -DMD_JSON_SOURCE_DIR=/path/to/cJSON \
  -DCMAKE_C_FLAGS='-g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build core/build-sanitized
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir core/build-sanitized --output-on-failure
```

An initial standalone sanitizer compile command from older development docs
failed to link the canonical-store symbol; the full CMake target above was used
successfully instead. ESP-IDF setup initially needed a non-venv Python interpreter
and CMake on PATH; both were resolved locally without repository configuration changes.
No unit test is claimed as electrical ACK, hardware-loopback, display/touch or
vehicle validation. All new CAN responses/identities are synthetic. Settings,
calibration, NVS formats, bootloader and partition table are unchanged.

See [architecture and prioritized acceptance gates](../architecture/vag-diagnostic-discovery.md)
for what is implemented and what remains experimental.

### Firmware build

ESP-IDF **5.5.5**, SDK commit `b774170ff46c393eeb5e495ea37936038d3f4f4f`,
Xtensa GCC14.2.0. Tools/SDK were installed under the task workspace, not globally.
The locked component manager resolved the tracked versions without a lockfile
change. Standalone build **PASS**; CAN enabled, standard OBD enabled, GPIO15/5,
bench OFF, no VAG profile. Existing UI stack checks passed: diagnostics1664+1024,
saved configuration5808+1024 <=8192 bytes. No device was flashed.

```sh
# In the initialized ESP-IDF5.5.5 environment, firmware/:
idf.py -B build-diagnostic-standalone -D PROJECT_VER=diag22-exp \
  -D SDKCONFIG=sdkconfig.diagnostic.generated \
  -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.standalone' build
```

Default/listen-only firmware presets were not separately built in this task;
no-transmit behavior was covered by host tests. Historical canfix19 bench results
remain historical. Newly built firmware still requires bench and car acceptance.

Final candidate app version `diag22-exp`: 1852592 bytes; SHA-256
`3c2b3e1e5bd2c204a39a4f4062576aa44b45f5bdc33eb6ce78d21d03703d637e`. Build artifacts remain local, not committed.
