# CAN bench build: black display and UI stack regression, 2026-10-13

Follow-up completed with explicit flashing authorization: [bench acceptance,
matching UART bootloader, build guards and branch delivery](can-display-acceptance-2026-10-13.md).
The owner confirmed image and touch. Use the [manual flashing guide](manual-flashing-ru.md).
The report below records the earlier read-only phase. The app-only assumption
for a USB-to-UART console transition was disproved on this board: first flash
the matching UART bootloader as well as the app.

## Finding and limits of the evidence

The `751d2f8` engine-analytics build contains a demonstrable UI task stack
overflow. The fixed image was initially prepared for bench verification;
display/touch acceptance is now recorded in the follow-up above. CANH/CANL must remain
disconnected during this work.

The local worktree was clean, at detached
`751d2f8417a6e8dd54a6c6fffff4e522f80db49f`, before creating
`fix/can-display-startup`. The separate `feature/esp32-display-prototype`
checkout and its uncommitted changes were not edited. No device flash writes,
NVS erasure, `erase-flash`, or `fullclean` were performed during diagnosis.

The owner-provided full flash backup was independently verified:

- File: `C:\Users\user\minidash-backup\flash-before-can.bin`.
- Length: 16,777,216 bytes.
- SHA256: `5B7803BCE8E6E34A01EC7701CC6444B6BBA1D7344C04A044441F92F7EDA5A66B`.
- Application descriptor: `vag_minidash`, version `e4bb06c`, ESP-IDF `v5.5.5`,
  compiled October 5, 2026 at 23:39:58.

Commit `e4bb06c58295656dfdaa8fa11b0535446ef807a5` is titled
"Keep the UI signal snapshot off the controller task stack". It allocates the
signal snapshot once outside the task stack and passes it into diagnostics.
It is not an ancestor of `751d2f8`; comparing the two versions shows that the
CAN worktree again has a stack-local signal store and a second stack-local
store inside `diagnostics_snapshot`. The backup therefore supplies a concrete
explanation for the previously working display.

## Measured stack violation

The actual build's `compile_commands.json` entries were replayed with GCC
`-fstack-usage`, writing objects and `.su` reports outside the repository build.
Compiler: installed Xtensa ESP GCC 14.2.0, ESP-IDF 5.5.5, original `-Og` flags.
The original ELF's DWARF types confirm a signal store of 2952 bytes and a
vehicle status of 2712 bytes (92 signals).

| Function | Original frame, bytes | Fixed frame, bytes |
|---|---:|---:|
| `ui_task` | 4384 | 1440 |
| `diagnostics_snapshot` | 5840 | 176 |
| `vehicle_task` | 2880 | 2880 |
| `md_configuration_storage_load` | 608 | 608 |
| `md_configuration_decode` | 3104 | 3104 |
| `md_configuration_validate` | 640 | 640 |

`md_display_app_start` gives `ui_task` 8192 **bytes** of stack. Its direct call
to diagnostics requires at least 4384 + 5840 = 10,224 bytes, before nested
functions and the task wrapper. Loading an existing configuration also has a
potential call chain of 4384 + 608 + 3104 + 640 = 8736 bytes. This second path
can overflow before the startup reveal and before the backlight is lit.

These are static compiler frame measurements, not hardware stack high-water
measurements or a decoded panic backtrace. They prove the budget violation;
the direct hardware panic site has not yet been captured.

## COM3 observation and console routing

`idf.py -p COM3 monitor` opened successfully using the original ELF. Windows
identifies COM3 as `USB-Enhanced-SERIAL CH343`, VID 1A86 / PID 55D3. Only ROM
messages arrived, repeatedly:

```text
rst:0xc (RTC_SW_CPU_RST),boot:0x8 (SPI_FAST_FLASH_BOOT)
Saved PC:0x4037bf66
mode:DIO, clock div:1
entry 0x403c8948
```

A separate 15-second UART capture recorded 14 such resets. The saved PC
symbolizes to `esp_cpu_wait_for_intr`; it is not a useful crashing call site.
It does not by itself prove a watchdog or identify the failing task.

The original configuration routes application/IDF logs and panic output to
`CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG`, with no secondary output. The CH343
UART bridge is a different interface, so COM3 shows ROM output but hides the
application panic. No native ESP USB console port was enumerated at inspection.

`sdkconfig.uart-console` is a new optional final defaults overlay. It selects
UART0 as the primary console at 115200 baud, using ESP32-S3 default GPIO43/44,
and retains native USB Serial/JTAG as secondary output. Interactive commands
use COM3 with this overlay. It does not change the normal shared defaults.

## Display, backlight, CAN and configuration review

The separate diagnostic configuration preserves the original non-console
settings; a normalized comparison found only console-related differences.

