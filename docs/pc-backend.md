# PC reader / `probe`

The live reader is a bounded, **read-only** UDS `0x22` client. It cannot code, flash, run adaptations/basic settings, request security access, write a DID, clear DTCs, or send a functional broadcast. It reads one DID per request, no automatic retry; a timeout or malformed response taints the connection and stops further traffic.

```powershell
python -m minidash_live doctor --output vnci-python.json
python -m minidash_live profile data/ecu_profiles/j623-czpa-5na907115d.json
```

`doctor` reads Windows registration/PE metadata only; it does not load a DLL or open the vehicle.

## Connection configuration

Physical CAN addresses are intentionally absent from vehicle profiles. After gathering actual evidence, create a local `connection.json` (do not commit local paths/IPs):

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
  },
  "fingerprint_observation": {
    "sw_part": "5NA907115D",
    "sw_version": "0003",
    "hw_part": "06L907309B",
    "hw_version": "H31",
    "asam_id": "EV_ECM20TFS0115NA907115D",
    "asam_version": "001004"
  }
}
```

The numbers above are an illustrative normal-addressing layout, not verified CAN IDs for this car. Replace them only with the captured IDs. `fingerprint_observation` is optional; when present it is compared exactly to the selected profile and written to the session trace. It exists to support safe matching while raw identity DID layouts are still unproven.

Supported real transports are `socketcan` and a generic Windows `j2534` adapter. `replay` is an offline synthetic fixture transport. `d_pdu` and `doip` are named in the abstraction but deliberately refuse to open until their VNCI-specific evidence has been captured; there is no fallback or protocol guessing.

## First real probe

With a verified transport/config, ignition state documented, and ODIS disconnected from the VCI:

```powershell
python -m minidash_live probe --config .\connection.json --ecu engine --did F405 --did F433 --did 16F3 --did 11BD --did 11F0 --trace .\engine-first.jsonl --raw
```

`--ecu` uses the vehicle profile aliases `engine`, `transmission`, `abs`, `awd`; it does not derive a profile from the vehicle name. Unknown DIDs are allowed for raw research. A pre-existing `--trace` file is refused without changing it.

Each successful `uds_exchange` line includes timestamp, vehicle profile, logical ECU, full fingerprint record, service `0x22`, DID, complete request and response PDUs, pending count, and the conservative decoded object. Until a decoder is evidence-backed, its result is `raw_only` and `value` stays `null`.

Offline development is covered by `ReplayTransport` and `tests/test_probe.py`; its synthetic bytes are explicitly not vehicle evidence.

## Fingerprint matching

For any collected identity record, matching is exact and independent of `Tiguan` text:

```powershell
python -m minidash_live match --fingerprint .\ecu-fingerprint.json
```

Only a complete exact `sw_part`, `sw_version`, `hw_part`, `hw_version`, `asam_id`, `asam_version` match selects a catalog profile. An ODX/container revision is stored separately and cannot match ECU-reported ASAM version.
