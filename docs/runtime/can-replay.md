# Offline recorded CAN replay

`replay-can` feeds an existing `can_capture.py` schema-1 JSONL capture through
`AcquisitionPlanner → DbcAdapter → SignalStore → SourceArbitrator`, using an
injected monotonic clock. It opens no CAN bus, VCI, transport or ECU session;
there is no fallback wire discovery, no UDS polling and no display/live bridge.
The existing synthetic UDS ReplayTransport remains unchanged.

## Capture and replay

On a prepared Linux capture bench, the existing receive-only recorder is:

```sh
python can_capture.py capture --channel can0 --output capture.jsonl --seconds 60
```

The recorder itself cannot verify hardware listen-only configuration. Metadata
reports this explicitly; choose/verify adapter mode separately. Merely opening
SocketCAN receive sockets does not prove that a controller never ACKs frames.
No arbitrary vehicle IDs or a new physical endpoint are supplied by replay.

Use an already obtained identity snapshot and locally supplied external DBC:

```sh
python -m pip install -r requirements-data.txt
python -m minidash_live replay-can engine.rpm vehicle.speed   --config connection.json --identity-snapshot identity.json   --trace capture.jsonl --output decoded-offline.jsonl --final-age 1
```

`connection.json` supplies the existing `dbc_databases` map. Other connection
fields are ignored: replay does not open an adapter even if J2534/SocketCAN is
configured. Identity snapshot is mandatory, preventing accidental live discovery.
An identity context is not proof that a supplied file came from that car.
External definitions stay candidate unless their existing provenance explicitly
qualifies for that context; replay never adds validation evidence or upgrades it.
Every output record has `offline: true` and `vehicle_validation: false`.
Snapshots can retain the existing definition quality classification, but must
not be mistaken for current vehicle measurements or a validated new capture.
No VIN/identity payload is copied into decoded output.

Input supports only the recorder's Classical CAN schema (not FD, ASC or BLF).
Metadata must state schema 1 and userspace_receive timestamps. Required frame
fields: id, extended/remote/error booleans, length 0..8, hexadecimal data,
non-negative finite monotonic timestamp. Wall time, when supplied, is checked
but never drives TTL. Relative time starts at the first frame; duplicates are
allowed, decreasing monotonic timestamps are rejected. Error/remote flags are
passed to the existing decoder rather than converted into ordinary data frames.

The first output is metadata, then per-frame snapshots and a final summary.
Summary includes complete/incomplete status, frame/decode/error counters,
virtual elapsed time, measured end-to-end processing seconds and throughput.
`--final-age` advances virtual time after the last frame to exercise stale TTL;
no real-time sleep occurs. Processing throughput includes file read, validation,
decoding, snapshot/arbitration and optional JSON output; it is not a hardware
CAN receive capacity or ESP32 FPS measurement.

## Fault handling and limits

- At most 100,000 frames by default; `--max-frames` selects a positive limit.
  Exceeding it fails rather than silently truncating a report.
- Line size bounded to 8,192 characters, streaming input/output, no whole-trace
  in-memory collection. Duplicate JSON keys, non-finite constants, unknown
  records, bad flags/IDs/DLC/hex, wrong end count and data after end are rejected.
- Missing end record means interrupted/incomplete, explicitly marked in summary.
  Malformed final JSON is an error, not silently ignored as an interrupted log.
- Output uses exclusive-create mode. Existing files are never overwritten.
  Failure after opening output appends replay_error, produces no success summary
  and exits nonzero; partial output is retained for investigation.
- No ASC/BLF parser is reinvented. A later optional converter should use the
  already pinned python-can LogReader and retain original channel/timestamp
  provenance. Multi-bus selection is not supplied by this single-channel schema.

## Tests / boundaries

`tests/test_can_replay.py` covers bounded/invalid records, interruptions/order,
clock/TTL, candidate decoding with the pinned MQB subset fixture, no bus/socket/
scheduler/VCI calls, CLI failure reporting and output preservation. Fixture
records are synthetic and do not prove signal visibility on a vehicle.
The optional fixture DBC licensing notice is in tests/fixtures/README.md.
Runtime engine files are reused; no second decoder, SignalStore or arbitrator.