- ST7789: rotation 2, landscape 320x240, gap 0/0, inversion disabled,
  SCLK 12, MOSI 11, MISO 13, CS 10, DC 8, reset 9.
- XPT2046: enabled, shared SPI with CS 14. Its initialization occurs after UI
  startup, so touch initialization is not the first failing startup phase.
- Backlight: GPIO4, active high, 5 kHz / 12-bit LEDC. `app_main` initializes it
  off. `startup_effect` restores the loaded brightness after creating the UI.
  A startup crash can therefore leave the panel unlit. There is no evidence
  supporting a pin/polarity change; these were kept as previously configured.
- PSRAM: octal 8 MB, 80 MHz, initialized at boot. Flash: configured QIO at
  80 MHz / 16 MB; ESP-IDF's generated flash argument is DIO. This QIO/DIO
  combination already exists in the original build and is not evidence of
  a new memory-mode fault.
- CAN: enabled on TX 15 / RX 5; no collision with the reviewed display/touch
  pins. `install_can(false)` selects `TWAI_MODE_LISTEN_ONLY`.
  `STANDARD_OBD` is disabled, so the guarded switch to normal mode cannot run;
  `send_can` also rejects sending while normal mode is false. Host telemetry
  injection remains unavailable with CAN enabled.
- `vehicle_task` has its own 8192-byte stack, delays each loop and does not
  initialize or power down the LCD. Its measured own frame is 2880 bytes.
  This review does not establish an exhaustive bound on every vehicle call.
- NVS initialization deliberately falls back without erasing records. This
  patch changes neither persistence APIs nor record schemas/migrations.

The backup partition bytes at 0x8000 match the diagnostic build's table when
the 3072-byte generated binary is padded to 4096 with FF:

| Partition | Offset | Size |
|---|---:|---:|
| nvs | 0x9000 | 24 KiB |
| phy_init | 0xf000 | 4 KiB |
| factory | 0x10000 | 3 MiB |

There is consequently no partition migration or reason to erase NVS.

## Minimal implementation and ownership

Only `display_app.c` changes runtime behavior:

1. The UI render signal snapshot becomes a static, single-owner buffer.
2. Diagnostics summarizes that same snapshot instead of making another copy.
3. The diagnostics vehicle copy becomes another static, single-owner buffer.

Both functions run only in the sole UI controller task; no concurrent reader
uses these scratch buffers. The live signal store remains protected by its
existing critical section, and vehicle status by the existing mutex. No new
per-frame allocation, larger task stack, or cross-task pointer is introduced.
The fixed buffers consume 5664 bytes of BSS; linker and runtime allocation
acceptance still require the bench checks below. Public APIs, CAN mode, LCD
init commands, backlight policy and saved settings formats are unchanged.

## Build and verification

Work was performed on Windows with ESP-IDF 5.5.5 and its installed Python
3.12 environment. The build uses separate output/configuration paths; original
`firmware/sdkconfig` retains its SHA256
`68A3AB20B5CD19E4F76FEDC90718B15EB5E8366A06DEF3A9A23D0E8C427750CF`.

```powershell
. 'C:\Espressif\frameworks\esp-idf-v5.5.5\v5.5.5\esp-idf\export.ps1'
Set-Location 'C:\Users\user\vag-minidash-can\firmware'
idf.py -B build-can-diagnostic -D SDKCONFIG=sdkconfig.can-diagnostic.generated -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.listen-only;sdkconfig.uart-console' build
```

The generated configuration is deliberately local/ignored. ESP-IDF defaults
do not reliably replace choices already stored in an existing `sdkconfig`;
using this distinct generated file is part of the procedure. Do not apply the
UART overlay by merely appending it to an old configured build.

Full ESP-IDF build completed successfully (1705 application build steps plus
the bootloader sub-build). Application binary: 1,843,728 bytes (`0x1c2210`),
inside the 3 MiB factory partition, with 1,302,000 bytes free (41%). No source
compile error or warning was reported in the build logs. The app-only flash
argument file was checked: it contains only `0x10000 vag_minidash.bin`.
Completed checks: original ELF type sizes, before/after GCC stack frames,
UART boot-loop capture, backup length/hash, exact partition comparison,
generated console/CAN/LCD/PSRAM settings, and `git diff --check`.
No synthetic tests are presented as physical display or vehicle evidence.

## Safe device verification

Keep CANH/CANL disconnected. Close any other COM3 terminal before flashing.
The following writes the application at 0x10000 only, leaving NVS, PHY data,
the bootloader and partition table outside the written region:

```powershell
idf.py -B build-can-diagnostic -D SDKCONFIG=sdkconfig.can-diagnostic.generated -p COM3 app-flash
idf.py -B build-can-diagnostic -D SDKCONFIG=sdkconfig.can-diagnostic.generated -p COM3 monitor
```

