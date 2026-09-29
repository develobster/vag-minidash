# Python runtime to embedded display bridge

## Boundary and transport

`minidash_live watch` can optionally send its arbitrated canonical signal
snapshot to the ESP32-S3 over USB Serial/JTAG. The bridge forwards only signals
requested from the host runtime. It does not decode CAN/UDS, choose an ECU,
derive signals or add another source-arbitration path. The firmware accepts only
the canonical signal names already supported by its display API.

Install the optional serial dependency:

```powershell
python -m pip install -r requirements-display.txt
```

Example, using the serial port assigned to the running ESP32 application:

```powershell
python -m minidash_live watch engine.rpm vehicle.speed engine.coolant_temperature `
  --config .\tiguan-read-only.json --display-port COM5
```

Use the existing read-only runtime configuration and identity snapshot process.
The bridge opens no vehicle transport itself. A serial port can have only one
owner at a time, so close a serial monitor before starting `watch`.

## Sample protocol

Each sample is one bounded ASCII line:

```text
MDS1|engine.rpm|VALID|1500|2450
MDS1|vehicle.speed|STALE|1000|-
```

Fields are protocol version, canonical name, state, TTL in milliseconds and
value. Non-`VALID` frames must use `-` for the value. TTL is limited to 1–60,000
ms. The firmware rejects unknown names, unknown states, non-finite numbers,
malformed or oversized lines, invalid TTLs and values attached to non-valid
states. It reports rejected frames on the console. Serial samples are always
marked as non-demo; synthetic or candidate data must not be presented as live.

The host bridge forwards a value only when source arbitration selects a row
whose quality is exactly `valid`. Ambiguous decisions become `ERROR`; stale,
unsupported and unavailable states remain explicit and never carry a number.
Candidate and raw-only values are not displayed as valid values. The firmware's
TTL handling marks a value stale if host updates stop.

## Responsiveness and limits

Serial writes run on a dedicated worker with a one-snapshot queue. When writes
fall behind, older pending snapshots are replaced by the newest one. A serial
write error is reported once and does not stop the runtime acquisition loop.
The default display interval is 100 ms and can be changed with
`--display-interval`.

Only signals listed in `firmware/main/ui_core.c` can be sent. Unsupported watch
signals are rejected before the serial port is opened. No live vehicle-to-screen
session has been performed yet; CAN remains disabled in the firmware. Validate
the bridge with unavailable states or a configured replay transport before
using a live vehicle source.
