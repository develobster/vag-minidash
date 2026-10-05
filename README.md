# MiniDash / vag-minidash

Development baseline: **`integration/standalone`**. Continue work on this branch.
See the [complete branch map](docs/development/branch-consolidation.md),
[delivery / regression results](docs/development/standalone-consolidation-2026-10-05.md),
and [Car Validation Mode / first stationary session](docs/validation/car-validation-mode.md).
Old remote branches remain intact.

Standalone automotive dashboard in development. ESP32-S3 firmware now owns
CAN/ISO-TP/OBD acquisition, bounded derived analytics and display publishing. Python remains a
development/research tool, not a production dependency. The autonomous path is
offline-tested; physical-board and vehicle acceptance are still required.

## Mobile Web Configurator

`python -m minidash_live.configurator` → http://127.0.0.1:8765.
Пять mobile-first страниц, canonical simulation/SSE, screens/widgets/themes,
host logging и versioned backup. Для телефона в одной сети с ПК:
`.\tools\Run-WebConfigurator.ps1 -Lan`. Существующий `watch` поддерживает
`--web-port 8765`. Синхронизация этого host-инструмента с ESP32 и OTA пока
недоступны; у прошивки есть собственный временный локальный Wi-Fi редактор.
Возможности, API, проверки и ограничения host-инструмента:
[web-configurator.md](docs/web-configurator.md).

Device Settings / Advanced расширены schema v2, capability detection, безопасным
Apply, защищёнными Wi-Fi credentials и diagnostic bundle:
[device-settings.md](docs/device-settings.md).

Первый дисплейный прототип ESP32-S3 (ST7789/ILI9341 backends), граница host/firmware и проверка
проводки описаны в [display-prototype.md](docs/hardware/display-prototype.md).

## Start here

- [Latest CAN/OBD → Analytics pipeline and scheduling fix](docs/research/analytics-pipeline-scheduling-2026-10-04.md)

- [Derived Analytics delivery, metrics, tests and remaining work](docs/research/engine-analytics-delivery-2026-10-04.md)
- [Read-only CZPA/DQ500/Haldex evidence procedure](docs/validation/engine-analytics-car-evidence-2026-10-04.md)
- [Standalone implementation, verification and remaining work](docs/research/standalone-final-2026-10-03.md)
- [Source/license review and competitor matrix](docs/research/standalone-sources-and-matrix.md)
- [First standalone bench/car test](REAL_CAR_TEST.md)
- [Current feature status and evidence limits](docs/status.md)
- [Build, tests and repository map](docs/development.md)
- [Current architecture](docs/architecture/overview.md)
- [Documentation index: current, research and historical](docs/README.md)
- [Repository consolidation audit and branch recommendations](docs/research/repository-consolidation-2026-09-30.md)
- [First real-car session: commands and current VNCI blockers](docs/validation/first-real-car-session.md)
- [Real-car preparation delivery and readiness matrix](docs/validation/real-car-preparation-2026-09-30.md)
- [Current state and practical next-work plan](docs/validation/practical-next-work-2026-10-01.md)
- [Focus instruments and vehicle scale profiles](docs/ui/focus-instruments-2026-10-01.md)
- [Project working instructions](AGENTS.md)

## Current capabilities

ESP32-S3 LVGL screens, XPT2046 affine calibration in NVS, explicit touch controls,
semantic navigation, English/Russian fonts, per-card signal selection, four Custom
pages, layouts/themes/widgets and an explicit temporary local Wi-Fi Web editor.
The Python runtime has identity-scoped knowledge, passive CAN/UDS acquisition,
source arbitration, CLI/trace facilities and offline recorded CAN replay.

The UI has 92 canonical IDs (66 derived), six analytics presets, a paged Widget
Picker, Focus min/max/history/source and configurable events. Schema 4 migrates
older UI settings. Statistics/events stay bounded in RAM; all new thresholds
default off and manufacturer bindings stay disabled.

The UI receives canonical samples from the native CAN task. Default builds keep
CAN disabled; explicit `sdkconfig.standalone` and `sdkconfig.listen-only` presets
provision GPIO15 TX/GPIO5 RX for reviewed installation wiring, not assumed wiring.
Standalone supports standard 11-bit/500-kbit OBD and stationary Mode03 stored
emissions DTC reading from the selected ECU. No VAG profiles ship enabled; DTC
clear, coding/adaptation and product BLE remain unimplemented. GPIO4 retains the owner-accepted PWM backlight. QUIET stops polling, not electrical power:
use the physical switch until a protected automatic power system is validated.

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
