# Tiguan / VNCI first live — read-only checklist

Status: **Engine physical route is confirmed; the first F190-only MiniDash
connection is authorized after J2534 doctor passes. Measurement polling remains
identity-gated.**

The confirmed real-vehicle route, observed with ODIS Engineering 17.0.1 on
2026-09-22, is:

| Field | Confirmed value |
|---|---|
| Logical link | `LL_EnginContrModul1UDS` |
| Protocol | UDS / ISO-TP / classic CAN |
| Request / response | `0x7E0` / `0x7E8` |
| CAN identifier | 11-bit |
| ISO-TP addressing | Normal |
| Bitrate | 500000 bit/s |

ODIS used Volkswagen D-PDU API. That proves the vehicle-side route, not that a
VNCI J2534 FunctionLibrary is installed or compatible. MiniDash must pass its
own J2534 preflight before opening the interface.

## Safety boundary

The first live phase sends only physical `22 F1 90` (VIN). MiniDash persists a
redacted snapshot: the VIN payload is neither printed in the structured PDU
trace nor written to `tiguan-identity.json`.

The default live CLI path sends only ReadDataByIdentifier (`0x22`); it does not
switch diagnostic sessions or send TesterPresent. This checklist does not
authorize session switching or TesterPresent. Coding, adaptations, flashing,
WriteDataByIdentifier, ClearDTC, RoutineControl, SecurityAccess, ECUReset,
InputOutputControl and CommunicationControl remain forbidden.

The validated F405 definition is exact-identity scoped to:

* `5NA907115D`, software `0003`;
* `06L907309B`, hardware `H31`;
* `EV_ECM20TFS0115NA907115D`, ASAM version `001004`.

F190 observes the vehicle VIN, not those ECU fields. Therefore a successful
F190 response proves connectivity but **does not unlock F405**. Do not put the
ODIS identity into config as if MiniDash observed it. An evidence-backed set of
ECU identity DIDs/layouts is still required before the first F405 watch.

## PREPARATION

- [ ] Windows laptop charged; Python 3.11+ of known 32/64-bit architecture
- [ ] vendor VNCI 6154A driver installed
- [ ] ODIS and every other diagnostic tester closed before live I/O
- [ ] battery support connected if available
- [ ] ignition ON, engine OFF
- [ ] VNCI connected by its vendor-supported link
- [ ] `artifacts/first-live` does not contain files from an earlier attempt

From PowerShell in the repository:

```powershell
git switch feature/diagnostic-data-import
git pull --ff-only origin feature/diagnostic-data-import
py -3 -m venv .venv
Set-ExecutionPolicy -Scope Process Bypass
& .\.venv\Scripts\python.exe -m pip install -r .\requirements-transport.txt -r .\requirements-data.txt
New-Item -ItemType Directory -Force .\artifacts\first-live | Out-Null
Get-ChildItem .\artifacts\first-live -Force
```

MiniDash refuses to overwrite discovery/watch artifacts.

## PHASE 0A — offline driver inventory

No VCI or vehicle I/O occurs in this phase.

```powershell
powershell -NoProfile -File .\tools\Inspect-Vnci.ps1 `
  -Output .\artifacts\first-live\vnci-windows.json
& .\.venv\Scripts\python.exe -m minidash_live doctor `
  --output .\artifacts\first-live\vnci-driver-inventory.json
```

Find the VNCI entry under `j2534` and copy its exact `FunctionLibrary`. Do not
select the Volkswagen D-PDU DLL. If several PassThru providers exist, select
the one whose registry entry belongs to the installed VNCI driver; MiniDash
does not silently choose one.

Requirements:

* file exists;
* `python_bits` equals the DLL PE bitness;
* every `j2534_required_exports_present` check is true;
* the path is a J2534 PassThru FunctionLibrary, not a D-PDU library.

`doctor` parses registry/PE metadata but intentionally does not load the DLL,
call `PassThruOpen`, select a device or send vehicle traffic. Production later
uses `PassThruOpen(NULL)`; vendor default-device selection must resolve VNCI.

## PHASE 0B — create local config and run complete doctor

Only the machine-local DLL path is missing from the committed template:

```powershell
$env:MINIDASH_J2534_DLL = 'C:\absolute\vendor\FunctionLibrary.dll'
Copy-Item .\config\examples\tiguan-vnci-engine-read-only.template.json .\connection.json
$Config = Get-Content .\connection.json -Raw | ConvertFrom-Json
$Config.transport.dll = $env:MINIDASH_J2534_DLL
$Config | ConvertTo-Json -Depth 20 | Set-Content -Encoding UTF8 .\connection.json

& .\.venv\Scripts\python.exe -m minidash_live doctor `
  --dll "$env:MINIDASH_J2534_DLL" `
  --config .\connection.json `
  --artifact-dir .\artifacts\first-live `
  --output .\artifacts\first-live\vnci-driver.json
```

