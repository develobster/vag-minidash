# Car Validation Mode — Tiguan II 2017 / CZPA / DQ500 / 4Motion

Baseline: `integration/standalone`. This is first-session preparation, not a claim
that the new image or CAN wiring has passed physical vehicle acceptance.

## Safety and implementation

`CONFIG_MINIDASH_CAR_VALIDATION_MODE` is build-time; host/device configuration
cannot enable ECU writes. Two explicit presets use the reviewed installation
GPIO15 TX / GPIO5 RX and retain GPIO4 backlight. Verify actual wiring before use.

| Preset | CAN behavior | Diagnostic policy |
|---|---|---|
| `sdkconfig.car-validation-listen` | Hardware listen-only throughout; no ACK/TX/flow control | Raw RX only; no unverified manufacturer decode |
| `sdkconfig.car-validation` | Initial 1.5 s hardware listen-only, then standard OBD reads | At most 4 application requests/s, single outstanding transaction, bounded retries/timeouts/QUIET |

Active collection fixes demand to RPM, speed, coolant and ECU supply voltage.
Supported-PID discovery and Mode09 VIN are retained. The existing stationary
Mode03 read remains gated by fresh speed=0 and explicit UI request; it is optional,
not part of the initial sequence. Required ISO-TP flow control may add TX frames
and has its existing authenticated address/service/length gate. No coding,
adaptation, DTC clear, actuator test, diagnostic session change, TesterPresent,
SecurityAccess, routine, arbitrary raw command or write operation is added.
The default non-validation scheduler retains the 100 ms floor and 12-source
600 ms speed / 195-requests synthetic regression contract.

The compiled manufacturer profile remains NULL. Knowledge/confidence, an ODIS
label, nominal VIN/model or a successful synthetic trace never activates a binding.
Future manufacturer reads require the existing compiled VIN/address/license/
definition/observed exact ECU identity gates. Validation mode does not bypass them.

## Trace and export

The existing temporary device AP exposes **GET `/api/validation`** and a Download
evidence snapshot link in the local editor. One locked snapshot contains:

- Firmware version, 64-bit boot uptime, monotonic frame/request/response ms, VIN
  (private), chosen standard response address and supported PID bitmaps.
- 96 raw CAN frames with sequence, direction, ID, DLC, extended/remote flags.
- 16 complete/partial diagnostic exchanges (up to 128 payload bytes), service,
  PID/DID, request/response addresses, response time, raw payload, decoded canonical
  value/source/validity, NRC, timeout/malformed/TX-failed/bus-fault outcomes.
- All current canonical/derived signal values, units, ages, validity and source,
  plus RX/TX/drop/error/bus-off/overwrite counters. Unavailable/stale values are null.

Frame timestamps are firmware processing timestamps, not hardware wire timestamps.
Response time is the application's request-to-complete/error elapsed time, including
queue/scheduling effects; do not interpret it as isolated ECU execution time.

Acquisition uses fixed storage and no new sample-path allocation. JSON/export
allocation occurs outside the CAN mutex after copying a bounded snapshot. The AP
is opt-in and expires after 600 s; no phone/PC is required for device operation.

**This is bounded evidence, not guaranteed continuous CAN recording.** Raw frames
may overwrite in milliseconds on a busy bus. Diagnostic exchanges are separate,
so raw traffic cannot evict them. A nonzero cumulative overwrite counter can be
normal even when consecutive exports overlap; sequence gaps show actual missing
records. Save gaps/errors and do not claim lossless capture. Persistent device
logging is not implemented; for complete passive wire evidence use an independently
validated receive-only recorder in a separate session. Do not run two active
diagnostic clients together.

An optional development PC can connect to the device AP and collect GET snapshots:

```sh
python tools/capture_car_validation.py --output PRIVATE_NEW_SESSION.jsonl --duration 60 --interval 0.5
```

The collector has no CAN/VCI or write path, preserves raw snapshots, adds capture
UTC/boot epochs and reports missing frame/exchange sequences. Existing files are
never overwritten. HTTP failure leaves the partial file; preserve it and restart
with a new file. Boot/restart and 32-bit sequence wrap are handled. Session files
contain private VIN/traffic: hash and store outside tracked source directories.
The snapshot schema is distinct from legacy `can_capture` replay input; do not
feed it to a different reader without a reviewed explicit conversion.

## READY FOR CAR — conditional on protected wiring/bench checks

