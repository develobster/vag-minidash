# Research ledger — authoritative Tiguan test vehicle data

The source of truth is now `data/vehicles/tiguan-5na-czpa-dq500.json` and its four ECU profiles. The earlier `profiles/diagnostic/*.json` and this document's previous revision are retained as historical v0.7 research; where they differ, the 2026-09-20 handoff and the `data/` catalog take priority.

Every catalog entry stores its DID, logical ECU, exact ECU fingerprint, canonical signal name, unit, raw-layout placeholders, source, validation status, and notes. `raw.decoder` is deliberately `null` throughout. ODIS has supplied rendered values, not a verified on-wire byte layout. No scale, endian, signedness, enum or sentinel is inferred from a label or a single physical value.

## Status values

| Status | Meaning |
|---|---|
| `CONFIRMED_LIVE` | Observed changing or valid on the actual test vehicle in ODIS. It is not decoder validation. |
| `CONFIRMED_ODIS` | DID/name exists in ODIS, but its live behaviour was not independently exercised. |
| `CANDIDATE` | A useful but unresolved signal or validity state. |
| `UNSUPPORTED` | This ECU/DID renders unusable data; it remains raw-probeable and is never shown as an actual value. |

## Fingerprints — do not merge revisions

| ECU | Software / hardware | ASAM returned by ECU | Separate ODX container observation |
|---|---|---|---|
| J623 engine | `5NA907115D` / `0003`; `06L907309B` / `H31` | `EV_ECM20TFS0115NA907115D` / `001004` | approximately `001003` |
| J743 DQ500 | `0DL300011P` / `2004`; `0GC927711G` / `H06` | `EV_TCMDQ500021` / `001001` | — |
| J104 ESC | `5Q0614517CE` / `0413`; `5Q0614517AC` / `H25` | `EV_Brake1UDSContiMK100IPB` / `036010` | — |
| J492 Haldex | `0CQ907554H` / `7083`; `0CQ525130` / `H52` | `EV_AllWheelContrHA1VW37X` / `004013` | approximately `004016` |

J492 also observed coding `01 01`, parameter set `v1260x015G0`, version `0000`. These are identification evidence only; the project performs no coding.

## Live observations that constrain decoder work

- Engine OFF: `F405` ~64 °C, `16F3` 0 rpm, `F433` 99 kPa, `11F0` ~993.44 hPa, `11F1` ~280.08 hPa, `1632` −273.1 °C invalid.
- Warm idle: RPM ~840, `11F0` ~992–993 hPa, `11F1` ~448–500 hPa; `11DC` ~397.42 hPa, `11DD` ~329.84 hPa, `11DA` ~374.53 hPa. With ambient ~990 hPa, MAP is ~0.37–0.40 bar absolute. This is a decoder sanity check, not a proposed formula.
- Warm idle fuel rail: `159F` ~8.276 MPa actual, `11D7` ~8.928 MPa specified.
- DQ500 stationary R: K2 actual/spec pressure ~1.94 / 1.82 bar; K1 near zero. Neutral/no displayed gear pressure ~0.03–0.05 bar. The ~−100 Nm clutch torque readings are not transmitted torque until raw encoding is proven.
- ABS `1816`: released switch/not operated, pressure ~0.30 bar; pressed/operated, pressure 39.00 bar. `1800`–`1804` and `1816` are structured DIDs.
- J492 stationary: module 23.0 °C, fin 22.90 °C, clutch 23.20 °C, pump 0.024 A, PWM 0%, voltage 0 V, `2BF3` blockage 0.00%. No Haldex pressure or torque DID has been found.

For a future boost gauge use only the documented relationship after decoding is confirmed:

`boost_gauge = engine.charge_pressure_actual - engine.ambient_pressure`

The project will not use a hard-coded 1000 hPa reference.

## Raw capture protocol

1. Do not run ODIS and MiniDash against the same VCI/ECU concurrently.
2. Record the PC/VNCI API evidence first (`docs/vnci.md`). Physical CAN IDs and bitrate must be supplied explicitly; the profile has none.
3. Probe one DID at a time with a contemporaneous ODIS screenshot/value and save the resulting JSONL unchanged.
4. Capture at least three materially different operating points before proposing length, signedness, endian, scale, offset or enum mapping.
5. Add an evidence-linked decoder and turn the actual raw responses into regression fixtures only after the values agree across the captured points.

The most useful first engine batch is `F405`, `F433`, `16F3`, `11BD`, `11F0`, `11F1`, `11DC`, `11DD`, `11DA`, `159F`, `11D7`. The reader accepts an unknown DID and records it as `unknown.did.<DID>` rather than crashing.
