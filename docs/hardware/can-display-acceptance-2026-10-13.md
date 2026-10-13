# CAN display startup: bench acceptance and build protection, 2026-10-13

## Outcome and evidence boundaries

With explicit follow-up authorization, the assistant flashed the device over
COM3. ST7789 image and XPT2046 touch work; the owner confirmed both. A
70-second UART observation showed no panic/repeated ROM boot. Read-only status
queries around 10 and 50 seconds both returned `boot=1`. Saved brightness 70%
and the existing touch calibration loaded; touch navigation appeared in logs.

CANH/CANL remained disconnected. This is startup/display/touch bench evidence,
not vehicle/CAN reception validation. The original display-prototype worktree
and its uncommitted changes were preserved. No NVS erase, `erase-flash`, or
`fullclean` was performed. Raw serial captures and backup images stay local.

## Bootloader dependency discovered during flashing

The first `app-flash` wrote version `0222a01` at 0x10000, 1,843,728 bytes,
with esptool hash verification. The reset loop stopped, but another 70-second
capture still showed ROM output only; UART commands received no replies.

The old USB Serial/JTAG bootloader calls `release_default_console_io()` in
ESP-IDF 5.5.5 `bootloader_console.c`. That disables UART0 TX output and
disconnects RX. The default-UART application does not restore those pin
connections itself. The earlier app-only console-transition assumption was
therefore incorrect for this board.

`bootloader-flash` then wrote the matching UART bootloader at 0x0, 22,272
bytes, with verified hash. UART logs and commands became available. The
bootloader erase range was 0x0..0x5fff; the app erase range started at 0x10000.
Neither write addressed the partition table at 0x8000, NVS at 0x9000, or PHY
at 0xf000. A subsequent esptool read of all 4096 table bytes matched the
original full backup exactly. Normal persistence remains active, so preserving NVS from erasure
does not prohibit ordinary settings saves during user navigation.

For first use of this UART profile, a console-profile change, or an unknown
bootloader, write both bootloader and app. Subsequent updates with this same
bootloader may write only the app. The [manual Windows guide](manual-flashing-ru.md)
records this verified procedure and NVS-preserving recovery boundaries.

Sanitized application milestones:

```text
esp_psram: Found 8MB PSRAM device
esp_psram: SPI SRAM memory test OK
app_init: App version: 0222a01
md_backlight: INIT gpio=4 frequency=5000 resolution=12 gamma=2.0 boot=off
MINIDASH_BOOT count=1 reset=POWERON idf=v5.5.5
md_ui: Display initialized with ST7789 backend at 320x240 rotation=2
md_ui: STARTUP end elapsed_ms=2032 brightness=70
md_ui: UI ready with 320 x 240 partial buffers and 100 ms model refresh
md_ui: Touch input ready; calibrated edge controls active
md_backlight: STATUS gpio=4 frequency=5000 duty=2007/4095
```

The human reported "Изображение и тач работают". Serial RTS reset is not
evidence of a real cold USB power-cycle test.

## Automatic prevention in normal firmware builds

`firmware/main/CMakeLists.txt` now:

1. Applies GCC `-Werror=stack-usage=3072` to `display_app.c`; the original
   4384/5840-byte own frames fail compilation.
2. Emits `-fstack-usage` reports for UI and configuration load/decode.
   `tools/check_ui_stack_budget.py` runs automatically after compiling the
   main component, sums the two identified paths and reserves another 1024
   bytes for additional calls. Missing evidence, dynamic frames or budget
   overflow fail the build. Headless/panel-smoke modes skip the inactive UI.
3. Defines `MD_UI_TASK_STACK_BYTES=8192` once and uses it for both
   `xTaskCreate` and the checker; the allocated stack and check cannot silently
   diverge through separate numeric literals.

| Path | Measured frames | Extra reserve | Task stack |
|---|---:|---:|---:|
| UI → diagnostics | 1440 + 176 = 1616 | 1024 | 8192 |
| UI → saved config → decode → validate | 1440 + 608 + 3104 + 640 = 5792 | 1024 | 8192 |

This covers the identified regression and configuration growth on these paths.
It is not a complete call-graph proof or guarantee against every stack failure.
If new compiler optimizations inline/remove named frames, review the checker
rather than silently accepting missing reports. Hardware soak remains useful.

`tools/check_ui_stack_regression.py` replays the actual compiler command in
temporary files. It requires the automatic guard, accepts the fixed source,
and rejects the actual `751d2f8` source for stack usage. It does not open hardware
or overwrite configured build objects/images. The original-code replay passed.

## Reproduction and verification

Windows, ESP-IDF 5.5.5, installed Python 3.12 and Xtensa GCC 14.2.0:

```powershell
. 'C:\Espressif\frameworks\esp-idf-v5.5.5\v5.5.5\esp-idf\export.ps1'
Set-Location 'C:\Users\user\vag-minidash-can\firmware'
idf.py -B build-can-diagnostic -D SDKCONFIG=sdkconfig.can-diagnostic.generated -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.listen-only;sdkconfig.uart-console' build
python ..\tools\check_ui_stack_regression.py --build-dir build-can-diagnostic
Set-Location ..
python -m unittest tests.test_firmware_stack_budget -v
```

The build passed with both automatic nested-path checks: 1616 + 1024 and
5792 + 1024, each below 8192 bytes. The own-frame guard and original-source
replay passed. All four tests in `tests/test_firmware_stack_budget.py` passed,
covering the measured original overflow, future configuration growth and missing
frame evidence. The original sdkconfig remains
separate from generated UART configuration; CAN TX15/RX5, disabled STANDARD_OBD,
LCD/touch/PSRAM parameters, persistence schemas and partition layout are unchanged.

## Local/remote delivery and manual

The clean CAN worktree carries the fix, guards, tests and manuals together in
`fix/can-display-startup`. At the owner's request the actual development branch
`feature/engine-analytics` is fast-forwarded to the same commit locally and on
origin. Refresh/check the remote ancestor before pushing; never overwrite a
concurrent divergent update. No force push, main merge or changes to the other
active worktrees are involved. The CAN worktree is attached to the updated
development branch for subsequent work.

Final remote equality is checked with `git ls-remote`; the installed application
version is checked against the final commit after an authorized app-only update.
The guide is committed at `docs/hardware/manual-flashing-ru.md`, linked from
README/status/documentation entries, and copied to this chat's local outputs.
It covers preparation, branch updates, separate configuration/build, protection
checks, bootloader/app writes, expected logs, common errors and recovery.

## Remaining work

1. Cold USB boot and longer soak: a real power cycle and extended navigation,
   with visible UI/touch and no new panic/reboot. Requires board/human evidence;
   serial reset and the recorded 70 seconds do not cover all power conditions.
2. Additional UI/Web memory stress: preserve compiler gates and inspect runtime
   stack headroom when enlarging schema/rendering/analytics or testing AP/Web.
   Web stress acceptance was not performed in this task.
3. Vehicle connection: complete the existing wiring/termination/power and
   first-session gates separately. Real CAN RX/TX and vehicle behavior remain
   untested; this report authorizes no vehicle connection.

## Provenance and compatibility

Sources: local compiler/build/UART evidence, human observation, installed
ESP-IDF 5.5.5 bootloader console code, and installed IDF Monitor key configuration
for the guide's logging shortcut. No external implementation/dependency, new
runtime allocator, API/persistence migration or license change was introduced.
