# CYD integration: preliminary work, 2026-10-27

**Historical preliminary report: see the 28 October update below. Do not flash
or use this branch on CYD until the remaining acceptance gates pass.** The baseline
is `origin/integration/standalone` at `c10b76b`; the CYD reference is
`origin/feature/hardware-cyd-esp32` at `30eda5b`; merge base is `b599fbc`.
Both remote refs were fetched. No branch was merged, pushed or rewritten.
The original worktree had unrelated changes and was left intact.

## Subsystem map and remaining integration

| Area | CYD reference | Integration baseline and unresolved work |
|---|---|---|
| Board | ESP32, 4 MB flash, ILI9341, XPT2046, separate SPI buses and CYD partition/config/lock | No CYD build profile here yet. Review the actual board revision and transfer isolated build configuration without changing S3. |
| Display and UI | Partial double DMA buffers, single active scene, themes, pixel fonts, touch calibration, ambient light | Baseline has five automotive screens and different renderer/navigation. Reconcile lifetimes, display geometry, redraw and RAM; preserve saved IDs. |
| Settings and Web | Schema 4, persisted demo, calibration, themes, manual AP and Display Studio | Baseline has newer controls and diagnostics. Merge APIs and page intentionally; test old NVS and cancel/preview behavior. |
| CAN and diagnostics | Demo/monitor/armed mode policy; GPIO22 TX, GPIO27 or 35 RX candidates; explicit confirmation for TX | Baseline has RX owner, queue, mailbox, epoch, loss counters, bus-off recovery and bounded diagnostics. Wire interlocks into that controller and test source changes, stale demo clearing, and RX ownership. Candidate GPIOs require board verification. |
| Vehicle data | CYD simulation and guarded acquisition | Baseline PQ/MQB signals are candidates, production profile unprovisioned; Drag requires trusted speed. Preserve quality and provenance. |
| Memory | No PSRAM, no large analytics buffer, two 320x8 RGB565 buffers | Measure static RAM, queues, stacks, heap and app partition after building; do not assume S3 sizing fits. |
| Sleep | Board-specific backlight and pins | Baseline sleep is optional and off by default. GPIO wake and power behavior are not validated for CYD. |
| Tests and CI | CYD mode, GPIO, UI and browser tests in reference | Baseline CI builds S3 variants. Add actual ESP32 builds only after the board profile is integrated; no CYD CI build exists yet. |

`git merge-tree` identified conflicts in 12 files, including the acquisition
controller, UI, Web, and build configuration. These require semantic resolution;
an automatic merge is not an acceptance result. `cyd_mode.[ch]` and its original
host test are copied from `30eda5b` as a staged policy only: **they are not
linked into firmware and do not enforce a live interlock**. Historical hardware
reports are retained for provenance, not proof of current hardware state.
The CYD reference simulation and captive DNS modules and their host contract
test are also staged, but are not wired into firmware. The reference's direct
TWAI stop/uninstall on a source change must be replaced with commands to the
new sole RX owner; copying it would race that owner. The reference CYD config
enables standard OBD at build time, so the runtime arm interlock must be proven
before it can be used; the integration must fail closed during development.
The S3 application ring holds 512 samples and its TWAI queue 128 frames; any
CYD reduction must be documented, tested and verified with real loss counters.

## Checks performed

- Generated CAN and VAG sources: both `--check` commands passed.
- Python unittest discovery: 237 tests, 21 skipped, no failures.
- JavaScript frontend tests: 10 passed.
- CYD mode policy native test: passed with Zig C compiler and CTest.
- CYD simulation and captive DNS host contract: passed. These tests exercise
  the reference modules only, not a running device or integrated firmware.
- Full native build and all 20 configured CTest cases passed with cJSON on
  Windows. The original realtime pipeline test initially failed to compile
  without pthread; a Windows thread adapter was added and its 100,000-frame
  concurrency test passed. LVGL software rendering and sanitizers were not run.
- The generated C scalar decoder and Python decoder agreed on 13,000 seeded
  input vectors across the declared layouts and four boundary layouts, using a
  locally compiled Windows DLL. This does not validate signal meanings on a car.
- No firmware build, firmware size, RAM budget, CI run or hardware test had
  been performed at the time of the preliminary report. COM6 exposed a CH340 UART but was not identified
  as the CYD; no serial connection, reset, flash, NVS or eFuse operation occurred.
  An old report of version 0.5.1 does not establish what is installed now.
