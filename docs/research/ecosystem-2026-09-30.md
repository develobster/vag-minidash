# MiniDash ecosystem research and integration decision

Date: 2026-09-30. This report precedes implementation in
`feature/research-replay-reliability`, based on UI commit `6f436dd`.
No architecture replacement, new product repository or UI reconstruction.

## Repository state / local-only changes

Three managed/local checkouts share `C:/Users/user/vag-minidash/.git`:
original display prototype `dc16a90` (dirty research/import files), production
UI `00c6317` (clean), latest touch/Web UI `6f436dd` (clean at audit).
The user confirmed that the night work is exactly `6f436dd`; it exists locally
and remotely. Its four pages, Web editor, themes, calibration and shared core
must remain. No unidentified newer night UI source remains pending.

Original dirty files: docs/research/vag-diagnostic-data-sources.md;
minidash_live/mcd_import.py, profiles.py, registry.py; tests/test_can.py,
test_signal_registry.py. Untracked: artifacts/hardware, artifacts/odis-offline-audit,
tests/fixtures/mcd-typed-scalars.json. They are not automatically imported:
typed MCD changes need separate review, and artifacts can contain private data.
Full binary patches, statuses, all-ref history/reflogs/stashes and a copy of the
untracked scalar fixture were saved outside the repo in the chat's
outputs/forensic-2026-09-30. All 57 original artifact files have a SHA-256
manifest; original artifacts remain in place. No reset/clean/restore/rebase.
All inspected stash lists are empty. Original dirty files remain untouched.

Critical discovery: origin fetch refspec only tracked diagnostic-data-import.
An ordinary fetch therefore missed newer remote branches and made commits look
local-only. Explicit all-heads fetch restored remote tracking references without
changing local branches. Remote tips observed:

| Branch | Tip | Relationship |
|---|---|---|
| main | de7f071 | historical audited baseline |
| diagnostic-data-import | 54d0ea5 | common ancestor of UI/MQB work |
| esp32-display-prototype | dc16a90 | early display/touch, not latest UI |
| production-ui | 00c6317 | ancestor of latest UI |
| ui-ux-touch-zones | 6f436dd | current complete night UI |
| mqb-knowledge-import | 0b3121d | seven additional backend/knowledge commits |
| runtime/session-scheduler | e62ca59 | historical scheduler foundation |

After full fetch, no local commit absent from all origin branches was found.
MQB and UI branches diverge after 54d0ea5; both must be retained. MQB changes
are 19 files, 3,711 additions / 105 deletions relative to that ancestor: sourced
candidate definitions, provenance license/revision fields, passive observation
grace, no default diagnostic session switch/keepalive, fixture/evidence tests
and car-validation manifests. GitHub public REST returned rate-limit 403 for
license queries; git transport succeeded. PR inventory is not established by
that result; no claim that PRs are absent.

## Current feature / architecture matrix

| Area | State before this integration | Evidence / gap |
|---|---|---|
| Transport/CAN/ISO-TP/J2534 | partial | transports.py/j2534.py; VirtualBus/ABI tests, own VNCI physical route unproven |
| UDS/discovery/identity | partial | uds/discovery/identity; bounded 0x22, identity evidence; broad live inventory absent |
| Providers/applicability/provenance | implemented | knowledge/vag/registry; exact identity gates, source metadata; external candidates not verified |
| Planner/scheduler/store/arbitration | implemented with limits | pipeline/runtime/arbitration; passive fallback, cooldown/backoff/single flight; no screen subscriptions/global bus-load feedback |
| Passive definitions | partial | DbcAdapter + external DBC; MQB branch adds candidates, not vehicle verification |
| CLI/doctor/trace | implemented with limits | __main__, JSONL PDU/events, VIN redaction; MCU appliance integration remains |
| Logging/replay | partial | can_capture raw schema-1 receive log; synthetic UDS ReplayTransport; real CAN file → runtime replay gap |
| ESP32/display/touch/themes/layouts | implemented | LVGL + XPT2046, calibration/Russian, four pages, numeric/bar/status/graph; physical readability acceptance outstanding |
| Wi-Fi/Web/settings | implemented | explicit WPA2 AP, session/revision/strict JSON, CRC NVS Preview/Save/Cancel; offline editor, no diagnostic write routes |
| BLE/app | bring-up only | no product BLE API; S3 has BLE, do not assume Classic Bluetooth SPP |
| CAN hardware on MCU | missing integration | disabled; no verified transceiver/vehicle wiring |
| DTC read/clear | missing live implementation | FakeECU is not live 0x19/0x14 support |
| Coding/adaptation | scaffold-only | simulator/journal; live writes prohibited |
| Hardware variants | prepared boundary | same eight C modules, capability/feature profiles; no OLED/STM32 firmware |
| Power/brightness/OTA/recovery | partial / missing | display sleep is not automotive power shutdown; BL fixed 3.3 V; factory-only partition, no OTA slots |
| Reliability/metrics | partial | watchdog, quality/TTL, transport recovery, AP heap samples; no persisted crash lifecycle/bus-load/performance dashboard |

