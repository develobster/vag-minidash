# Tiguan / VNCI first live — read-only checklist

Status: **blocked before vehicle I/O until the Engine physical diagnostic route
is evidenced**. The repository contains no verified Engine request/response CAN
IDs, addressing mode or bitrate for this VNCI/ODIS/car path. `7E0/7E8` in
`docs/pc-backend.md` is explicitly illustrative and must not be copied.

Reference vehicle expectations from the ODIS handoff are CZPA and
`EV_ECM20TFS0115NA907115D/001004`. They are review hints, not observed MiniDash
identity and are deliberately absent from runtime identity fields.

## What this run is allowed to send

Phase 1 uses only physical `0x22 F190` (VIN). The runtime contains no coding,
adaptation, flashing, SecurityAccess, ClearDTC, ECU reset, write-DID or routine
command. Do not add another identification DID until its raw layout has been
captured for this ECU. Do not use functional address `7DF`.

`watch` is **not authorized by this checklist yet**. The exact CZPA DID/name
associations exist, but there is no evidenced ECU identity probe for reconnect
verification and no raw decoder for coolant or RPM. A value rendered by ODIS is
not enough to claim a raw byte scale.

## One evidence gate

Obtain one traceable Engine physical-route record from this exact
VNCI/ODIS/Tiguan setup. It must contain all of:

- request CAN ID;
- response CAN ID;
- CAN bitrate;
- addressing mode (the current backend accepts only Normal 11-bit ISO-TP);
- provenance: capture/log filename, timestamp and how the route was observed.

Treat this as one endpoint record, not four independent guesses. If it indicates
29-bit, extended/mixed addressing, CAN FD, or a D-PDU-only path, stop: the current
generic J2534 backend is not compatible with that route.

## PREPARATION

- [ ] Windows laptop charged and Python 3.11+ installed
- [ ] VNCI vendor driver installed; no replacement driver installed for this test
- [ ] VNCI connected by the vendor-supported link
- [ ] ODIS and every other diagnostic tester closed before MiniDash vehicle I/O
- [ ] battery support connected if available
- [ ] ignition ON
- [ ] engine OFF
- [ ] verified Engine physical-route record available
- [ ] an empty `artifacts/first-live` directory available

From a PowerShell prompt in the repository:

```powershell
py -3 -m venv .venv
Set-ExecutionPolicy -Scope Process Bypass
& .\.venv\Scripts\python.exe -m pip install -r .\requirements-transport.txt -r .\requirements-data.txt
New-Item -ItemType Directory -Force .\artifacts\first-live | Out-Null
Get-ChildItem .\artifacts\first-live -Force
```

The last command must show no output files from an earlier run. MiniDash refuses
to overwrite discovery/watch artifacts.

## PHASE 0A — driver inventory (no VCI or vehicle I/O)

```powershell
powershell -NoProfile -File .\tools\Inspect-Vnci.ps1 `
  -Output .\artifacts\first-live\vnci-windows.json
& .\.venv\Scripts\python.exe -m minidash_live doctor `
  --output .\artifacts\first-live\vnci-driver-inventory.json
```

Find the VNCI entry under `j2534` and copy its exact `FunctionLibrary` value.
Do not select a DLL merely because its filename contains `VAS`, `VNCI` or
`PassThru`. The Python architecture must match the DLL (`python_bits` equals
`dlls[].bits`) and all `j2534_required_exports_present` checks must be true.

`doctor` parses PE exports but intentionally does not load the DLL, call
`PassThruOpen`, select a device or touch the vehicle. The production backend
later calls `PassThruOpen(NULL)`, so the vendor driver/default-device selection
must resolve the connected VNCI; MiniDash has no vendor-specific device picker.

## Build the local config after the evidence gate

Do not modify the committed template. Set values copied from the local driver
inventory and verified endpoint record:

```powershell
$env:MINIDASH_J2534_DLL = 'C:\absolute\vendor\FunctionLibrary.dll'
$env:MINIDASH_ENGINE_TXID = 'VERIFIED_HEX_REQUEST_ID'
$env:MINIDASH_ENGINE_RXID = 'VERIFIED_HEX_RESPONSE_ID'
$env:MINIDASH_CAN_BITRATE = 'VERIFIED_INTEGER_BITRATE'
$env:MINIDASH_ADDRESS_EVIDENCE = 'capture filename + timestamp + observation method'

$Config = Get-Content .\config\examples\tiguan-vnci-engine-read-only.template.json -Raw | ConvertFrom-Json
$Config.transport.dll = $env:MINIDASH_J2534_DLL
$Config.ecus.engine.txid = $env:MINIDASH_ENGINE_TXID
$Config.ecus.engine.rxid = $env:MINIDASH_ENGINE_RXID
$Config.ecus.engine.bitrate = [int]$env:MINIDASH_CAN_BITRATE
$Config.ecus.engine.address_evidence = $env:MINIDASH_ADDRESS_EVIDENCE
$Config | ConvertTo-Json -Depth 10 | Set-Content -Encoding UTF8 .\connection.json
```

