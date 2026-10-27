# CYD integration: preliminary work, 2026-10-27

**Incomplete: do not flash or use this branch on CYD.** The checked-out baseline
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

## Checks performed

- Generated CAN and VAG sources: both `--check` commands passed.
- Python unittest discovery: 237 tests, 21 skipped, no failures.
- JavaScript frontend tests: 10 passed.
- CYD mode policy native test: passed with Zig C compiler and CTest.
- Full native build failed on Windows because `pthread.h` is unavailable for
  `test_can_pipeline.c`; no full native suite or sanitizers were run.
- No firmware build, firmware size, RAM budget, CI run or hardware test was
  performed for this branch. COM6 exposed a CH340 UART but was not identified
  as the CYD; no serial connection, reset, flash, NVS or eFuse operation occurred.
  An old report of version 0.5.1 does not establish what is installed now.

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