For an initial USB-to-UART transition, first run `bootloader-flash` with the
same build/configuration/port arguments. The USB bootloader disables UART pin
routing; a matching UART bootloader is required for this board's COM3 console.
Expected application milestones are `MINIDASH_BOOT`, backlight `INIT`,
`Display initialized with ST7789`, `STARTUP begin/end`, `UI ready`, and touch
initialization. `MINIDASH_READY` alone is printed before UI setup and does
not establish display acceptance.

After UI ready, type read-only commands `status` and `backlight-status`.
Observe at least 60 seconds without new boot banners, then check cold USB boot,
touch navigation, existing brightness/theme and touch calibration. Do not run
configuration reset/recovery commands or change brightness merely to collect
the first log. Quit monitor with Ctrl+].

If it still fails, collect the full UART log from ROM reset through the first
failure and the following boot. Include `Guru Meditation`, stack-overflow
messages, `Backtrace`, PSRAM/heap allocation failures, and `md_backlight`,
`md_ui`, `md_vehicle` lines. Include the response to `backlight-status` if the
console is alive. Report whether the backlight glows and whether reboot banners
repeat. Symbolize with this diagnostic build's ELF, not the original ELF.

## Recovery preserving NVS

The complete 16 MB backup remains the authoritative recovery source. The
matching partition layout also permits an application-only rollback. These
commands extract the original 3 MiB factory region locally; verify the backup
hash above before doing so. Execute rollback only if needed:

```powershell
Set-Location 'C:\Users\user\vag-minidash-can\firmware'
$source = [System.IO.File]::OpenRead('C:\Users\user\minidash-backup\flash-before-can.bin')
try {
    $source.Seek(0x10000, [System.IO.SeekOrigin]::Begin) | Out-Null
    $originalApp = New-Object byte[] 0x300000
    $offset = 0
    while ($offset -lt $originalApp.Length) {
        $read = $source.Read($originalApp, $offset, $originalApp.Length - $offset)
        if ($read -le 0) { throw 'Incomplete backup application region' }
        $offset += $read
    }
    [System.IO.File]::WriteAllBytes((Join-Path (Get-Location) 'build-can-diagnostic\original-app.bin'), $originalApp)
} finally { $source.Dispose() }
python -m esptool --chip esp32s3 -p COM3 -b 460800 write_flash 0x10000 build-can-diagnostic\original-app.bin
```

The checked read loop is also usable in Windows PowerShell 5.1.
Restoring this application restores its original USB-primary
console too. App-only rollback does not restore NVS bytes, intentionally.
There is no instruction here to overwrite the complete flash backup onto the
device or to erase any partition.

## Remaining work in priority order

1. **Bench acceptance of this patch:** application-only flash, UART log, 60 s
   stability, cold boot and display/touch/settings checks. Affects UI startup
   and fixed scratch-buffer memory; success requires visible UI and stable
   console with no panic/allocation failure. Requires the physical board and
   owner observation. Keep CAN disconnected.
2. **If acceptance fails:** obtain the now-visible first panic before changing
   memory modes, pin assignments, backlight polarity or persistence. If UI
   reports ready but the display is dark, check the GPIO4 PWM and panel supply
   with the existing confirmed wiring; saved brightness and LCD SPI init are
   the next discriminators. Requires a log and, if needed, meter/scope evidence.
3. **Longer runtime acceptance:** inspect Diagnostics CAN TX count (expected
   zero), RX availability, saved calibration and settings; assess stack
   high-water marks under navigation/analytics if further stack changes are
   proposed. Own-frame measurements are not a full whole-program stack bound.
4. **Vehicle work only after display acceptance:** continue the existing
   wiring/termination/power review and first-session runbook. This task supplies
   no vehicle validation and authorizes no CANH/CANL connection.

The direct LCD smoke mode is disabled in this build. Its current direct path
does not explicitly restore GPIO4 brightness after early boot-off; enabling it
alone would therefore be a poor discriminator for this problem. Review that
separate path if a panel-only smoke test becomes necessary.

## Provenance and licensing

Evidence comes from local source/ELF/compiler output, the owner-provided backup,
Windows device identity and live UART capture. Backup binaries and private
captures are kept local, outside committed reports. No external code or
dependency update was imported.

ESP-IDF official references: [FreeRTOS task stack units (5.5.5)](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/api-reference/system/freertos_idf.html),
[USB Serial/JTAG console and secondary output (5.5)](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-guides/usb-serial-jtag-console.html).
The installed 5.5.5 `esp_system/Kconfig` independently confirms UART0 defaults
and secondary-console behavior. Documentation is paraphrased; repository
licenses and pinned managed dependencies are unchanged.
