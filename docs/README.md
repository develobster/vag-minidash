# Указатель документации

Начать с [состояния](status.md), [разработки](development.md) и
[архитектуры](architecture/overview.md). Даты и количества тестов исторических
отчётов не являются актуальной приёмкой.

## Актуальные материалы

| Тема | Документы |
|---|---|
| Стендовая загрузка единой версии 14 октября | [Образ 875106b, 27 проверок UI, подтверждённые дисплей/тач и сохранность настроек](hardware/standalone-flash-2026-10-14.md), [машинная сводка](hardware/standalone-flash-verification-2026-10-14.json) |
| Актуальная интеграция от 14 октября | [Слияние, защита стека, свежие регрессии и оставшаяся работа](development/standalone-realignment-2026-10-14.md) |
| Дисплей, консоль и ручная прошивка | [Инструкция для единой ветки](hardware/manual-flashing-ru.md), [стендовая приёмка 13 октября](hardware/can-display-acceptance-2026-10-13.md), [исходная диагностика](hardware/can-display-startup-2026-10-13.md) |
| Единая база разработки | [Матрица веток](development/branch-consolidation.md), [поставка / регрессии / память](development/standalone-consolidation-2026-10-05.md) |
| Русский интерфейс и стендовая прошивка | [Перевод, проверки, сохранение настроек и результат загрузки](development/russian-ui-flash-2026-10-05.md) |
| Автономная проверка автомобиля | [Режим, экспорт, готовность и первый сеанс на стоянке](validation/car-validation-mode.md) |
| Host mobile configurator | [Web](web-configurator.md), [settings](device-settings.md); development tool only |
| Latest analytics pipeline / scheduling | [Fix and measured cases](research/analytics-pipeline-scheduling-2026-10-04.md), [verification](research/analytics-pipeline-verification-2026-10-04.json) |
| Derived Analytics / full metrics / evidence | [Delivery](research/engine-analytics-delivery-2026-10-04.md), [design/provenance](research/derived-engine-design-2026-10-04.md), [branch audit](research/analytics-integration-2026-10-04.md), [verification JSON](research/engine-analytics-verification-2026-10-04.json), [car collection](validation/engine-analytics-car-evidence-2026-10-04.md) |
| Standalone implementation / research / tests | [Current report](research/standalone-final-2026-10-03.md), [source/license matrix](research/standalone-sources-and-matrix.md), [verification JSON](research/standalone-verification-2026-10-03.json), [REAL_CAR_TEST](../REAL_CAR_TEST.md) |
| Integration baseline / branches / acceptance | [Consolidation audit](research/repository-consolidation-2026-09-30.md) |
| Feature evidence | [Status matrix](status.md), [UI implementation/hardware record](ui/status.md) |
| Host diagnostic runtime | [End-to-end](runtime/end-to-end.md), [knowledge](architecture/vehicle-knowledge.md) |
| Focus needles and vehicle scales | [Current delivery, catalogue sources, migration and acceptance](ui/focus-instruments-2026-10-01.md) |
| Device architecture / portability | [UI architecture](ui/architecture.md), [targets](architecture/targets.md) |
| Configuration / settings | [Configuration](ui/configuration.md), [settings](ui/user-settings.md), [USB bridge](ui/host-bridge.md) |
| Hardware and display/touch | [ESP32 bring-up](hardware/esp32s3-bringup.md), [display prototype](hardware/display-prototype.md) |
| First physical vehicle gate | [Current first-session runbook](validation/first-real-car-session.md), [preparation/readiness report](validation/real-car-preparation-2026-09-30.md); earlier [Tiguan first live](runtime/tiguan-first-live.md), [MQB validation](runtime/tiguan-mqb-live-validation.md), [manifest](runtime/tiguan-mqb-validation-manifest.json) |
| Practical next work | [Current project state, verified build and phased bench/vehicle plan](validation/practical-next-work-2026-10-01.md) |
| Offline evidence processing | [CAN replay](runtime/can-replay.md), [artifacts](artifacts.md), [fixture notices](../tests/fixtures/README.md) |

## Research: preserve, check against code before implementation

- [Diagnostic data sources / importer scope](research/vag-diagnostic-data-sources.md)
- [Ecosystem research and archaeology](research/ecosystem-2026-09-30.md)
- [Research/replay/MCD delivery results](research/ecosystem-2026-09-30-results.md)
- [UX design alternatives](ui/ux-design.md) and adjacent SVGs: design references,
  not screenshots proving physical acceptance.
- [Transport/VNCI investigation](vnci.md), [earlier DID research](research.md).

## Historical or version-specific notes

Preserved paths keep old links working. These documents may contain earlier
architecture, test totals, session defaults or planned functionality:

- [Previous README](history/README-pre-consolidation.md): version 0.6/0.8/0.10 narrative.
- [Technical audit](TECHNICAL_AUDIT.md), [early audit](audit.md),
  [runtime fork](RUNTIME_FORK.md), [PC backend](pc-backend.md).
- Root [ARCHITECTURE](../ARCHITECTURE.md), [HARDWARE](../HARDWARE.md),
  [DIAGNOSTICS](../DIAGNOSTICS.md), [TRANSPORT](../TRANSPORT.md),
  [RESEARCH](../RESEARCH.md): original design/lab notes. Simulator DTC/coding and
  intended CAN hardware must not be interpreted as current device features.

No research/evidence was removed. One duplicated navigation purpose is resolved
by this index; historical text is retained, rather than merging contradictory
versions into an apparent current specification.

## Diagnostic feature branch (2026-10-22)

[Research and validation](research/vag-diagnostics-deep-research.md) | [Architecture](architecture/vag-diagnostic-discovery.md) | [Next car test](validation/vag-diagnostics-car-test-plan.md). P0/P1 fixes and Web connection doctor; experimental gateway helpers are offline-only. No new active UDS queries or vehicle acceptance.
