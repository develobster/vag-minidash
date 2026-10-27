# Embedded modernization — 27 October 2026

Base: `integration/standalone`, `f73a81a14815e586d8e42fed1675616163795bef`.
Continue the existing baseline; historical branches and `main` are untouched.
Terminal network access is unavailable, so the source snapshot was retrieved
through GitHub. All original blobs and the complete tree were hash-verified.

## Plan by module and delivered changes

| Step | Files | Implementation |
|---|---|---|
| Isolate RX | `vehicle_app.c`, `can_pipeline.[ch]` | Core 1/priority 18 receives into a fixed 512-entry SPSC ring. Processing/analytics run on core 0/priority 5. RX has no decoding, NVS, UI calls or application mutexes. The hardware driver queue retains 128 entries. |
| Transfer UI snapshots | `can_pipeline.[ch]`, `display_app.[ch]` | A C11 atomic ownership exchange transfers complete canonical stores through three slots. Writer and reader never access the same slot concurrently. Sample timestamps and TTL survive publication. |
| Recover CAN | `vehicle_app.c`, `can_pipeline.[ch]` | Bus-off backoff is 1/2/4/8/16/30 seconds. Invoke hardware recovery, wait for STOPPED, then reinstall Listen-Only. TX requires one clean second, ten frames, recent activity and no new bus errors. Manual scan cannot bypass recovery. |
| Share decoders | `data/can/platforms.json`, `scripts/generate_can_signals.py`, generated C/Python files, `acquisition.c`, `minidash_live/passive.py` | One catalogue generates C declarations/unpacking and Python tables. Existing MQB definitions are imported by reference. Provisioned passive and UDS scalar decoding use the common generated primitive. |
| Prioritize passive traffic | `acquisition.[ch]`, `vehicle_app.c` | Fresh verified passive values suppress matching OBD/UDS polling. Application gap is at least 100 ms, raised to 200 ms under queue pressure/AP activity; validation retains 250 ms. External tester requests suspend polling for two seconds. |
| Bound transport | `diagnostic_transport.[ch]`, `acquisition.[ch]` | Validate FC status, BlockSize and STmin encodings. Separate RX observation time from processing deadlines. Preserve N_Cr, bounded NRC 0x78 and NRC 0x31 handling. |
| Reduce redraw | `ui_screens.c`, `display_app.c` | Reuse existing LVGL port, DMA and two 16-line partial buffers. UI/LVGL use core 0. Invalidate old/new needle bounds; cache tick artwork and chart updates. Existing renderer/backend interfaces remain the abstraction boundary. |
| Gate RF | `ui_web.[ch]`, `sdkconfig.service` | Manual AP requires fresh zero speed or service boot. Positive speed stops RF; stale parked evidence revokes permission. Stop RF before waiting for HTTP cleanup. No automatic AP restart. |
| Deep sleep | `power_manager.[ch]`, `display_app.[ch]`, `main.c`, `Kconfig.projbuild` | Optional silence policy, off by default. Finish NVS commits, prevent further saves, hold LVGL ownership, blank backlight, send SLPIN, stop RX/controller, hold TX recessive and enter sleep. RTC EXT0 and fallback timer wake are supported. |
| Validate | `core/test_can_pipeline.c`, transport/acquisition tests, `tests/test_can_codegen.py`, `.github/workflows/validation.yml` | Concurrency, parity, protocol boundaries, static analysis, sanitizers and ESP-IDF build matrix. CI never flashes a device. |

`md_ui_model_lock` does not exist in this baseline. The actual UI path used a
critical section and 92 separate publications. The acquisition mutex remains
for bounded status/configuration access, but physical RX no longer depends on
it. Publishing a mailbox under this mutex only copies a fixed store and swaps
an atomic index; it never enters LVGL. Driver install/uninstall is owned by RX
and acknowledged after its current receive returns, preventing stale handles.
`CONFIG_TWAI_ISR_IN_IRAM=y` keeps the ISR available during flash cache stalls.
The ESP-IDF PSRAM atomic workaround is preserved: its compiler-wide
`ATOMIC_INT_LOCK_FREE` value is conservative, while the SDK dispatches naturally
aligned internal-RAM 32-bit operations to hardware S32C1I stubs. Ring and mailbox
metadata are explicitly placed in internal DRAM. The target assertion checks
hardware CAS support instead of rejecting the required PSRAM workaround.

## Observable limits and compatibility

The validation API adds `rx_driver_dropped`, `rx_ring_dropped`,
`rx_epoch_dropped`, `rx_ring_high_water`. `rx_dropped` sums loss counters.
Overflow drops the newest frame without blocking RX. Frames queued across a
controller mode change carry an old epoch and are discarded explicitly.

This is not a demonstrated zero-loss guarantee. Flash stalls, finite queues,
mode-switch blind intervals and electrical faults can still lose frames. The
128-entry driver queue can fill during a flash/cache stall even with an IRAM
ISR. Verify loss counters and queue occupancy against a physical CAN generator.
The bounded evidence trace overwriting history is distinct from receive loss.

