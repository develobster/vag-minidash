# Repository consolidation — 2026-09-30

## Executive summary and delivery state

The verified integration base is `22a346bee87f82c12943fd7a3277ea219d698d03`.
All eight current remote branches and all local branch tips are ancestors of it
or equal to it. No newer/outside branch commit is missing. This conclusion uses
Git merge-bases, actual diffs and authenticated GitHub branch/PR inventory.
The product remains a bench UI plus host diagnostic runtime, not yet standalone
automotive acquisition. Architecture and UI source are preserved.

The owner corrected the original no-push instruction during this task and
explicitly authorized commit/push. Remote `feature/repository-consolidation` was
created at the verified base through the authenticated GitHub connector.
The local common Git directory still rejects writes in this execution context;
therefore local branch creation/checkout/commit could not yet succeed. Publication
uses the authorized GitHub Git-data API, preserving parent history; it does not
write around the local filesystem restriction. Local synchronization remains a
separate blocker. See delivery receipts/final response for published commit SHAs.

## 1. Repository before work

Origin: `https://github.com/develobster/vag-minidash.git`.
Working checkout: existing `Documents/Codex/work/minidash-ux-touch` worktree;
common metadata remains in the original `vag-minidash/.git`.
Before changes: clean `feature/research-replay-reliability`, HEAD `22a346b`.
Five local branches, three occupied worktrees, no tags or stashes.

- Original display worktree: six dirty tracked files (MCD importer/profiles/
  registry, two tests, diagnostic research document), typed synthetic fixture,
  hardware and OEM-audit artifacts. Nothing there was changed by this task.
- Production UI worktree: clean, `00c6317`.
- Current worktree: initially clean, `22a346b`; task changes are now prepared here.

The configured origin fetch refspec covers only `feature/diagnostic-data-import`.
An ordinary fetch is not an all-branch audit. Local tracking refs omit the
research branch even though its remote tip is `22a346b`. This task does not edit
shared Git configuration while its filesystem permission is blocked.

## 2. Commit graph and branch relationships

```text
de7f071 main -> e62ca59 runtime -> 54d0ea5 diagnostic import
                                   |
+                                   +-> display/LVGL/USB -> 6f436dd UI -> 18b74d9
                                   +-> seven MQB commits -> 0b3121d -----+
+                                                                       |
+                   874c202 (two-parent merge) <--------------------------+
+                       -> 5ae64cf watch -> fe3346f replay -> 013d350 MCD
                       -> d56cb34 results -> 16c4a3c AGENTS -> 22a346b docs rule
```

For every listed branch, merge-base with `22a346b` equals the branch tip.
Unique commits/files means work outside baseline, not files differing because
an old branch lacks newer features. Actual per-ref diff stats were saved locally.
No manual feature port or additional merge is necessary from the named branches.

| Branch (local and/or origin) | Tip | Merge-base with baseline | Outside commits / unique files | Purpose / preserved work | Recommendation |
|---|---|---|---|---|---|
| `feature/diagnostic-data-import` | `54d0ea5` | `54d0ea5` | 0 / 0 | Identity/registry/import and first exact coolant route | ARCHIVE after verified consolidation delivery |
| `feature/esp32-display-prototype` | `dc16a90` | `dc16a90` | 0 / 0 | ST7789/XPT2046 prototype; occupied original dirty worktree | KEEP; local work/artifacts need preservation |
| `feature/production-ui` | `00c6317` | `00c6317` | 0 / 0 | LVGL/navigation/USB canonical bridge; occupied worktree | KEEP until worktree owner approves archival |
| `feature/research-replay-reliability` | `22a346b` | `22a346b` | 0 / 0 | UI + MQB + replay + watch fix + typed MCD + AGENTS | KEEP as integration/rollback checkpoint |
| `feature/ui-ux-touch-zones` | `6f436dd` | `6f436dd` | 0 / 0 | Current configurable UI/Web/shared core checkpoint | ARCHIVE checkpoint after physical acceptance and backup |
| `feature/mqb-knowledge-import` | `0b3121d` | `0b3121d` | 0 / 0 | MQB candidates/provenance/passive grace/read-only default | KEEP while PR #1 remains open |
| `main` | `de7f071` | `de7f071` | 0 / 0 | Existing default branch, intentionally old integration state | KEEP; no merge or default-branch change authorized |
| `runtime/session-scheduler` | `e62ca59` | `e62ca59` | 0 / 0 | Earlier session/scheduler/passive runtime foundation | SAFE TO DELETE candidate, ancestry only; approval still required |