- Subsequently the unchanged S3 standalone firmware baseline built with local
  ESP-IDF 5.5.5 and locally prepared pinned components, target `esp32s3`.
  Application size was 1,896,208 bytes in a 3,145,728-byte partition, leaving
  1,249,520 bytes (40%). This is not a CYD or unified build. No flash was run.

### Subsequent read-only USB status

After the user confirmed the USB connection, COM6 returned MiniDash CYD status
at 115200 baud. A passive read and the documented `status` command were used;
no flash, NVS, eFuse or CAN operation was requested. The device reported IDF
5.5.5, display and touch ready, calibration present, Wi-Fi off, DEMO off,
CAN ready, TX not armed, UDS off, GPIO22/27, 500 kbit/s, RX=0 and TX=0.
Reported heap was 139248 bytes, minimum 138408 and largest block 110592;
reported `stack_free` was 1972 (the status output does not identify its units
here). These are readings from the currently installed firmware, not from the
new branch, and do not establish hardware wiring or CAN functionality. The
application version was absent from this response and remains unknown.

## Acceptance sequence still required

1. Integrate board configuration and independent lock/build directories; build
   ESP32 and all existing S3 variants with ESP-IDF 5.5.5. Record binary size,
   partition headroom, static RAM, queues and stack allocations.
2. Reconcile UI and Web, restore calibration, NVS and demo settings across old
   configurations; run both existing and CYD UI and browser tests.
3. Integrate mode policy with RX ownership and test DEMO to listen-only,
   clearing synthetic values, TX authorization/revocation, queue loss and
   mailbox behavior, recovery, C/Python parity and trusted-speed Drag.
4. Add an explicit ESP32 CYD CI target and run native, sanitizers, static
   analysis, generation checks and both firmware build matrices.
5. Before any separate flashing decision, identify the board and current
   partition layout; prepare a private full-flash backup, verify it against
   flash, record its hash and recovery procedure, and check that the new
   partition layout preserves NVS. A full image at offset zero can overwrite
   partitions; no update command is safe to recommend until these checks pass.
6. On an isolated bench verify display/touch, power, memory, passive RX and loss
   counters with a generator, recovery and TX interlocks. Measure entire-system
   current before making sleep claims. Then, only with an approved power and
   transceiver wiring plan, compare passive Tiguan observations with ODIS;
   candidate signals remain unconfirmed until independently evidenced.
   Active diagnostics requires a separate decision after wiring, bus behavior,
   interlocks and bench results are verified. No car operations were performed.

## Integration work in progress, 28 October

The CYD branch was merged for review into the isolated integration worktree;
conflicts were resolved across display, settings, Web, CAN and build files.
The CYD profile uses the reference 4 MB partition layout and ILI9341/XPT2046
pin map. CAN source changes are routed through the RX owner's stop command,
reinitialize acquisition, clear the canonical store and publish an empty UI
frame. TX still requires the CYD arm interlock. The separate
`minidash_cyd --listen-only` preset disables standard OBD at build time and
rejects arming. Deep sleep is disabled for CYD pending board-specific review.
The serial arm confirmation must match the configured RX pin: the historical
GPIO27 confirmation is rejected for GPIO35 and vice versa. This software
comparison does not verify the actual wiring.

The five automotive screens are allocated one at a time on CYD. Demo rendering
is labelled and remains outside the trusted vehicle database; Drag does not
consume demo speed. CYD retains its previous explicit limitation: minute/trip
derived analytics are unavailable without the large S3 analytics buffer.
The RX ring remains 512 entries (12,304 bytes in the ESP32 object), the TWAI
driver queue 128 entries, and the triple mailbox 10,144 bytes. The CAN RX and
vehicle task stacks are 4,096 and 8,192 bytes respectively; UI controller is
8,192 bytes, touch task 4,096 bytes. The additional allocations and concurrent
Wi-Fi need actual heap/stack measurements on a flashed board before acceptance.

