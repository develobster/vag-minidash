# Standalone consolidation delivery — 2026-10-05

Development continues on **`integration/standalone`**. This branch starts at
`751d2f8417a6e8dd54a6c6fffff4e522f80db49f` and recovers, through a normal merge, the seven useful
local-only commits ending at `bdd5f7a2067ffb126c4037020f703ea809037594`.
All 12 old remote branch histories are ancestors of the selected base. The
[branch matrix](branch-consolidation.md) and [SHA/count audit](branch-audit-2026-10-05.json)
record the evidence and disposition. Old remote heads and `main` are retained.

## Delivered behavior and preservation

- Recovered mobile host Web Configurator: configuration/SSE/recording, screen and
  theme editor, settings v2/capabilities, guarded Apply and protected Wi-Fi secrets,
  offline shell and browser tests. Host settings remain host-side; firmware remains
  schema 4. No implicit firmware settings sync or host production dependency.
- Resolved watch CLI conflicts by retaining serial instrument-profile publication,
  offline replay and Web publication together. A combined-consumer test proves
  both receive the same runtime snapshot and close correctly; duration starts
  after catalog/setup. Existing registry/identity/provenance and stronger MCD
  scalar/endian/applicability work is preserved rather than replaying old dirty edits.
- Added build-time Car Validation Mode with passive and active presets. It uses
  production acquisition, transport, canonical publication and analytics. Passive
  means hardware zero TX/ACK; active reads four standard sources with a 250 ms
  request floor. Read-only allowlist, selected responder, exact lengths, identity
  gates, bounded retries, timeouts and QUIET remain. No manufacturer profile ships.
- Added bounded raw/exchange trace with sequence/time/raw/decoded/source/validity,
  NRC/timeouts/errors and response time; one locked snapshot feeds local GET
  `/api/validation`. The existing AP has a download link and expires after 600 s.
  Optional GET-only collector preserves private JSONL/boot epochs and reports gaps.
- Preserved display/touch/navigation/NVS/settings/layouts/themes/rotation/Focus,
  the device local editor, all 92 canonical IDs/66 derived nodes, statistics,
  history/events/performance timers/staleness/unavailable states, Mode03 DTC read,
  fail-closed knowledge and existing replay. Corrected two stale FIXED backlight
  descriptions using the existing GPIO4 acceptance report. No source directories
  are moved and no proprietary/local hardware artifacts are imported.
- Fixed the existing headless/no-touch backlight compile failure: the touch-CS
  GPIO collision check is compiled only when touch is enabled. Two display-only
  tasks are explicitly marked unused in headless builds. The enabled-touch GPIO
  protection remains; all seven firmware images and both native toolchains
  were rebuilt after this configuration failure was found.
- Updated README/current architecture/developer instructions to continue from one
  integration branch. Historical PC/VNCI documents remain explicitly historical.
  The acceptance runner handles the Windows analytics `.exe` harness and now
  requires the eighth native validation group.

Production is CAR → OBD-II → protected MiniDash hardware → ESP32-S3 → CAN →
ISO-TP → UDS/OBD-II → acquisition → canonical → Derived Metrics/Engine Analytics
→ history/statistics/events → display. PC/Python/VNCI/ODIS/J2534/phone/Raspberry Pi/
external dongle/host Configurator remain optional development and evidence tools.

## Regression evidence

Full machine-readable results, firmware configuration flags, hashes and memory
measurements are in [consolidation-verification-2026-10-05.json](consolidation-verification-2026-10-05.json).
Local raw logs stay in `.local/`; no car or physical board was connected/flashed.

| Gate | Result |
|---|---|
| Python | 223 tests, 222 PASS and one expected Windows AF_CAN skip; all optional CAN/ISO-TP/DBC/UDS dependencies present |
| Native | Eight CTest groups PASS, including canonical/analytics/acquisition/pipeline/JSON/UI/instruments/backlight and Car Validation |
| Scheduler | Full 12-PID fast scenarios: speed maximum 600 ms, RPM maximum 1100 ms, 195 requests/20 s; unchanged request count. 300 ms overload: 66 requests, timing result unavailable |
| Standalone VirtualBus / exact replay | PASS, 27 TX frames; production C core, VIN/DTC ISO-TP, quiet/loss/fault/reconnect |
| Analytics synthetic replay | 12 scenarios PASS, 66 embedded assertions, 47 independent Python oracle comparisons, 9 malformed inputs |
| Device Web logic / host Web build | PASS; fresh native registry fixture; static production assets and 10 Node model/unit tests |
| Host browser suite | 19 Playwright scenarios PASS, mobile/desktop/settings/offline/reconnect/capability/recording/privacy |
| ASan + UBSan | All eight native groups and both standalone/analytics replay verifiers PASS with recover disabled |
| Generated catalogs/traces | Analytics catalog, instrument profiles and reviewed synthetic traces consistent with committed generated files |
| ESP32 | Seven configurations PASS with zero warnings: ordinary standalone, listen-only, active/passive validation, ST7789 host display, ILI9341 and headless |

The first concurrent full Python run found fixture timing failures: a 100 ms CAN
sample and 1 s candidate could expire during loaded Windows scheduling. The
CAN→UDS test now advances the already-supported runtime/store/availability clock
explicitly; production TTLs and request timing are unchanged. The host API test
accepts an expired candidate as stale while still requiring a hidden value and
correct source. Full acceptance subsequently passed. The watch combined-consumer
test was also rerun after strengthening the merge contract.