## 3. Role of 22a346b

Last known pushed checkpoint, independently verified as the current remote
research tip. It contains both UI and MQB ancestry, recovered typed MCD,
production-path recorded CAN replay, watch timing fix, research and AGENTS.
It is selected for its verified content/ancestry, not because it was last pushed.
No hard reset or destructive rebase was performed.

## 4. Role of 6f436dd

Preserved UI/night-work checkpoint. `git diff 6f436dd 22a346b -- firmware core
docs/ui docs/architecture/targets.md` is empty. Thus later host/MQB work did not
discard display/touch/buttons/localization/layouts/Web or shared C modules.
This consolidation only annotates the UI delivery documentation; UI/firmware
and core source remain unchanged.

## 5–6. Canonical baseline selection and reason

`feature/research-replay-reliability` at `22a346b` is the verified superset base.
Authenticated GitHub inventory returned eight branches, default `main` at
`de7f071`, and exactly one open PR:
[PR #1](https://github.com/develobster/vag-minidash/pull/1), MQB `0b3121d` into
diagnostic import `54d0ea5`. Its commits are already ancestors of baseline via
`874c202`, but the PR itself is still open/unmerged. No `master`, `development`,
`feature/screen-button-navigation`, extra controls, hardware or newer UI branch
was present in that inventory. No ref was inferred from a suggested name.

Git HTTPS from this sandbox failed with Schannel `SEC_E_NO_CREDENTIALS`; the
GitHub connector supplied authoritative reads. No credentials were extracted,
TLS validation disabled, main changed, PR merged/closed or branch deleted.

## 7. Lost/unique work detection

`git fsck --full --no-reflogs` found no dangling commits; one dangling tree
`7a63355a36f9ca62856379a7181bb8a08a1990a3` and one blob
`0d6155d0be7460b628e951aae81ec621ecdbb0f6` were found.

- Tree versus the completed MQB merge differs only in `tests/test_can.py`:
  unresolved conflict markers and duplicate `import socket`. It is an
  intermediate merge tree, not a missing implementation. Archived locally as
  `dangling-tree.tar`; no conflict-marker content is integrated.
- Blob is an older UI status document (production UI / universal gestures).
  Saved locally as `dangling-blob.txt`; current calibrated touch/UI supersedes it.
- Reflog entries resolve to reachable history. No stash or old-branch-only commit
  contains another controls/BLE/Web/DTC/replay implementation.
- Original local MCD code was recovered by `013d350`; current shared validation
  also retains unsigned legacy compatibility, rejects unknown byte order and
  keeps candidate gates. The original code, fixture and full binary patch are
  retained locally; they are not blindly overlaid on newer source.
- Original research text includes local OEM extraction progress and scripts.
  Those unique local tools/reports remain under `artifacts/odis-offline-audit`.
  They need provenance/privacy/license review before any separately scoped port;
  ignoring them does not remove them. Full raw/generated OEM data is not published.

Local forensic output includes audit JSON, all worktree patches, ignored-file
inventories, artifact SHA-256/size manifest, reflog/graph and dangling archive.
Build caches, reports, SVGs, test environment and replay benchmark outside Git
were inventoried/preserved. No Git GC, cleanup or worktree archival ran.

## 8. What was integrated

No feature merge was needed: `874c202`, `5ae64cf`, `fe3346f`, `013d350` already
integrated the relevant unique features. This batch adds clear developer/current
documentation, historical labels, artifact guards and repeatable offline
acceptance with five runner fault-contract tests.

## 9. What remains separate

Original dirty checkout, private/OEM/hardware artifacts, the production worktree,
open MQB PR and historical branches. No car candidate was promoted; no extra
DBC/OEM package was vendored. Hardware acceptance and local Git synchronization
are separate from publishing the reviewed feature-branch tree.

## 10. Structure changes

Source/runtime/provider/replay/core/firmware/Web/test/tool paths stay unchanged.
Added `docs/README.md`, `docs/development.md`, `docs/status.md`,
`docs/artifacts.md`, `docs/architecture/overview.md` and this report.
Added `tools/verify_repository.py` and `tests/test_repository_acceptance.py`.
No import/build/API/schema/NVS/partition migration or architecture rewrite.
See the developer guide's explicit ownership map.

## 11. Gitignore changes

Added `.local/`, Python check/coverage caches, Node/PlatformIO caches, temporary
logs/backup/editor OS files and local hardware/OEM artifact directories. Existing
IDF/build/traces/connection guards remain. Hardware policy is directory-scoped;
there is no blanket ignore of all JSON/binary data. Reviewed fixture `.jsonl`,
`.log`, `.bin` exceptions remain visible. `git check-ignore` verified factory/OEM/
build/local output ignored and synthetic JSON/DBC/capture fixtures visible.

## 12. Artifact cleanup and preservation

Cleanup means classification/ignore guards, not deleting irreplaceable evidence.
All 57 original artifacts match the earlier forensic hashes: none.
Factory backup length: 16777216 bytes; SHA-256:
`CD0F30AC9491DA499EAF2167F07B5ACCF23CF6779C506110431AA766F346273E`.
No backup/dump/generated report was overwritten or uploaded. Local patches and
dangling-object copies add recovery evidence; a hash is not a second factory copy.
Policy and copy-before-delete requirement: [artifacts](../artifacts.md).

## 13. Documentation cleanup

README is a short accurate entry. The complete previous README is retained at
`docs/history/README-pre-consolidation.md`, with local links adjusted for its
new location. Root lab/design notes have historical notices, no body deletion.
The docs index separates current, research and historical references; original
paths continue working. UI old test totals are labeled historical. Current MCD
description/roadmap now acknowledges the typed subset and existing arbitration.
Research/reports are retained with their original context. No broad claim that
coding, standalone CAN, PWM/AUTO or unverified candidate decoding works.

## 14. Python/tests and runner

Before changes: 159 tests, one Windows Linux-AF_CAN skip, PASS (pinned optional
python-can/isotp/udsoncan/cantools present). After changes: 164 tests, one same
skip, PASS. Five additions verify acceptance logging, nonzero/missing commands,
existing-log preservation and incomplete native-gate refusal. Existing VirtualBus,
identity, knowledge/provenance/applicability/schema, scheduler/store/arbitration,
typed MCD and replay regression tests ran. No original test was removed.

Runner generates fresh Web fixtures from native C registries only after a
successful rebuild and checks both native CTest names. Its first real run
correctly reported `INCOMPLETE_OR_FAILED` when native rebuild was denied; it did
not relabel previously compiled binaries as a fresh passing gate.

## 15. ESP32 configure/build

ESP-IDF 5.5.5 environment exists. Dot-sourcing failed under constrained language;
normal profile invocation and direct IDF Python worked. `PYTHONUTF8=1` fixed
cp1251 output exceptions. Configure completed despite SDK ownership warnings.
Initial builds failed due ccache process/handle and protected Windows temp
creation; an xtensa compiler application error was also reported by the user.
Retry uses disabled ccache and a workspace-local temporary folder.
Final configure and complete build PASS after workspace TEMP and ccache=0. App size 1,717,984 bytes (0x1a36e0), 45% of the 3 MiB partition free. Rebuilt image SHA-256: `23D642EA6918992658A6E1C5DD09198FC3413755B02F8C1984449A119A88D413`. The version descriptor is 22a346b-dirty; this is a local build artifact, not a flashed release.
No flash/erase/partition/efuse operation or device acceptance ran. Historical
successful UI image/bench tests do not substitute for this gate.

## 16. Native and Web result

Existing compiled artifacts: CTest 2/2 PASS; Node Web logic PASS. Fresh native
rebuild initially failed `FindFirstFileExA(...ziglang/lib/include): Access is
denied`. This is an environment/access blocker, not a source-compiler diagnostic.
A scoped Zig 0.13.0 copy and workspace-local Zig caches resolved that blocker. Fresh configure/build in core/build-consolidation PASS; CTest 2/2 PASS; Web fixture regenerated from current C sources and Node logic PASS. The final acceptance runner summary is PASS, including all dependencies, 164 Python tests + one platform skip, fresh native build, both CTest names and Web gate.
Node uses a fake DOM, not visual browser/mobile acceptance.

## 17. Replay and CLI result

CLI `discover`, `signals`, `watch`, `replay-can`, `doctor` help/contracts tested;
real VirtualBus/ISO-TP discovery and fallback exercised by existing pipeline
tests. No physical VCI connected. Existing replay CLI tests assert no discovery,
bus, socket, scheduler or VCI calls. The 10,000-frame synthetic benchmark was
rerun through the production decoder/store/arbitrator: 10,000 frames received/decoded, zero UDS requests, two stale signals after final-age=1; 1.696273 seconds processing (about 5,895 frames/s on this PC), CLI exit 0.
Not real-car evidence, MCU receive throughput or render FPS. Candidate state
and final TTL expiration stay explicit. Recorded output creation is exclusive.

## 18. Created commits / publication method

Published logical sequence:

| Commit | Scope |
|---|---|
| `bdf254135c055504dbdb3660f4a37faa42117fb8` | Artifact ignore guards and retention/privacy documentation |
| `34aa05fa26e8b14cbbc21e1b6eebb888378881e5` | Repeatable offline runner, five fault tests and developer/build guide |
| Documentation consolidation commit containing this report | README/index/current architecture/status/history and corrected importer/UI documentation |

The final documentation commit's SHA is obtained from the published branch head;
a report cannot contain its own Git object hash. Exact final SHA, remote tree/ref
verification and local synchronization limits are recorded in the final delivery
receipt/response. Remote Git-data commits use
the original `22a346b` parent and normal linear parents thereafter, retaining all
earlier merge ancestry. No squash-all, force update or main merge.
Local Git attempts failed writing shared `refs/...lock`; permission requests
returned no additional grant even after user stated access was enabled.

## 19–20. Local status and HEAD

Local HEAD remains `22a346bee87f82c12943fd7a3277ea219d698d03`, branch
`feature/research-replay-reliability` until shared Git permissions are available.
The local task patch is deliberately retained, not falsely claimed clean or
committed locally. Published remote consolidation HEAD is reported separately.
After publication, sync the original existing worktree with writable common Git,
preserve its task patch, fetch all branch heads, checkout the published branch,
then verify every task file and a clean status before discarding any duplicate
patch. Do not overwrite the original dirty display worktree or reset it.

## 21. SAFE TO DELETE proposals

`runtime/session-scheduler` contains no unique commit/file outside baseline:
ancestry-safe deletion candidate after verified backup/delivery and explicit
approval. Other fully contained unoccupied legacy branches may be archived,
but no branch deletion was performed or authorized by this task. Keep MQB while
PR #1 is open, and keep both occupied worktree branches. No remote cleanup now.

## 22. Branches/work to keep

Consolidation development line, `22a346b` rollback/research checkpoint,
`6f436dd` UI checkpoint until physical acceptance, original display worktree
with dirty/ignored data, production worktree pending owner review, MQB PR branch,
and `main`. Factory backup and OEM local audit tooling are protected separately.

## 23. Technical debt and safety gaps

P0 local Git synchronization and physical device acceptance; fresh build gates now pass; no repository
CI workflow yet; no root project license; historical documents retain outdated
details behind explicit labels. Host pipeline is not an MCU port. Physical VNCI
and CAN connection-point visibility, protected power/transceiver and bus budget
remain unproven. No MCU logger, live DTC inventory/clear, coding/adaptation,
verified vehicle controls/illumination, AUTO source, OTA or linked Lite firmware.
Read-only identity/provenance gates remain unchanged. Current BL is power at
3.3 V; GPIO must not drive LED load. See [status](../status.md).

## 24. Feature matrix

[Current status](../status.md) inventories display, touch, screens/layouts/themes/
editor, navigation/controls, BLE/Wi-Fi/Web/settings, passive CAN/UDS/ISO-TP/VCI,
discovery/VehicleIdentity/EcuIdentity/context/providers/MQB/registry/MCD/planner/
scheduler/store/arbitrator/replay, DTC/coding/logging/evidence/firmware/abstraction.
Each row links source/test paths, evidence labels and limitations. Historical
HARDWARE TESTED and exact F405 REAL-CAR VALIDATED are explicitly bounded.

## 25. Next milestone / first bench and car actions

| Priority | Purpose / modules | Acceptance | Dependency / risk |
|---|---|---|---|
| P0 | Finish local Git sync + repeat acceptance (`tools/verify_repository.py`, IDF build) | Clean local consolidation checkout equals published tree; 164+1 skip, CTest 2/2, fresh Web and IDF build | Common Git and toolchain/temp filesystem access; never reset unrelated original work |
| P0 | Bench UI sign-off (`ui_screens`, configuration, XPT/NVS/Web) | RU glyphs, every dashboard picker, Settings select/CHANGE, 3/4-page wrap, Save/Cancel/reboot retain calibration; firmware hash/UI self-test recorded | Physical panel/phone; density/FPS not yet measured |
| P0 | MiniDash physical read-only route (`doctor`, discovery/identity/transport) | Verified DLL bitness, protected power, F190 then exact ECU fingerprint, correct response route/cleanup, no implicit session/write | Stationary vehicle and adapter; existing ODIS trace is insufficient for this implementation |
| P1 | Passive evidence + budget (`can_capture`, providers, planner, evidence manifest) | Connection-point traces/reference labels; replay same production path; identity-scoped codec promotion only after comparison; measured latency/load | Car/hardware listen-only validation; candidates remain unverified |
| P1 | Standalone acquisition/logging bridge (`firmware` CAN hardware adapter, shared sample API) | Confirm transceiver/pins/power/storage, fresh TTL under disconnect/watchdog, measured render/log load | Real schematic/hardware; no guessed pins or car messages |
| P1 | DTC read before clear (`uds`, diagnostics/CLI/UI) | Exact ECU support, inventory raw/status provenance, no writes; clear requires separate guarded acceptance/recovery | Real-car DTC evidence and write authorization beyond this cleanup |
| P2 | GPIO then automotive buttons / illumination (`ui_platform`, adapter/NVS) | Shared action semantics; held contact/lockout tests; exact mappings/ambient input evidenced | Wiring/captures; no guessed CAN/LIN IDs; BL driver/MOSFET required unless verified control pin |
| P2 | Coding/adaptation / BLE / OTA / Lite | Scoped allowlists, recovery/authentication, phone acceptance or complete link/RAM/board budget | Explicit product design and hardware/evidence; not implied by capability bits |

Bench procedure: record hash, run `ui-test`, exercise touch/RU/layouts/pages and
temporary phone editor; retain config/calibration and factory backup; BL stays
FIXED. Car procedure: staged doctor/F190/identity, bounded read-only evidenced
signals, tagged passive/reference capture, offline replay, reviewed provenance.
No clear/coding or unconfirmed vehicle-button experiments in this milestone.

## Acceptance command record

Actual tooling: Python 3.12 with python-can 4.6.1, can-isotp 2.0.7, udsoncan
1.26.1, cantools 41.0.2; CMake 3.30.2, Ninja 1.12.1, Zig 0.13.0 (Clang 18.1.6),
LLVM AR/RANLIB from installed ESP-Clang; Node editor test; ESP-IDF 5.5.5.

```text
python -m unittest discover -s tests -v
cmake -S core -B core/build-consolidation -G Ninja [explicit installed compiler/AR/cJSON paths]
python tools/verify_repository.py --native-build-dir core/build-consolidation --cmake C:/Espressif/tools/cmake/3.30.2/bin/cmake.exe --ctest C:/Espressif/tools/cmake/3.30.2/bin/ctest.exe --output-dir .local/acceptance/consolidation-20260930-02
# In firmware with the initialized SDK, PYTHONUTF8=1, ccache=0 and local TEMP:
idf.py -B build-ui-platform-managed -DCCACHE_ENABLE=0 reconfigure
idf.py -B build-ui-platform-managed build
python -m minidash_live replay-can engine.rpm vehicle.speed --config <synthetic-config> --identity-snapshot <synthetic-identity> --trace <synthetic-10000.jsonl> --final-age 1
git diff --check
git diff --exit-code HEAD -- firmware/main firmware/sdkconfig.defaults firmware/dependencies.lock firmware/partitions.csv core/CMakeLists.txt core/tests.c core/test_json.c core/test_webui.cjs
```

The full installed compiler configuration and PowerShell environment are in
[development](../development.md). Local acceptance summary/logs are under the
ignored .local directory; raw forensic logs/patches stay outside Git. The initial
failed summary is retained alongside the final PASS summary. All 21 JSON files
in data/profiles/config examples/test fixtures parse; all new documentation links
resolve. All CLI help commands above exit 0. Original artifact and dirty-patch
preservation checks pass. No runtime/UI/firmware source was changed by cleanup.