Runtime stays Transport → discovery → Vehicle/EcuIdentity → IdentityContext →
Knowledge Providers → Registry → Planner → passive CAN/UDS → SignalStore →
SourceArbitrator → UI/CLI/debug. Python is not MCU firmware. New work should
feed this pipeline, not add a parallel decoder/scheduler/store.

## Research sources / comparison / license status

Links are primary repositories, maintainer documentation or manufacturer pages.
Habr/Drive2 searches are useful leads; unattributed forum layouts are not promoted
to authoritative vehicle knowledge. Claims below are paraphrased, not copied code.

| Source | Useful comparison / decision | License / integration policy |
|---|---|---|
| [opendbc](https://github.com/commaai/opendbc) | Existing MQB provider already uses pinned DBC metadata; incorporate existing branch, no duplicate importer | MIT checked; external DBC revision/data notices retained |
| [SavvyCAN](https://github.com/collin80/SavvyCAN) | Graph/correlation/capture/replay workflow; use as external analysis companion, not embed desktop app | [MIT](https://github.com/collin80/SavvyCAN/blob/master/LICENSE); notice required for code reuse |
| [python-can log readers](https://python-can.readthedocs.io/en/stable/file_io.html) | ASC/BLF/CSV/candump readers exist; future format adapter should call these, not write new BLF parser | [LGPL-3.0](https://github.com/hardbyte/python-can/blob/main/LICENSE.txt); existing optional dependency, no vendoring |
| [can-utils](https://github.com/linux-can/can-utils) | candump/canplayer/canbusload and ASC conversion; external tools for measurement | Per-file licensing must be checked; no code copied |
| [cantools](https://github.com/cantools/cantools) | Existing decoder already covers DBC/multiplexing; improve tests and provenance, no second parser | MIT in project metadata; existing pinned optional dependency |
| [can-isotp rate limiter](https://github.com/pylessard/python-can-isotp/blob/v2.x/doc/source/isotp/implementation.rst) | Sliding-window transmit limiter is distinct from job frequency; don't equate scheduler cooldown with full bus budget | Installed pinned can-isotp 2.0.7 metadata confirms MIT (LICENSE.txt); no vendoring |
| [udsoncan](https://udsoncan.readthedocs.io/en/latest/udsoncan/connection.html) | Layered connections and read-DTC services are references; retain current small ReadClient allowlist | MIT verified in upstream LICENSE.txt; no automatic replacement/dependency expansion |
| [odxtools](https://github.com/mercedes-benz/odxtools/blob/main/README.md) | Variant-aware ODX/PDX exploration for future legal offline importer | MIT tool; OEM database rights are separate |
| [OpenXC](https://vi-firmware.openxcplatform.com/en/master/) | Canonical semantic data independent of hardware and presentation | Upstream BSD-style notice checked; no Ford definitions imported |
| [ESP32RET](https://github.com/collin80/ESP32RET) | External ESP32/SavvyCAN capture bridge and buffer/drop metrics inspiration | MIT checked in upstream LICENSE; driver choice must match ESP32-S3 |
| [RaceChrono DIY](https://racechrono.com/article/2572) | Documented BLE CAN/Monitor API makes future optional bridge concrete | Protocol docs reference; repo license not confirmed, no copied implementation |
| [RaceChrono CAN support](https://racechrono.com/support) | Vehicle-specific packet layouts are explicitly required, consistent with evidence-first model | Proprietary product documentation, no decoder extracted |
| [RealDash CAN](https://realdash.net/manuals/realdash_can.php) | Explicit transport/description mapping; optional external export from canonical model | Documentation/reference only; source license not verified |
| [Car Scanner](https://www.carscanner.info/) | Custom PID/profile usability inspiration; app profiles are not open validated VAG database | Proprietary; no data extraction/redistribution |
| [Ross-Tech measuring values](https://www.ross-tech.com/vcds/tour/adv-meas-blocks.php) | Variant-specific labeled live values; control sample rate as selected channels grow | Proprietary reference; no label-file copying |
| [OBDeleven UDS adaptation](https://support.obdeleven.com/en/articles/5608792-adaptation-uds) | Separate read/report from protected writes/security access; no generic automatic adaptation | Proprietary reference; no security unlock implementation |
| [Clever OBD2](https://clever-obd2.ru/offer/1514003/) | OLED 1–8 parameters, Wi-Fi settings/update, maxima, ignition behavior; own ignition-retard evidence still needed | Manufacturer claims, not open CAN/DID/firmware data; do not emulate unsafe clear-all button |
| CanSensor | Exact product/source could not be reliably identified by fresh searches; keep as unresolved lead | Unknown, do not invent capabilities/license |
| [CANedge](https://www.csselectronics.com/pages/black-box-can-bus-logger) | Filtering/triggers/power-safe logging are acceptance goals; manufacturer claims do not prove MiniDash power safety | Proprietary product docs; ideas only |
| [ESP-IDF 5.5.5 TWAI](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/api-reference/peripherals/twai.html) | Listen-only, external transceiver, error counters/bus-off recovery; hardware gap remains | Official API use within existing IDF, no unapproved GPIO |
| [ESP-IDF OTA](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/api-reference/system/ota.html) | Dual OTA slots/otadata/first-boot validation/rollback need partition migration | Official API; don't change installed factory-only layout in this task |
| [ESP-IDF sleep](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/api-reference/system/sleep_modes.html) | Deep sleep suspends MCU; regulator/LCD/transceiver drain and wake source need board measurements | API ideas, no unverified wake pin/source |
| [TI automotive protection](https://www.ti.com/tool/TIDA-01167) | Reverse polarity/jump-start/load-dump protection; USB breadboard is not automotive-qualified power | Reference design, actual purchased BOM/schematic must be reviewed |
| [LVGL](https://github.com/lvgl/lvgl) / [ESP HTTP server](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/api-reference/protocols/esp_http_server.html) | Already used. Measure render/event/heap latency before changing engines/frameworks | Existing dependencies, no alternative UI stack |
| [car-hud](https://github.com/fbiego/car-hud) | BLE ELM/LVGL dashboard reference; MiniDash already has richer configuration and different acquisition route | License not yet checked: reference only, no code reuse |

Searches also covered GitLab, Habr, Drive2, STM32/CAN/OLED, community VAG DID
and dashboard projects. No newly verified raw VAG decoder was established by
this web search. The already present remote MQB branch is more immediately
valuable than adding another unsourced DID list. Unknown-license examples stay
reference-only. The local repository has no root LICENSE: redistribution policy
needs explicit project-owner choice before broad third-party code incorporation.

## New findings / classified opportunities / top next improvements

A = safely implement now; B = car/trace needed; C = hardware needed;
D = architectural preparation; E = research; F = already implemented;
G = improve existing; H = reject. Categories may overlap.

| Opportunity | Class | Already present / exact gap | Basis | Complexity/risk | Concrete components / dependency |
|---|---|---|---|---|---|
| Restore all-branch knowledge + UI state | A/G | Both exist on divergent branches; single-head fetch hid MQB | Forensic git refs/diff | Low/merge tests | Isolated integration branch, merge MQB retaining UI; no car |
| Real CAN trace → deterministic local decode | A/G | Raw recorder + synthetic UDS replay exist; real CAN replay absent | can-utils/SavvyCAN/python-can workflows | Medium/low if no bus/session calls | can_capture validation + existing replay/pipeline/CLI; recorded file, no car |
| Measure replay decode/arbitration and staleness | A/G | Runtime counters exist; no reproducible offline trace path | Existing clock injection + replay | Low | Same store/arbitrator, virtual clock, measured processing time; no vehicle claims |
| Screen/log-aware subscriptions | D/G | Requested signals fixed for watch; MCU pages know bindings but no host subscription contract | Existing planner/UI and Ross-Tech selected-values model | Medium, load/freshness risk | pipeline/PollJob/display protocol versioning; synthetic tests first, car load validation |
| Explicit bus/ECU budget + priorities | B/D/G | Cooldown/backoff already exist; no measured bus-load feedback | can-isotp limiter, canbusload | Medium/high if guessed rates | Scheduler/transports/metrics; car bus measurement |
| Expand ignition/torque/gear/wheels/buttons | B/G | MQB/Haldex/UDS candidates already sourced; no broad exact-vehicle validation | Pinned opendbc + validation manifest | Medium, false decode risk | VAG data/evidence/fixtures; real tagged traces required |
| Read-only DTC report/snapshot | B/D | FakeECU only; no scoped 0x19 live API | udsoncan/Ross-Tech | Medium, ECU-specific semantics | uds/discovery/diagnostic model/CLI; fixture plan then ECU replies |
| Import ASC/BLF/candump via existing library | A/G next | Raw schema-1 only | python-can readers | Low/format/timestamp correctness | Small optional converter, no new binary parser; licenses/fixtures |
| CAN activity ≠ ignition; protected sleep/wake | B/C | UI sleep only, no power budget | ESP sleep/Clever/TI | High battery/transient risk | Power frontend/transceiver/display; board current and vehicle timing evidence |
| Signed A/B OTA + recovery | C/D | factory-only 3 MiB app; migration absent | IDF OTA/secure boot | High persistent/brick risk | partitions/bootloader/key lifecycle/assets/config compatibility; dedicated backup/bench |
| Reset reason/boot health/heap/latency dashboard | A/G next | Watchdog/AP heap already available; no stable status contract | IDF + actual previous heap/LVGL defects | Low/medium; bound NVS wear | firmware main/Web status/metrics; measure before thresholds |
| Optional RaceChrono/RealDash bridge | D | Existing canonical USB bridge; BLE product protocol missing | Documented vendor protocols | Medium, platform BLE compatibility | Adapter over canonical model, never raw UDS writes; phone acceptance |
| MiniDash Lite renderer | C/D | Common C/UI/input/config boundary already exists | targets.md and STM32 budgets | Medium, RAM/flash limits | Approved OLED/MCU/transceiver only; no fork |
| Second DBC parser / UI framework rewrite | H/F | cantools/LVGL/Web config already implemented | Existing code/tests | High needless compatibility risk | Reject |
| Blind DID scanning / automatic coding | H/E | Fail-safe identity/allowlist present | Vehicle safety and missing write evidence | High | Reject; unknown vehicle stays fail-safe |

## Pre-code implementation plan and risk boundary

1. Preserve forensic state (done), create isolated branch from confirmed night UI
   (done), publish this report before application changes.
2. Integrate existing `origin/feature/mqb-knowledge-import` with an ordinary merge.
   Review any conflict; retain both histories and UI files. Run baseline/full tests.
3. Add bounded raw CAN log validation and deterministic offline replay into the
   existing runtime. Reuse DbcAdapter/Planner/Store/Arbitrator, reject malformed
   frame/timestamp/ordering, preserve error/remote flags, never call CAN Bus,
   transport/session/discovery-on-wire. Mark output offline and unvalidated.
4. Add failure-injection/replay/CLI regressions and optional VirtualBus tests using
   the already pinned dependencies in an isolated test venv. Document commands.
5. Small logical commits, inspect diffs; no factory artifact or UI asset overwrite.
   No auto car probing, DTC write, OTA repartitioning, power wiring or UI redesign.

This is a selected safe A/G batch, not a claim to implement every researched
feature. Larger B/C/D items have explicit gates above. Final execution/results
are recorded separately so the pre-code decision remains distinguishable.

## Additional reviewed local-only recovery decision (before import)

After the first replay batch, the preserved original dirty patch was reviewed.
Its typed scalar import is a compatible A/G improvement: explicit IEEE754
FLOAT32 and bounded non-overlapping TEXTTABLE choices, signed invalid sentinels,
and one shared scalar validator for profiles/registry. It retains single leaf
DOP restrictions and candidate/evidence gates; it adds no proprietary database
or actual vehicle decoder claim. The five-record fixture is synthetic. Import
only mcd_import.py/profiles.py/registry.py/test_signal_registry.py hunks plus
that fixture into a separate logical commit, retain the remote provenance fields,
add validation/identity regressions and reject unknown byte order. Original
checkout/dirty files/artifacts must remain byte-for-byte untouched. Original
research notes are reference input rather than wholesale replacements for the
newer merged research document. This is recovery of existing local work, not a
new parallel importer. Tests must establish compatibility before committing.