Preliminary host verification: all 25 configured CTest tests passed, including
CYD core, acquisition, orientation, mode policy and the concurrent pipeline;
both S3 and CYD automotive software-render tests passed at 320x240 and 240x320.
239 Python tests passed with 21 skipped, frontend JS 10/10, and the Web editor
logic test passed. The Chromium mocked-device test passed 45 checks, and 23
native tests passed with undefined-behavior sanitizer (without cJSON tests in
that configuration). These are synthetic tests. Four local ESP-IDF 5.5.5
target builds passed, using the local pinned components:

| Variant | Application bytes | App partition free bytes |
|---|---:|---:|
| CYD, guarded OBD | 2,024,272 | 2,104,496 |
| CYD, listen-only | 2,024,096 | 2,104,672 |
| S3 standalone | 2,008,720 | 1,137,008 |
| S3 listen-only | 2,008,704 | 1,137,024 |

The CYD guarded image reports 132,980 bytes static DRAM and 113,519 bytes
static IRAM. Linker-reported remaining static-region space is not runtime heap.
The S3 sleep-enabled variant was not rebuilt locally. CI has not been run.

Outstanding: test integrated mode changes and loss counters rather than only
the policy; complete all target build variants and current size reports; test
touch/navigation, historical NVS migration and Web with concurrent CAN under
memory pressure; run address sanitizer, static analysis and CI; independently review
the changed HAL observer on real ESP32. Do not use for vehicle operation or
flash until these gates and backup/partition verification are complete. The
current USB status belongs to the previously installed firmware, not this code.
The attempted Windows ASan configuration failed at the compiler check because
the available Zig toolchain did not supply `__asan_init` and
`__asan_version_mismatch_check_v8`; no ASan result is claimed.
The installed clang-tidy could not analyze the ESP32 build database: it rejected
GCC-only flags and could not resolve the target `stdio.h`; no static-analysis
pass is claimed. The CI Linux job retains its native clang-tidy check.

### Storage regression, 29 October

Added host tests of the actual `ui_configuration_nvs.c` adapter for both CYD
and S3 configurations with a transactional NVS stub. The tests cover a legacy
settings fallback, a saved pixel theme and user-page title, brightness, corrupted
configuration fallback without overwriting the damaged record, and open/write/
commit failures without changing the previously committed mock record. The same
test exercises the actual touch-calibration adapter: independent storage in the
same namespace, matching geometry and rotation, and checksum rejection. Added a
separate CYD-configured run of the 100,000-frame concurrent ring and mailbox
test, including overflow, sequence wrap and high-water bounds. Both platform
variants run the same fixed queue capacity; host execution does not establish
hardware throughput or the accuracy of driver loss counters. All 28 configured
native CTest cases passed; 26 cases passed with UBSan (the latter
build does not include cJSON). The stubbed legacy loader is not a migration test
of actual flash, and these tests do not demonstrate persistence across power
loss, validate the connected board's NVS, or replace a verified backup.

The same-day follow-up adds a source-generation field to the internal dashboard
mailbox. CYD tags frames with the applied generation; the UI clears history and
rejects frames from any previous generation, including a quick toggle back to
the original mode. Host tests exercise the stale-frame filter and check that
the generation stays consistent in snapshots during 100,000 publications.
This does not prove a physical TWAI transition: testing the driver owner,
source switch and loss counters remains necessary.

### Remote validation, 29 October

The isolated `integration/cyd-unified` branch is authorized for publication by
the owner. Its push trigger is included in the validation workflow so future
updates can run the host and firmware matrix without merging into the primary
branch. The local YAML parses successfully; consult the actual workflow run for
its status rather than treating configuration as a passed CI run. Backups and
device-specific records remain local and are not committed.

### Bench and Tiguan acceptance, after a separate flashing decision

1. Before wiring CAN, photograph and identify the board and transceiver, check
   GPIO22/27 continuity and absence of conflicting loads, supply voltage, RS
   state and any onboard terminator. Verify protected power and common reference.
   The documented pin map is a candidate for this board, not a measurement.
2. On USB alone, record boot version, free/minimum/largest heap and task stack
   watermarks. Verify all screen orientations, touch calibration and saved NVS,
   themes, backlight and Wi-Fi. Repeat while exercising Web and switching pages;
   compare heap and stack minima and check for resets or allocation failures.