Stationary display/touch/NVS and RX/errors can be checked in passive validation.
Then standard supported-PID/selected-ECU discovery, RPM/speed/coolant/voltage,
unavailable states, loss/QUIET/reconnect behavior, and raw/decoded/latency evidence
can be checked in active validation. Existing standard standalone mode also has
12 optional OBD sources and derived analytics; their presence and accuracy must
be measured on this ECU before interpreting results. Validation's four-source
subset intentionally leaves other metrics unavailable. Do not test performance
runs or change device settings while driving.

## BLOCKED BY VEHICLE EVIDENCE

| Binding / route | Evidence still required |
|---|---|
| Standard 11-bit/500-kbit OBD path on this assembled device | Actual connector/tap, protected electrical integration, responder/capabilities, raw lengths, timing and independent reference comparison |
| CZPA native manufacturer profile / F405 | Existing host/ODIS evidence is Engine 7E0/7E8 and F405 A−40 for one exact identity; repeat native transport and fingerprint/layout evidence before compiling any profile |
| DQ500 ATF / selected gear | Existing 2104 / 3816 measuring-value observations do not confirm raw DID layout, endpoint, signedness/scaling or applicability; native bindings disabled |
| Haldex temperatures/pump/status | Existing 2BF1 / 028D / 2BE4 observations need raw route, payload, scaling, units, identity and validity evidence; disabled |
| MQB passive RPM/speed/wheels/temperatures/steering/brake | Pinned openDBC candidates (e.g. 0A8/0FD/0B2/640/086/106) are source definitions, not evidence of visibility/scaling/quality at this Tiguan OBD tap; disabled |
| Retard, misfires, requested/actual VAG boost and drivetrain analytics | Exact ECU endpoints, identifiers, raw samples, offsets/endian/sign/scaling/units, resets/invalid sentinels and reference evidence; unavailable |

See the existing [identity/source register and analytics protocol](engine-analytics-car-evidence-2026-10-04.md)
and [MQB evidence manifest](../runtime/tiguan-mqb-validation-manifest.json).
No identifiers or scaling were guessed or enabled during consolidation.

## NEXT CAR SESSION

1. **On the bench:** save factory/NVS/config/calibration backups. Check protected
   power, ground, transceiver voltage and actual OBD pin orientation; no 12 V to
   MCU and no added vehicle-bus terminator. Confirm TX15/RX5 and GPIO4 separation.
   Keep the physical power switch. Verify partitions before flashing; do not erase
   NVS or change partitions. Record exact commit, preset, binary/config SHA-256.
2. From `firmware/`, build the passive preset in a fresh build/config in the IDF environment:
   `idf.py -B build-car-listen -D SDKCONFIG=sdkconfig.car-listen.generated -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.car-validation-listen" build`.
   Flash only on the bench using the established board/port/backup procedure.
   Confirm generated CAN=y, validation=y, standard OBD=n, host injection=n;
   check screen/touch/save/reboot before connection.
3. **Connect:** vehicle stationary, P and parking brake, other active diagnostic
   clients disconnected. Connect with device power off, then switch on. Check
   SERVICE STATUS RX and errors; **TX must remain zero**. Save passive snapshots,
   wiring notes, power readings and local timestamps. Absent RX alone is inconclusive.
   Stop and disconnect for bus-off, instability or changes in vehicle behavior.
4. After passive/wiring acceptance, disconnect the car and return to the bench.
   Build/flash `sdkconfig.car-validation` in another fresh build/config. Reconnect
   stationary and observe LISTEN → discovery/capabilities/VIN → LIVE, or bounded
   QUIET/error. Record chosen response address and supported bitmaps; do not force
   a missing response or enable manufacturer candidates.
5. Save 30 s ignition-on/engine-off and, if the environment permits normal engine
   operation with ventilation, 60 s stable idle. Compare speed=0 and RPM with the
   cluster; compare coolant and module voltage with a known reference in a
   **separate** read-only diagnostic session. Module voltage is not terminal
   voltage. Note each value/unit/time/difference and per-source sample interval.
   Save repeated JSON/JSONL snapshots, exact raw responses, NRC/timeouts/drops/gaps
   and reference files; unavailable values must not become a reassuring zero.
6. Switch off/disconnect after the test. QUIET does not cut power; do not leave the
   prototype powered overnight. Hash private originals and record ON/QUIET/OFF
   current, boot→LIVE, reconnect/NVS results and any failure. Review offline before
   scheduling a manufacturer evidence session or compiling any new binding.

No vehicle request, flashing or hardware connection was performed by repository
acceptance. Permanent automotive power/sleep, CAN physical acceptance, real heap/
task watermarks and complete passive recording remain separate physical gates.