Native toolchain: existing Zig 0.13.0 (Clang 18.1.6), CMake 3.30.2/Ninja 1.12.1,
cJSON from installed ESP-IDF 5.5.5. Sanitizers use scoped
[LLVM-MinGW 20260922 / LLVM 23.1.2 UCRT](https://github.com/mstorsjo/llvm-mingw/releases/tag/20260922)
because the installed bare-metal compiler cannot run Windows host sanitizers.
Downloaded archive SHA-256:
`e3ad77d117a4bea19a7a3b333341824d79a5a371004a10e25b8504e7b3047666`.
No global toolchain/dependency upgrade. LeakSanitizer and GNU allocation-wrap
instrumentation are not claimed on Windows; native/sanitizer tests are not RTOS
heap/stack/physical CAN acceptance.

Firmware: installed ESP-IDF 5.5.5 / xtensa-esp-elf 14.2.0_20260121, ESP32-S3.
All four external components match the existing lock hashes; dependency lock
SHA-256 of the committed LF bytes remains
`e8f34402c90bc55635eb48b1bfe8dbe87fd0edc15ad0c6ae85803a4dd7fe896e`.
The Windows worktree uses CRLF; the verification JSON also records its raw hash.
No version solver, partition change, NVS erase or hardware flash. Builds use fresh
SDKCONFIG/build paths with local hash-verified components and component manager
disabled as documented in [development](../development.md).

The acceptance images were built before the consolidation commit. Their embedded
IDF project-version strings are recorded rather than replaced with a later Git
SHA; normalized hashes witness the exact firmware/native source content tested.
Subsequent bench images should record their own commit, preset and image hash.

## Fresh before/after memory comparison

The before image was rebuilt from archived `751d2f8` source with the same local
locked components/toolchain/standalone defaults. The after image includes the
consolidated source and validation export; validation polling itself is disabled
in the ordinary standalone preset. Sizes are linked image/static allocation,
not measurements of live free heap or stack watermarks.

| Measurement | Before `751d2f8` | After ordinary standalone | Delta |
|---|---:|---:|---:|
| App binary | 1,832,960 B | 1,837,200 B | +4,240 B |
| Internal static data + BSS | 176,768 B | 180,128 B | +3,360 B |
| Used DIRAM (including executable DIRAM) | 271,367 B | 274,727 B | +3,360 B |
| Remaining DIRAM | 70,393 B | 67,033 B | −3,360 B |
| App partition free | 1,312,768 B | 1,308,528 B | −4,240 B |

Analytics external PSRAM storage, task-stack budget and normal scheduler demand
are unchanged. New internal RAM holds bounded diagnostic evidence. The native
acquisition/exchange struct sizes and final per-preset image/RAM figures are in
the verification JSON. Real heap/FPS/task watermarks under CAN+AP load still need
physical measurement; no synthetic measurement is called vehicle validation.

## Reproduce and recover

From the integration checkout, use installed/pinned dependencies:

```sh
python -m unittest discover -s tests -v
cmake -S core -B core/build-acceptance -D MD_JSON_SOURCE_DIR=/known/idf/components/json/cJSON
cmake --build core/build-acceptance
python tools/verify_repository.py --native-build-dir core/build-acceptance --output-dir .local/acceptance-new
node --test tests/frontend/model.test.js tests/frontend/units.test.js
node tools/build-web.mjs
npx playwright test
```

On Windows choose the installed C compiler/Ninja explicitly and use the existing
environment paths documented in `docs/development.md`. Sanitized builds use
`-g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer`;
place the scoped LLVM bin/DLL directory on PATH and `ASAN_OPTIONS=detect_leaks=0`.
The verification JSON records the actual commands/configs. Temporary browser data,
compiler caches, firmware output and private traces remain ignored.

For firmware, combine `sdkconfig.defaults` with standalone/listen/validation
presets using new SDKCONFIG and build paths. Download only hash-verified locked
components with `tools/prepare_locked_components.py`. ILI9341 and headless builds
are compile checks, not new hardware acceptance. To recover on a bench, use the
existing known-good app image and saved NVS/calibration/partition checks; do not
reset the repository or erase hardware to address an evidence gap.

## Remaining work in priority order

1. **P0, physical first session:** follow [Car Validation Mode](../validation/car-validation-mode.md).
   Components: protected power/transceiver/firmware/AP. Acceptance: passive TX=0,
   stable display/NVS, no bus fault, then supported standard readings/raw/decoded
   reference/timing/gap evidence. Needs assembled wiring and stationary Tiguan;
   electrical protection/sleep and real CPU/heap watermarks remain physical gates.
2. **P1, manufacturer evidence:** one identified ECU/source at a time in a separate
   read-only reference session. Components: provider definitions, compiled profile,
   provenance and fixtures. Acceptance: observed native endpoints and exact ECU
   identity plus raw lengths/endian/scaling/units/invalid/TTL/reference evidence.
   DQ500/Haldex/MQB/retard/misfires/charge bindings remain disabled until reviewed.
3. **P1, evidence completeness:** measure CAN queue drops, AP load and ring gaps;
   preserve originals and replay offline. A verified independent passive recorder
   is needed if the short device buffer cannot capture the required wire evidence.
4. **Backlog only:** persistent device recording, protected automatic power/sleep,
   additional evidence-backed manufacturer sources. Do not add cosmetic features,
   raw-command controls, DTC clear/coding/adaptation or a host runtime dependency
   as a continuation of this consolidation.

## Git delivery

The consolidation merge retains both parent histories. Final commit/remote
verification is reported after all gates complete; the integration worktree must
be clean. Original dirty worktrees/stash/private artifacts remain preserved and
are not included merely to clean their status. Repository `AGENTS.md` now directs
subsequent tasks to this one development branch.
