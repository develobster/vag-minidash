# TFT backlight PWM — 2026-09-30

## Wiring and source of truth

Work continues in the existing `feature/real-car-validation` worktree, not the
older display-prototype checkout. `firmware/sdkconfig.defaults`, the generated
`sdkconfig`, and ST7789/XPT2046 code agree: SPI2 SCLK=12, MOSI=11, MISO=13;
TFT CS=10, DC=8, reset=9; XPT2046 CS=14, IRQ unconnected; TFT SDO unconnected.
CAN remains disabled with no assigned transceiver GPIO. Firmware has no other
wired GPIO adapter. GPIO19/20 are native USB; GPIO0/3/45/46 are straps;
GPIO26–37 are conservatively excluded for module memory; GPIO39–44 for
JTAG/UART. GPIO4 is unused by those functions and the project.

The owner identified board marking `2025-V1.4` and confirmed the exposed header
contact `4` by moving the TFT LED wire there. This is physical owner evidence,
not a claim that an exact manufacturer schematic was found.

**Power off before wiring. TFT LED: 3V3 → GPIO4. Keep TFT VCC on 3V3 and GND
on GND. Never connect LED simultaneously to 3V3 and GPIO.** The direct GPIO
experiment was explicitly requested. Q1, R6=102 (~1 kΩ), R5=393 (~39 kΩ),
and the measured ~3.25 V were reported by the owner; these alone do not prove
LED input current or transistor topology. Stop the test on resets, brownout,
heating, abnormal output or excessive GPIO loading; use an external
MOSFET/transistor driver if this control input proves unsuitable.

## Implementation

`backlight_controller.[ch]` owns LEDC low-speed timer 0/channel 0, 5 kHz,
12 bits, XTAL clock. Percent 0–100 maps to round((percent/100)^2 * 4095),
with a minimum duty of 1 for nonzero percentages. 0 is LOW/off; 100 is
4095/4096 HIGH (~99.976%), avoiding the counter overflow boundary. Active-low
polarity remains configurable. Conflicting/reserved GPIO are rejected at init.

The controller preloads the off GPIO latch before enabling output. `app_main`
initializes it early; panel backends reuse that instance. Backlight stays off
through panel initialization and UI creation until the stored brightness is
applied. No forced 100% boot fallback remains. Firmware cannot control the pin
before application execution; hardware pull-down/topology and a cold-boot
visual check are still required to verify absence of a ROM-stage flash.

Both existing ST7789 and alternate ILI9341 backends delegate brightness/power
to the same controller; panel SPI, orientation and touch configuration stay
unchanged. Sleep extinguishes backlight without overwriting the desired value;
wake restores it. LEDC failure stops PWM at the inactive level and reports it.

Existing Settings → Brightness row is retained; its existing CHANGE/plus/minus
semantics use 10-point steps. No second control is added. The existing Web
configuration brightness field accepts arbitrary 10–100 values. The owner
requested a visible minimum after the first physical test, so Settings wraps
between 10 and 100 instead of reaching 0. Values below 10 are rejected by the
Web validation and bench command. Historical saved values below 10 are raised
to 10 at load without losing other settings. Manual changes
apply on the next UI iteration (~20 ms under normal load), without the prior
one-point/20 ms ramp. Default is 80%; stored values 10–100 are preserved; older values below 10
are raised to 10. Existing NVS configuration storage, CRC/version and 1500 ms debounce
are reused. Old FIXED configurations acquire the new dimming capability and
Manual mode while preserving their saved percent. Touch calibration is retained.

`brightness <10..100>` is a USB bench console command. It posts to the UI queue
and updates the same setting/NVS path; it does not drive LEDC from the console
task. `config-export` reads configuration; `ui-test` runs device self-tests.
Use `brightness 80` to restore a comfortable level and `wake` after sleep.
`backlight-status` reads the actual LEDC duty/frequency. Sample after the next
PWM period: the immediate getter in a write log can still show the preceding
cycle, as specified by the IDF API.

TEMT6000 is ordered and unconnected. No ADC pin or simulated ambient source is
introduced. A future Manual/Auto/Auto + Vehicle policy can supply a resolved
percentage to the existing controller API. Auto requires measured ADC input;
vehicle contribution requires verified illumination data. Current mode is Manual.

## Validation and remaining acceptance

Portable tests cover gamma endpoints/monotonicity, 100/75/50/25/10/0 encoding
round trips, 80% default, rejection of Manual settings below 10%, UI 10/100 wrap and disabled Auto capability. Codec tests
are not hardware NVS reboot evidence. Native CTest passed portable-core, configuration-json and the new
backlight-driver-contract. The latter compiles the actual controller against
driver doubles and checks boot-dark, exact curve outputs, remembered sleep/wake
level, updates while asleep, invalid values and fail-off behavior. The pre-flash connected board answered `ui-test`: 25 passed,
0 failed; this belongs to the previous firmware.

ESP-IDF 5.5.5 is reused from `C:/Espressif`; Python venv and activation profile
are existing installations. Build output uses `firmware/build-backlight` because
the previous build cache references a moved checkout. USB re-enumerated as COM5,
VID:PID 303A:1001, serial 14:C1:9F:50:93:64. App-only flash, physical PWM acceptance and NVS reboot persistence passed;
results and their limits are recorded below.