Expected: `preflight_ready: true`, config `valid: true` and `read_only: true`,
Engine route `7E0/7E8`, complete exports, matching bitness and a writable
artifact directory. `vehicle_io_performed`, `dll_load_performed` and
`backend_initialization_performed` must remain false.

Do not alter the committed route fields. They are a single evidence-backed
record, not generic MQB defaults. `connection.json` is ignored by Git.

## PHASE 1 — first live F190 discovery

Ignition ON, engine OFF. Confirm ODIS is closed.

```powershell
& .\.venv\Scripts\python.exe -m minidash_live discover `
  --config .\connection.json `
  --output .\artifacts\first-live\tiguan-identity.json `
  --debug | Tee-Object -FilePath .\artifacts\first-live\discover-console.txt
```

Success requires:

* one available `engine` endpoint with request/response `7E0/7E8`;
* exactly one TX PDU: `22f190`;
* either a positive RX header `62f190` with `payload_redacted: vehicle.vin`, or
  an explicit NRC to diagnose;
* no configured CZPA/ASAM/part/software values appearing as observed ECU fields;
* deterministic endpoint fingerprint and consistent PDU metrics.

The persisted JSON intentionally contains `vehicle.vin: null` and a redacted
F190 field marker. Keep the original ODIS capture outside Git.

### STOP AND REVIEW

Do not continue if the only TX PDU is not `22f190`, or if transport/identity
output is unexpected.

## PHASE 2 — applicable signal catalog (no vehicle I/O)

```powershell
& .\.venv\Scripts\python.exe -m minidash_live signals `
  --config .\connection.json `
  --identity-snapshot .\artifacts\first-live\tiguan-identity.json `
  --all-implementations | Tee-Object -FilePath .\artifacts\first-live\signals.json
```

With the current F190-only snapshot, the exact F405 implementation must not
appear. An empty catalog is the expected fail-closed result. If F405 appears
without observed part/software/hardware/ASAM identity, stop and report an
applicability defect.

## PHASE 3 — F405 gate check only (no measurement authorized yet)

The following command is safe with the F190-only snapshot because planning
finds no applicable definition and creates no PollJob. It may be used only to
record the fail-closed planning result:

```powershell
& .\.venv\Scripts\python.exe -m minidash_live watch `
  --config .\connection.json `
  --identity-snapshot .\artifacts\first-live\tiguan-identity.json `
  --duration 5 --interval 1 --json `
  --trace .\artifacts\first-live\watch-gate.jsonl `
  engine.coolant_temperature
```

Expected: `no_applicable_definition`, no J2534 session, no `22f405`, and no
selected value. This is not a measurement test.

After safe ECU identity DIDs and layouts are independently evidenced and added
to local config, a fresh discovery must observe all required fields. Only then
may the same canonical watch path create the configured conservative 2-second
F405 PollJob. The expected decode is `uint8 - 40 degC`; compare it against ODIS
before promoting any further signal.

## Immediate STOP conditions

Stop on any of:

* DLL missing, export failure or Python/DLL bitness mismatch;
* `PassThruOpen`, `PassThruConnect`, filter or transport error;
* endpoint/config differing from `500000`, 11-bit Normal, `7E0/7E8`;
* response associated with an unexpected CAN ID;
* malformed F190 response or repeated NRC/timeout;
* any TX service other than `22 F190` during discovery;
* configured expected identity appearing as observed identity;
* changed fingerprint or reconnect verification failure;
* diagnostic warning in the vehicle or evidence of another active tester.

Abort with `Ctrl+C`. If the process does not exit, disconnect VNCI from the PC.
Do not clear DTCs.

## Artifacts to preserve

* `vnci-windows.json`
* `vnci-driver-inventory.json`
* `vnci-driver.json`
* local `connection.json` after removing the DLL path before sharing
* `tiguan-identity.json`
* `discover-console.txt`
* `signals.json`
* optional `watch-gate.jsonl`

The MiniDash artifacts above redact F190 payloads by default. Raw ODIS
`.CAN.vmt`/`ecf.log` files can still contain VIN, local paths and diagnostic
metadata. Keep originals immutable and outside Git; publish only a reviewed,
sanitized derivative with its transformation and date documented.

## What remains before actual F405 polling

Capture and review the raw request/response layout for enough safe read-only ECU
identity DIDs to observe the exact fingerprint after every connection. F190
alone cannot provide this. Do not guess common VAG identity DIDs or copy them
from another ECU family.
