# MiniDash / vag-minidash

Standalone automotive dashboard in development, using an ESP32-S3 display/touch
prototype and an existing Python diagnostic runtime. The device UI works on the
bench; autonomous vehicle acquisition on the MCU is not implemented yet.

## Start here

- [Current feature status and evidence limits](docs/status.md)
- [Build, tests and repository map](docs/development.md)
- [Current architecture](docs/architecture/overview.md)
- [Documentation index: current, research and historical](docs/README.md)
- [Repository consolidation audit and branch recommendations](docs/research/repository-consolidation-2026-09-30.md)
- [First real-car session: commands and current VNCI blockers](docs/validation/first-real-car-session.md)
- [Real-car preparation delivery and readiness matrix](docs/validation/real-car-preparation-2026-09-30.md)
- [Focus instruments and vehicle scale profiles](docs/ui/focus-instruments-2026-10-01.md)
- [Project working instructions](AGENTS.md)

## Current capabilities

ESP32-S3 LVGL screens, XPT2046 affine calibration in NVS, explicit touch controls,
semantic navigation, English/Russian fonts, per-card signal selection, four Custom
pages, layouts/themes/widgets and an explicit temporary local Wi-Fi Web editor.
The Python runtime has identity-scoped knowledge, passive CAN/UDS acquisition,
source arbitration, CLI/trace facilities and offline recorded CAN replay.

The UI receives canonical samples through a publishing API or optional USB host
bridge. Python transport/UDS/providers do not run inside ESP32 firmware. MCU CAN
is disabled. BLE is bring-up work; live DTC read/clear and coding/adaptation are
not product implementations. Backlight is powered from 3.3 V and shows FIXED.

MQB/UDS candidates and synthetic replay are not vehicle validation. Only the
documented F405 coolant decoder has exact Engine identity evidence from an
ODIS/physical trace; MiniDash's physical VNCI route still needs its own acceptance.
See the current [first-car procedure](docs/validation/first-real-car-session.md) and the
[MQB validation workflow](docs/runtime/tiguan-mqb-live-validation.md).

The first-session tools record private sequential UDS evidence and replay it
through production discovery/planning/decoding/store/arbitration. Windows VNCI
remains blocked: locally installed D-PDU DLLs are not J2534 FunctionLibrary DLLs;
MiniDash D-PDU and Windows passive CAN adapters are not implemented. Check offline:

```sh
python tools/real_car_preflight.py --config config/examples/tiguan-vnci-engine-read-only.template.json
```

Expected current result: `BLOCKED`, exit 1, explicit blockers. No vehicle I/O.

## Offline checks

```sh
python -m pip install -r requirements-transport.txt -r requirements-data.txt
python -m unittest discover -s tests -v
```

For the native core, editor logic, firmware and repeatable acceptance command,
see [development](docs/development.md). No test command here flashes a device.
Private captures, OEM databases, factory backups and generated builds stay local;
see [artifact policy](docs/artifacts.md). Fixture licenses are retained in
[tests/fixtures/README.md](tests/fixtures/README.md); a root project license has
not yet been selected.

Earlier version notes are preserved in the
[historical README](docs/history/README-pre-consolidation.md).