Bench sequence: verify image and touch, set 100% first and obtain physical
confirmation of stable light/no heat; then check 75/50/25/10 and sleep (0)/wake, then
restore 80%; operate the existing Settings control, wait >1.5 seconds for save,
reboot and compare restored setting/duty. Check sleep/wake and cold USB boot.
App-only flash at 0x10000 preserves the existing partition table and NVS; verify
that table matches `firmware/partitions.csv` before writing. Do not erase NVS.

## References

- [ESP-IDF 5.5.5 LEDC](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/api-reference/peripherals/ledc.html)
- [ESP32-S3 WROOM module datasheet](https://documentation.espressif.com/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf)
- [Existing display prototype](display-prototype.md)

## Recorded bench results

- Existing 3 MiB factory app, partition table, NVS and PHY region backed up from
  0x8000 for 0x308000 bytes before writing. SHA-256:
  `33ec864871b659104749254bde9b5e5583563b4975cb2dc105fadeb790c6af89`.
  Backup stays outside Git in the chat task scratch directory,
  `work/minidash-backlight-evidence/pre-backlight-flash.bin`.
- Physical partition table parsed from that backup exactly matches the project:
  NVS 0x9000/0x6000, PHY 0xf000/0x1000, factory 0x10000/0x300000.
- Final build after the requested minimum change passed. App size 1728528 bytes;
  app image SHA-256 `dd32b6c55cf3a7b802c495f8ecdb67965c7620b3b5b350aebb0788250807c5e4`.
- App-only COM5 flash passed; esptool verified the written hash. Bootloader,
  partition table and NVS were not written. Device boot identifies ESP-IDF 5.5.5,
  ESP32-S3 revision 0.2, 16 MB flash and 8 MB octal PSRAM; CAN remains disabled.
- Boot logs confirm GPIO4 LEDC init off, ST7789 320x240 rotation 2, LVGL UI ready
  and XPT2046 calibrated edge controls active. Owner confirmed physical backlight
  works after rewiring and first flash at 100%.
- Actual `backlight-status` hardware readback matches every requested point:

  | UI % | LEDC duty / 4095 |
  | --- | --- |
  | 100 | 4095 |
  | 75 | 2303 |
  | 50 | 1024 |
  | 25 | 256 |
  | 10 | 41 |
  | sleep/off | 0 |
  | wake | 41 (restored 10%) |

- Device self-tests: 25 PASS / 0 FAIL after final flash. No brownout, panic or
  unexpected reboot was observed during the measured sweep. Software register
  evidence does not measure GPIO load current, PWM waveform or component heat.
- Native CTest: portable-core, backlight-driver-contract and configuration-json
  all PASS (3/3). Stub tests cover the actual controller; no hardware claim is
  inferred from those tests.
- Owner confirmed: 10% is readable and existing Settings → Brightness changes
  physical backlight immediately. Touch event logs (`source=0`, Settings screen 6)
  show 10→20→30% with LEDC duty 41→164→369. No second UI control was introduced.
- Reboot persistence PASS: owner subsequently selected 70%; configuration exports
  before/after software reboot match for all common/platform fields. Boot restored
  70%, actual hardware readback 2007/4095 at 5000 Hz; calibrated touch and LVGL
  restored. Device self-test remained 25 PASS / 0 FAIL. The 70% setting is retained.
- Owner also power-cycled USB and confirmed 70% persisted with no bright startup
  flash. This is physical visual sign-off; no oscilloscope waveform, GPIO-current
  or temperature measurement is claimed.

## Changed modules and next acceptance

Driver/config: `backlight_controller.[ch]`, both LCD backends, firmware main and
component CMake/Kconfig/defaults. UI/persistence: `display_app.[ch]`, `ui_core`,
`ui_model`, configuration conversion/validation and NVS migration. Tests:
`ui_core_selftest`, `ui_platform_selftest`, core CMake, controller contract harness
and deterministic IDF stubs. Current hardware/UI/status/development docs link
this report. Existing Web label already specifies the final 10–100% range.

P0 acceptance completed: actual touch brightness, readable 10% minimum,
software-reboot retention and owner-confirmed cold USB boot. P1: measure control input current and
cold-boot behavior; use an external driver if load/reset/heat evidence requires
it. P2: wire TEMT6000 to an independently verified ADC1 pin, measure/calibrate
light response, then implement Auto policy. Auto + Vehicle follows only after
trusted vehicle illumination input is captured. No vehicle actions or guessed
CAN mappings belong to this backlight task.

## Delivery and recovery

Changes are scoped to the existing `feature/real-car-validation` branch. The older
`feature/esp32-display-prototype` checkout and its unrelated dirty files were not
modified. Detailed logs and the pre-flash backup remain in the chat task scratch
evidence directory. App image currently on the board corresponds to the recorded
DD32B6... SHA-256 above; subsequent documentation commit does not rebuild that
verified binary. Rebuilding from the committed source may change app metadata.

Recovery from wiring problems: power off, remove GPIO4 from TFT LED completely,
then restore LED to 3V3; do not bridge the two. Set `CONFIG_MINIDASH_LCD_PIN_BL=-1`
for an always-on build, preserving panel/touch pins. For firmware rollback, extract
bytes 0x8000..0x308000 of the saved regional backup (the original factory app),
then app-only flash them to 0x10000. Do not restore/erase NVS unless separately
needed and authorized; the task's current settings/calibration are verified.