3. With an isolated terminated CAN generator, use the listen-only image. Verify
   traffic at 500 kbit/s, no transmission or ACK from the device, RX and drop
   counters under measured load and mailbox responsiveness. A controlled
   bus-off/recovery test requires a separately authorized isolated active-mode
   setup; do not assume listen-only can induce it. Test DEMO -> CAN -> DEMO,
   invalid/stale data, screen history clearing and rejection of arming in the
   listen-only image. Record generator counts and device counters separately.
4. Only with a reviewed automotive power and passive CAN wiring plan, perform a
   separate Tiguan passive RX session. Record bus rate, counters, freshness and
   candidates; compare time-aligned speed, RPM and temperatures with ODIS and
   distinguish gateway filtering from absence of a signal. Keep VIN and traces
   private. Candidate layouts are not confirmed merely because numbers appear.
5. Permit active OBD diagnostics only after the power, transceiver, wiring,
   bus load, listen-only behavior and interlocks have passed bench and passive
   checks, with explicit separate authorization and a reviewed request scope.
   Do not infer safe sleep current, losslessness, or measurement accuracy from
   synthetic tests; measure these on the actual installation.

### Build and future recovery preparation (not executed on the board)

In an exported ESP-IDF 5.5.5 environment, from the repository root:

```powershell
python tools/build_firmware.py minidash_cyd
python tools/build_firmware.py minidash_cyd --listen-only
python tools/build_firmware.py esp32_s3
```

For local hash-verified components, pass `--components <absolute-path>` with
`IDF_COMPONENT_MANAGER=0`. Separate build and generated config paths prevent
the profiles from inheriting one another. Inspect the generated sdkconfig and
app partition headroom before considering a board update.

Only after the outstanding tests and a separate decision to flash: disconnect
CAN and external supply, confirm COM6 and board identity, and save the **current
full flash** privately. The following reads may reset into the ROM bootloader;
do not commit these files, which can contain credentials and private settings:

```powershell
$idfPython = Join-Path $env:IDF_PYTHON_ENV_PATH 'Scripts\python.exe'
& $idfPython -m serial.tools.list_ports -v
& $idfPython -m esptool --chip esp32 --port COM6 read_mac
& $idfPython -m esptool --chip esp32 --port COM6 --baud 115200 --after no_reset read_flash 0 0x400000 cyd-before-update.bin
& $idfPython -m esptool --chip esp32 --port COM6 --baud 115200 --before no_reset --after no_reset verify_flash 0 cyd-before-update.bin
Get-Item cyd-before-update.bin | Select-Object Length
Get-FileHash cyd-before-update.bin -Algorithm SHA256
```

Require a 4,194,304-byte file, successful device comparison and a separately
stored hash. Keep a second private copy. Read the current partition table from
the backup and compare its NVS offset/size and factory app offset/capacity with
`firmware/partitions.cyd.csv`; verify compatibility of the existing bootloader
and new application as well, including whether secure boot or flash encryption
is enabled. Do not change eFuse, and do not proceed on a mismatch or an unknown
security state. After those checks,
the app-only operation (which does not write the NVS address range) is:

```powershell
& $idfPython -c "from pathlib import Path; p=Path('cyd-before-update.bin').read_bytes(); Path('cyd-partition-table.bin').write_bytes(p[0x8000:0x9000])"
& $idfPython "$env:IDF_PATH/components/partition_table/gen_esp32part.py" cyd-partition-table.bin
```

The following commands are for a later separately authorized update:

```powershell
& $idfPython -m esptool --chip esp32 --port COM6 --baud 115200 --before no_reset --after no_reset write_flash 0x10000 build-minidash_cyd-listen/vag_minidash.bin
& $idfPython -m esptool --chip esp32 --port COM6 --baud 115200 --before no_reset --after hard_reset verify_flash 0x10000 build-minidash_cyd-listen/vag_minidash.bin
```

This is conditional preparation, not a claim that the prerequisites have
passed. If recovery is separately authorized and needed, verify the backup hash
again and restore it with `& $idfPython -m esptool --chip esp32 --port COM6 --baud
115200 --after no_reset write_flash 0 cyd-before-update.bin`, followed by
`& $idfPython -m esptool --chip esp32 --port COM6 --baud 115200 --before no_reset
--after hard_reset verify_flash 0 cyd-before-update.bin`. Full restoration
overwrites all current flash contents including NVS. Neither operation changes
eFuse. No write, erase, CAN connection or diagnostic request was performed here.