Nine external candidate layouts are generated: MQB RPM/speed/coolant/oil/pedal,
and PQ35/PQ46-family RPM/speed/coolant/pedal. `md_generated_can_decode` is an
offline candidate decoder. `md_generated_can_bindings` stages four MQB or three
PQ-family fields with existing raw canonical IDs for an identity-gated
`md_vehicle_profile_t`; it neither enables them nor establishes evidence.
The production adapter still has `.profile=NULL`.

Live provisioning needs the existing VIN/ECU/addressing/definition gates plus
vehicle evidence for sender variants, validity bits, counters/checksums and
availability behind the OBD gateway. PQ35/PQ46 exact applicability is unverified.
Pedal has C/Python decoding but no embedded canonical UI ID yet: adding it needs
an append-only ID/schema design preserving existing saved widgets and derived
IDs. Firmware configuration schema remains 4; old settings are preserved.

`core.py` is the legacy simulator/recorder, not a CAN decoder. It is preserved.
The live host passive adapter uses the generated primitive through 32 bits;
its existing 64-bit byte-field compatibility path remains separate because
the embedded scalar/profile format supports at most 32 bits.

The transport receives bounded Classical CAN ISO-TP responses; application
requests fit single frames. There is no segmented request sender. Existing FC
uses BS=0 and STmin=10 ms, within the 128-byte receive bound. Parsed inbound FC
does not resume a nonexistent TX session. N_Cr remains 250 ms; initial response
deadline 500 ms; UDS NRC 0x78 is bounded by three pending responses and a total
ten-second transaction deadline. Confirmed SF/FC transmission retains its 20 ms
N_As budget. Fast CFs are not rejected solely for violating requested STmin;
that requires hardware arrival timestamps and separate vehicle acceptance.
This is not full ISO 15765-2 conformance certification.

## Build and emulator validation

Use Python 3.11+ and ESP-IDF **5.5.5** (the existing read-only HAL observer pins
this exact SDK). Commands below do not perform vehicle diagnostic operations.

```sh
python -m pip install -r requirements-transport.txt -r requirements-data.txt
python scripts/generate_can_signals.py --check
python -m unittest discover -s tests -v
cmake -S core -B .local/core -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build .local/core
ctest --test-dir .local/core --output-on-failure
node --test tests/frontend/model.test.js tests/frontend/units.test.js
python -m minidash_live.configurator
```

Open http://127.0.0.1:8765 and verify simulation, screen editing and stale states.
This remains a host simulation: it does not apply settings to ESP32. Inspect a
generated golden vector without transport:

```sh
python -c 'from minidash_live.generated_can_signals import decode_frame; print(decode_frame("MQB",168,bytes.fromhex("0000000000000032")))'
```

Expected: candidate RPM=3200. To include JSON/editor native tests, pass
`-DMD_JSON_SOURCE_DIR=/path/to/cJSON` containing `cJSON.c`/`cJSON.h`. CI checks
out cJSON v1.7.19 into ignored `.local/cjson` and runs clang-tidy on the actual
compilation database, native ASan/UBSan and three target variants: standalone
ST7789, Listen-Only ST7789, and sleep-enabled ILI9341. Target binaries are not
uploaded as public artifacts.

Inside an ESP-IDF 5.5.5 environment:

```sh
cd firmware
idf.py -B build-modern -D SDKCONFIG=sdkconfig.modern.generated -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.standalone' build
# Append ;sdkconfig.service for an explicit stationary service/USB bench image.
# Use sdkconfig.listen-only instead of sdkconfig.standalone for passive-only RX.
idf.py -B build-modern -D SDKCONFIG=sdkconfig.modern.generated -p COM3 flash monitor
```

Replace COM3 with the actual port (this was the previously used Windows port).
Keep the factory backup. Normal flash does not erase NVS; avoid `erase-flash`.
No device has been flashed as part of this task.

## Sleep and service configuration

Sleep remains off until explicitly provisioned. Create an ignored overlay
`sdkconfig.power.generated`, append it to SDKCONFIG_DEFAULTS and use a fresh
build directory/configuration:

```ini
CONFIG_MINIDASH_DEEP_SLEEP=y
CONFIG_MINIDASH_CAN_SLEEP_SECONDS=30
CONFIG_MINIDASH_WAKE_GPIO=-1
CONFIG_MINIDASH_WAKE_SECONDS=300
```

Start with timer wake. A timer boot with no traffic probes for three seconds
before sleeping again; ordinary boots/active sessions use the configured
silence interval. Only an available controller outside recovery may initiate
sleep. NVS failure, UI timeout, resumed CAN or an active wake level abort entry.
Canceled entry wakes the panel and restores Listen-Only reception. Firmware
logging is a bounded RAM trace, not a file logger; no filesystem log writer is
invented. Existing synchronous incident/NVS writes finish before shutdown.

