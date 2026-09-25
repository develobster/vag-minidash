# Tiguan MQB read-only validation workflow

Target: Tiguan II, 2017, CZPA, DQ500, Gen5 Haldex. The pinned MQB DBC and
community definitions are candidates until compared with this vehicle. The
machine-readable cases are in
[`tiguan-mqb-validation-manifest.json`](tiguan-mqb-validation-manifest.json).

## Prepare once

Use the existing read-only runtime config and identity snapshot procedure in
[`tiguan-first-live.md`](tiguan-first-live.md). Keep the actual physical ECU
addresses only if their evidence is already documented for this car. Do not
copy Octavia/MQB-sniffer `0x714/0x77E` addresses into the Tiguan config.

Install the optional DBC reader in the project virtual environment and place
the externally obtained `vw_mqb.dbc` at the path referenced by the runtime
config. Use the pinned revision shown in the manifest; the DBC is not vendored.
The provider already names this database `opendbc/vw_mqb`.

```powershell
python -m pip install -r requirements-data.txt
python -m minidash_live discover --config .\tiguan-read-only.json `
  --output .\artifacts\tiguan-identity.json --debug
python -m minidash_live signals --config .\tiguan-read-only.json `
  --identity-snapshot .\artifacts\tiguan-identity.json `
  --all-implementations | Tee-Object .\artifacts\tiguan-signals.json
```

Review the identity and catalog before watch. The Skoda candidate DIDs must not
appear for this identity. A CAN DBC definition can appear, but its catalog
confidence stays `candidate` and its expected period stays unknown.

## Passive-first capture

Set `passive_can` to the actual receive-capable interface and add this temporary
setting to the read-only config:

```json
"passive_can_grace": 60
```

The configured 30-second watch therefore listens for CAN throughout the case
and will not start a fallback UDS value poll during that capture. Identity
discovery has already completed, and `--identity-snapshot` prevents it from
running again. Use a fresh output path for every run because MiniDash refuses
to overwrite evidence.

```powershell
python -m minidash_live watch --config .\tiguan-read-only.json `
  --identity-snapshot .\artifacts\tiguan-identity.json `
  --duration 30 --interval 0.5 --json `
  --trace .\artifacts\ignition-baseline.jsonl `
  engine.rpm vehicle.speed wheel.front_left.speed wheel.front_right.speed `
  wheel.rear_left.speed wheel.rear_right.speed driver.throttle `
  engine.manifold_pressure_relative brake.pressed brake.pressure `
  steering.angle_magnitude steering.angle_sign transmission.gear `
  transmission.target_gear_code vehicle.ambient_temperature
```

For Linux SocketCAN, the independent raw recorder also gives an ID inventory.
Bring the adapter up in listen-only mode only after confirming the interface,
bitrate and adapter supports it. `can_capture.py` itself has no transmit API but
does not set hardware listen-only mode for you.

```bash
mkdir -p artifacts/mqb-validation
python can_capture.py capture --channel can0 --seconds 30 \
  --max-frames 100000 --output artifacts/mqb-validation/ignition-baseline.jsonl
python can_capture.py inspect artifacts/mqb-validation/ignition-baseline.jsonl
```

Run the same watch for each case in the manifest, changing the output filename
and the stationary vehicle state. For the 1500/2000 rpm case, keep the vehicle
stationary and have a second person mark the ODIS reference/timestamps. Capture
brake, steering, cruise, door, lights and indicator changes one at a time. A
moving-vehicle dynamics case must have a second person monitoring the capture;
the driver does not operate the laptop.

## UDS fallback and source comparison

After the passive-only run, use a short window such as `passive_can_grace: 0.5`
for only the exact Tiguan ECU profiles. A CAN sample disables the same signal's
UDS poll; if the CAN value goes stale, the existing planner enables the exact
identity-bound UDS fallback. Do not add DIDs from the community Octavia list to
`poll_periods` or call them against the Tiguan.

Compare raw frame counts from `can_capture inspect`, MiniDash's decoded CAN
rows/availability in the JSONL trace, and synchronized ODIS values. Check the
full tuple (identity, frame or UDS bytes, state, reference reading and time),
including quality/sign bits and invalid transitions. A DBC match or plausible
number by itself is not promotion evidence. Do not commit VIN-bearing captures;
keep original raw logs local and record a redacted evidence summary.

For DTCs, save an ODIS/VCDS **read-only** report before/after the capture. The
current MiniDash UDS client does not implement `ReadDTCInformation (0x19)`.
Never clear DTCs, run output tests, coding or adaptation as part of this
workflow.
