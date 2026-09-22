# PC reader / `probe`

For the first VNCI/Tiguan connection use the stricter staged checklist in
[`runtime/tiguan-first-live.md`](runtime/tiguan-first-live.md). The Engine
physical route is confirmed there; this lower-level `probe` documentation does
not authorize bypassing discovery, identity matching or reconnect verification.

The live reader is a bounded, **read-only** UDS `0x22` client. It cannot code, flash, run adaptations/basic settings, request security access, write a DID, clear DTCs, or send a functional broadcast. It reads one DID per request, no automatic retry; a timeout or malformed response taints the connection and stops further traffic.

```powershell
python -m minidash_live doctor --output vnci-python.json
python -m minidash_live profile data/ecu_profiles/j623-czpa-5na907115d.json
```

`doctor` reads Windows registration/PE metadata only; it does not load a DLL or open the vehicle.

## Connection configuration

The exact J623 profile now contains its confirmed route. For the first run copy
`config/examples/tiguan-vnci-engine-read-only.template.json` to local
`connection.json` and set only the installed J2534 FunctionLibrary path. Do not
commit that path or any VIN-bearing capture.

```json
{
  "transport": {
    "kind": "j2534",
    "dll": "C:\\Path\\To\\VerifiedPassThru.dll"
  },
  "ecus": {
    "engine": {
      "txid": "7E0",
      "rxid": "7E8",
      "bitrate": 500000,
      "address_evidence": "Captured from this specific VNCI/ODIS setup"
    }
  }
}
```

For this exact validated Engine profile, `7E0/7E8`, 500 kbit/s and 11-bit Normal
ISO-TP are confirmed real-vehicle evidence, not community defaults. They must
not be copied to another ECU/vehicle. Legacy `fingerprint_observation` belongs
only to bounded research `probe`; it is configured expectation and is never
treated as observed runtime identity.

Supported real transports are `socketcan` and a generic Windows `j2534` adapter. `replay` is an offline synthetic fixture transport. `d_pdu` and `doip` are named in the abstraction but deliberately refuse to open until their VNCI-specific evidence has been captured; there is no fallback or protocol guessing.

## Low-level research probe — not the first live command

The first real command is `discover` with F190, as documented in the garage
checklist. Do not use `probe` to send F405 before the identity gate succeeds.
For a separately reviewed research session, `probe` remains available as a
bounded raw reader:

```powershell
python -m minidash_live probe --config .\connection.json --ecu engine --did F190 --trace .\identity-research.jsonl --raw
```

`--ecu` uses the vehicle profile aliases `engine`, `transmission`, `abs`, `awd`; it does not derive a profile from the vehicle name. Unknown DIDs are allowed for raw research. A pre-existing `--trace` file is refused without changing it.

Each successful `uds_exchange` line includes timestamp, vehicle profile, logical
ECU, service `0x22`, DID, request, pending count and conservative decoded
object. F190 response payloads are redacted from trace/output. Other responses
remain complete raw evidence. Unsupported decoders stay `raw_only`; F405 is the
only exact-profile vehicle-tested scalar decoder at this point.

Offline development is covered by `ReplayTransport` and `tests/test_probe.py`; its synthetic bytes are explicitly not vehicle evidence.

## Fingerprint matching

For any collected identity record, matching is exact and independent of `Tiguan` text:

```powershell
python -m minidash_live match --fingerprint .\ecu-fingerprint.json
```

Only a complete exact `sw_part`, `sw_version`, `hw_part`, `hw_version`, `asam_id`, `asam_version` match selects a catalog profile. An ODX/container revision is stored separately and cannot match ECU-reported ASAM version.
