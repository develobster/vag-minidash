# ESP32-S3 hardware bring-up

The firmware under `firmware/` is the existing ESP-IDF project. This bring-up targets the
tested ESP32-S3 board configuration: 16 MB external quad flash, 8 MB octal PSRAM and USB Serial/JTAG.
It checks chip information, physical flash size, PSRAM read/write, internal heap read/write,
the GPIO driver without driving pins, Wi-Fi passive scanning without credentials, BLE
controller initialization, Task Watchdog feeding, serial-console input/output, and a
controlled software reboot. It does not initialize CAN or communicate with a vehicle.

The PSRAM check allocates 6 MiB and verifies two full-memory data patterns. Reported usable
PSRAM is also recorded because the heap has runtime overhead and is smaller than the chip's
nominal capacity. Wi-Fi uses a passive scan, so it listens for access-point beacons and does
not need or save network credentials; its storage backend is RAM and Wi-Fi NVS persistence is
disabled in the firmware defaults. The watchdog check feeds the task watchdog repeatedly;
it does not intentionally trigger a watchdog reset. GPIO driver readiness is checked without
configuring or driving a pin because the final board pinout is unknown.

## Prepare ESP-IDF in Windows PowerShell

This project is built with ESP-IDF **v5.5.5**, installed using Espressif Installation Manager
(EIM). Open the ESP-IDF v5.5.5 PowerShell shortcut so `idf.py` and `IDF_PATH` are available.
The project does not use Arduino or PlatformIO.

```powershell
Set-Location C:\Users\user\vag-minidash\firmware
idf.py set-target esp32s3
idf.py build
```

`sdkconfig.defaults` selects the target, 16 MB flash, octal PSRAM, BLE controller-only mode and
USB Serial/JTAG console. The first build may ask to confirm defaults if the selected ESP-IDF
release has additional configuration options.

## Flash and monitor COM5

Windows showed the board as COM4 while its application is running and COM5 in ROM download
mode after holding BOOT and pressing RESET. COM assignments can vary, so check Device Manager
and use the currently listed port. For a manual download-mode flash, use the ROM download port;
after reset, use the application port for monitoring and automated tests. Close any other
serial monitor on the selected port.

Before flashing, inspect the generated images and write map:

```powershell
Get-Item .\build\bootloader\bootloader.bin, .\build\partition_table\partition-table.bin, .\build\vag_minidash.bin |
  Select-Object Name, Length
Get-Content .\build\flash_args
```

For the currently verified build, `flash_args` lists the bootloader at `0x0000`, partition
table at `0x8000`, and factory application at `0x10000`. The build evidence file
`artifacts/hardware/esp32s3-bringup-build-*.json` records exact image lengths and SHA-256 hashes.
The normal `idf.py flash` command erases and rewrites only the sectors occupied by those three
images (aligned to 4 KiB flash sectors); it does not erase the full 16 MB device or the NVS
partition. The files replace the existing bootloader, partition table and factory application.
Never run `erase-flash` for this test. Keep the factory dump available and verify its SHA-256
before proceeding. Review the build evidence and confirm before starting the destructive write.

When ready to write the board:

```powershell
idf.py -p COM5 flash
idf.py -p COM4 monitor
```

The monitor should show `MINIDASH_BOOT` and `MINIDASH_READY`. The monitor can remain open for
manual serial interaction, but close it before running the automated capture script below.
Exit IDF Monitor with `Ctrl+]`.

## Automated test and evidence capture

With the bring-up firmware already running and its application-mode COM port free (COM4 on the
tested setup), run this from a second PowerShell window (the ESP-IDF environment is not required
for this script):

```powershell
Set-Location C:\Users\user\vag-minidash
.\tools\Run-Esp32Bringup.ps1 -Port COM4
```

The script requests the current boot status, sends `run`, collects all `MINIDASH_TEST` records, asks the firmware to reboot,
and checks that the next boot count increments with reset reason `SW`. It writes a uniquely
named serial log and JSON PASS/FAIL summary in `artifacts/hardware/`, preserving existing
files. A missing serial line, failed hardware check, missing test record or unexpected reboot
causes a non-zero exit code. To collect a manual transcript instead, use `idf.py -p COM5
monitor` and send `run` then `reboot` in the monitor.

The factory backup is user data. Do not replace or delete
`artifacts/hardware/esp32s3-factory-16mb.bin`; its recorded SHA-256 is
`CD0F30AC9491DA499EAF2167F07B5ACCF23CF6779C506110431AA766F346273E`.

References: [ESP-IDF ESP32-S3 getting started](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/index.html),
[USB Serial/JTAG console](https://docs.espressif.com/projects/esp-idf/en/v5.3/esp32s3/api-guides/usb-serial-jtag-console.html),
[Wi-Fi scan API](https://docs.espressif.com/projects/esp-idf/en/v5.3/esp32s3/api-reference/network/esp_wifi.html),
[heap capabilities API](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/mem_alloc.html).