After electrical validation, GPIO5/active-low can be the current CAN RX wake
input. EXT0 is level-sensitive. Alternatively use a conditioned Kl.15 input
on an unused RTC GPIO. Never connect 12 V to an ESP32 GPIO. Pin conflicts and
RX polarity are checked; external bias/level conditioning must be verified.
Deep sleep does not power off the LM2596, transceiver, LEDs or an always-powered
backlight circuit. Measure complete OBD supply current and wake latency.

The production image rejects AP start when speed is unknown. When ECU discovery
does not work, use the explicit `sdkconfig.service` image for stationary
connection-doctor access. Service boot permits manual AP before a speed
observation, but an observed positive speed always stops it. Stale parked data
does not restore service permission. The AP still expires after ten minutes.

## Verification and next acceptance

Local environment: macOS arm64, Apple Clang, LLVM clang-tidy 21.1.8,
Python 3.11.14. System Python 3.9 cannot import existing modern annotations.
CMake/ESP-IDF and hardware are unavailable locally. Native sources were compiled
directly from the CMake source list with C11/Wall/Wextra/Werror; test drivers
used their normal CMake warning policy.

Local checks:

- Nine native executables passed: transport, concurrent pipeline, acquisition,
  analytics, analytics pipeline, instruments, portable core, backlight and TX.
- Ring and mailbox tests: 100,000 FIFO transfers and 100,000 complete snapshot
  publications; wrap, overflow, deadlines and exponential backoff.
- C/Python parity: 13,000 deterministic random vectors plus independent RPM
  golden vectors, malformed layouts, candidate and profile identity gates.
- Selected clang-tidy checks for acquisition/transport/pipeline/generated C.
  The analyzer found an existing unchecked UDS signed shift; the generated
  checked decoder now replaces that code.
- Host JavaScript model/unit contracts.
- Local ASan/UBSan runs passed for transport, concurrent pipeline and acquisition.

The final local Python run exercised 237 tests: 209 passed, eight localhost HTTP
tests were blocked by the host sandbox, and twenty optional/platform tests were
skipped. CI run [40506696521](https://github.com/develobster/vag-minidash/actions/runs/40506696521)
resolved those environment gaps: the host job passed 11 native CTest groups,
237 Python tests (236 passed, one platform skip), 10 JavaScript contracts,
clang-tidy, and nine ASan/UBSan groups. The first target matrix correctly rejected
the overly strict unconditional lock-free assertion; the internal-DRAM/S32C1I
fix above is included for target revalidation. Record final target results
separately from bench/vehicle evidence.

Next steps, with acceptance criteria:

1. Build all target variants. Stress an isolated CAN generator at low/high
   loads, including short-frame bursts, while rendering, saving settings and
   serving AP. Compare sequence counts and loss counters; record ring occupancy,
   CPU idle, processing latency and task stack high-water marks.
2. Inject bus-off on that bench. Observe backoff, hardware recovery, clean
   Listen-Only validation and no premature TX. Verify a second tester suspends
   application requests. Multiframe external tester detection remains limited.
3. Check SLPIN/backlight, pending NVS saves, sleep cancellation/failure paths and
   complete supply current. Measure RX/timer/conditioned-ignition wake latency.
   Add transceiver standby only after wiring its control input.
4. Visually verify needle bounds across themes, rotations, changed scale and
   unavailable values. Measure frame rate, temperature and current with/without
   RF. No measured thermal improvement is claimed from synthetic tests.
5. Capture Powertrain CAN and independent ODIS/VCDS references on the exact car.
   Verify validity bits, sender/ECU identity, route and checksum before enabling
   passive profiles; then prove passive-first suppression and stale fallback.

## Primary sources and licenses

- [ESP-IDF legacy TWAI](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/api-reference/peripherals/twai.html): driver states, ISR affinity, queues/recovery. Retain the existing v2 API rather than mixing node API behavior.
- [ESP-IDF atomic shim](https://github.com/espressif/esp-idf/blob/v5.5.5/components/newlib/priv_include/esp_stdatomic.h): internal-RAM hardware stubs and PSRAM fallback; do not disable the PSRAM workaround to change a compiler macro.
- [ESP32-S3 sleep modes](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32s3/api-reference/system/sleep_modes.html): RTC wake, timer and pad restoration.
- [Espressif LVGL port](https://github.com/espressif/esp-bsp/tree/master/components/esp_lvgl_port): affinity and partial DMA. Existing lock pins port 2.9.0/LVGL 9.5.0.
- commaai/opendbc, MIT, pinned revision `c4465696dc2ef0d1340ae188810ac982586999a4`: [MQB DBC](https://github.com/commaai/opendbc/blob/c4465696dc2ef0d1340ae188810ac982586999a4/opendbc/dbc/vw_mqb.dbc), [PQ DBC](https://github.com/commaai/opendbc/blob/c4465696dc2ef0d1340ae188810ac982586999a4/opendbc/dbc/vw_pq.dbc). PQ candidate fields are Motordrehzahl, BR1_Rad_kmh, MO2_Kuehlm_T and MO3_Pedalwert. No complete DBC is vendored; existing MQB provenance and third-party attribution are retained.