`connection.json` is ignored by Git. It contains one Engine endpoint and only
the `F190` probe. It contains no expected CZPA/ASAM values, measurement DIDs,
poll periods, seeded vehicle identity or passive-CAN setup. This prevents
reference facts such as `MQB` from appearing beside observed fields without
provenance.

## PHASE 0B — complete offline doctor

```powershell
& .\.venv\Scripts\python.exe -m minidash_live doctor `
  --dll "$env:MINIDASH_J2534_DLL" `
  --config .\connection.json `
  --artifact-dir .\artifacts\first-live `
  --output .\artifacts\first-live\vnci-driver.json
```

Expected: `preflight_ready: true`, all four dependencies installed, config
`valid: true` and `read_only: true`, `ecu_roles: ["engine"]`, matching Python/DLL
bitness, complete required exports and a writable artifact directory.
`vehicle_io_performed`, `dll_load_performed` and
`backend_initialization_performed` must all be false.

Stop if any check fails. In particular, changing Python bitness is acceptable;
substituting an unrelated DLL or inventing an endpoint is not.

## PHASE 1 — discover, ignition ON / engine OFF

Connect VNCI to the vehicle only after Phase 0 passes. Confirm ODIS is closed.

```powershell
& .\.venv\Scripts\python.exe -m minidash_live discover `
  --config .\connection.json `
  --output .\artifacts\first-live\tiguan-identity.json `
  --debug | Tee-Object -FilePath .\artifacts\first-live\discover-console.txt
```

Expected successful transport result:

- one `engine` endpoint is discovered;
- `request_id` and `response_id` equal the verified config;
- `fields` contains a successful `F190 → vehicle.vin`, or `errors` contains an
  explicit NRC;
- `fingerprint` is deterministic and contains the configured/evidenced endpoint
  key (`logical_role`, request/response IDs), while `fields` contains the actual
  wire observations; neither may acquire CZPA/ASAM/part/software merely because
  those are expected;
- metrics show one configured endpoint and PDU TX/RX counts consistent with the
  trace.

A timeout/transport failure places Engine in `unavailable` and returns a failing
exit status. That is not a discovered ECU. Preserve all output.

### STOP AND REVIEW

Do not run `signals` or `watch` until the identity JSON and embedded debug PDU
events have been reviewed. Check that the only TX PDU is `22f190`.

## PHASE 2 — catalog only

After successful review:

```powershell
& .\.venv\Scripts\python.exe -m minidash_live signals `
  --config .\connection.json `
  --identity-snapshot .\artifacts\first-live\tiguan-identity.json `
  --all-implementations | Tee-Object -FilePath .\artifacts\first-live\signals.json
```

The catalog may be empty: the config deliberately does not seed `MQB`, and exact
CZPA UDS definitions must remain absent until an ECU identity probe has actually
observed the applicable ECU fingerprint. This phase sends no vehicle requests
when `--identity-snapshot` is supplied.

### STOP AND REVIEW

If an exact CZPA UDS definition appears without observed ECU identity fields,
stop and report it as an applicability bug.

## PHASE 3 — first measurement (currently blocked)

There is intentionally no approved `watch` command yet. Before one is issued we
need:

1. an evidenced, safe ECU identity DID/layout that observes enough of the CZPA
   fingerprint and can be re-read after reconnect;
2. a raw `0x62 F405 ...` capture synchronized with ODIS coolant temperature;
3. a tested decoder including length, endian, scale, offset and invalid values.

Only then should a low-rate, finite-duration
`watch engine.coolant_temperature` command be added. RPM follows after coolant,
with the engine running and comparison against cluster/ODIS. No high-frequency
polling belongs in this first test.

## Immediate abort conditions

Stop MiniDash, switch ignition off when safe, and preserve artifacts on:

- any J2534 driver error or repeated transport timeout;
- any TX service other than `22 F190` in Phase 1;
- unexpected `0x10`, `0x3E` or measurement `0x22` during discovery;
- identity mismatch or changed endpoint fingerprint;
- repeated NRC, unstable session/bus, or reconnect loop;
- any new vehicle diagnostic warning;
- any indication that ODIS/another tester is still active.

The abort action is `Ctrl+C`; if the process does not exit, disconnect VNCI from
the PC before changing anything else. Do not clear DTCs as part of this run.

## Artifacts to preserve

- `vnci-windows.json`
- `vnci-driver-inventory.json`
- `vnci-driver.json`
- local `connection.json` (share only after removing local DLL path)
- the physical-route evidence record
- `tiguan-identity.json`
- `discover-console.txt`
- `signals.json`, only after Phase 2

`discover` embeds its structured PDU debug events in `tiguan-identity.json`; it
does not create a separate `discover-trace.jsonl`. `watch.jsonl` and a metrics
summary are future artifacts after measurement authorization.

Keep the original local VIN-bearing capture unchanged. Before publishing or
committing a derivative, replace the VIN consistently with `<REDACTED_VIN>`,
remove the local DLL path, add a note that the copy was sanitized, and retain
the original capture hash/date separately. Never commit the original capture.
