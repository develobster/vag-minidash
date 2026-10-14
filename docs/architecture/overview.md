# Актуальная архитектура

Обновлено 2026-10-14. База разработки: `integration/standalone`.
Автомобиль → разъём OBD-II → защищённое оборудование MiniDash → ESP32-S3
→ CAN/TWAI → ISO-TP → UDS/OBD-II → сбор → канонические сигналы → производные
метрики / аналитика → ограниченные история/статистика/события → интерфейс/дисплей.

После прошивки ПК, Python, VNCI, ODIS, J2534, телефон, Raspberry Pi, внешний
OBD-адаптер и конфигуратор на ПК необязательны. Они остаются инструментами
разработки, исследований, воспроизведения, диагностики, прошивки и проверки доказательств.

Режим проверки автомобиля использует общий сбор и канонический мост.
Пассивный набор всегда остаётся в аппаратном прослушивании. Активный собирает
четыре стандартных источника с минимумом 250 мс между запросами приложения,
сохраняя проверки чтения/идентичности/длины. Фиксированные буферы кадров/обменов
передают согласованный снимок в GET `/api/validation` через существующую
временную точку доступа. Экспорт не вызывает CAN-трафик. Фирменные профили
остаются NULL. Отсутствие телефона/сборщика не меняет сбор и дисплей.
См. [режим и ограничения](../validation/car-validation-mode.md).

Восстановленный мобильный конфигуратор на ПК получает канонический снимок
после выбора источников в среде ПК; настройки/записи остаются файлами ПК.
Синхронизация настроек MCU и запись в ЭБУ не добавлены. Сбор, воспроизведение
и аналитика устройства продолжают использовать общие исходники прошивки;
исторические симуляторы остаются исследовательскими инструментами.

Снимок 92 сигналов выделяется один раз на время жизни задачи UI и под той же
блокировкой копируется из основного хранилища. Отрисовка и диагностика используют
этот общий снимок. Крупная копия `md_vehicle_status_t` в диагностике хранится
в статическом буфере, принадлежащем единственной задаче UI; статус заполняется
через прежний mutex. Дополнительные выделения памяти на кадр не добавлены.
При сборке проверяются собственные кадры и известные вложенные пути UI/NVS.
См. [интеграцию и пределы проверки стека](../development/standalone-realignment-2026-10-14.md).

Локализация затрагивает представление: русские подписи не меняют канонические
ID, единицы, масштаб или планировщик. GET `/api/signals` дополнен `label_ru`,
существующий `label` сохранён. Схема настроек остаётся 4.
См. [перевод и стендовая загрузка](../development/russian-ui-flash-2026-10-05.md).

Production runs on ESP32: transceiver/TWAI → native
ISO-TP/OBD and evidence-gated UDS/passive → per-source samples/TTL/arbitration
→ canonical SignalStore → DerivedSignalEngine → windows/events/session/trip
→ canonical publication → physical UI. UI demand controls
the native scheduler, with bounded freshness rescue under sampling pressure.
The production/replay canonical bridge is `md_acq_publish_canonical`; see the
[pipeline follow-up](../research/analytics-pipeline-scheduling-2026-10-04.md). No PC or Python dependency.

`vehicle_app.c` owns RTOS/CAN; `diagnostic_transport.c` owns bounded ISO-TP RX;
`acquisition.c` owns discovery/identity/scheduling/source samples.
`derived_engine.c`, `window_statistics.c`, `performance_timer.c` consume canonical
signals only; they never read CAN or issue requests. Fixed storage, zero sample
allocation, PSRAM BSS on ESP32 and 100 ms ticks bound their work. Default disables CAN;
the explicit installation preset enables standard OBD, not unverified VAG profiles.
See [limits and verification](../research/standalone-final-2026-10-03.md).

## Preserved host development/research architecture

The host pipeline retains transport, configured ECU discovery, identity context,
knowledge providers, signal registry, planning, passive/UDS acquisition, store and
source arbitration. CLI/debug/replay/optional USB bridge remain development tools.

| Boundary | Existing implementation | Contract |
|---|---|---|
| Wire/VCI | `minidash_live/transports.py`, `j2534.py`, `uds.py` | Evidenced addresses, bounded transactions, response validation; no default broadcast |
| Discovery/identity | `discovery.py`, `identity.py` | Configured endpoints and observed identity; snapshot alone does not validate a decoder |
| Knowledge | `knowledge.py`, `vag.py`, `registry.py`, `mcd_import.py`, `dbc.py`; `data/` | Provider/applicability/provenance gates; canonical/raw namespaces stay separate |
| Planning/acquisition | `pipeline.py`, `runtime.py`, `passive.py` | Passive grace, fallback eligibility, bounded scheduler, reconnect/backoff |
| Values | `runtime.py:SignalStore`, `arbitration.py` | Timestamp/TTL/quality/source selection; unavailable values are not zero |
| CLI/trace | `__main__.py`, `api.py`, `display_bridge.py`, `can_capture.py` | Canonical consumers and receive-only recorder; trace creation preserves existing output |
| Offline replay | `replay.py`, `can_capture.py:CanTraceReader` | Existing engine feed/snapshot path; injected clock; no transport or ECU session |
| Embedded model | `firmware/main/ui_core`, `ui_model`, `ui_configuration`, `ui_platform` | Canonical sample vocabulary, navigation/config/presentation; no vehicle IDs |
| ESP32 adapters | `display_app`, `ui_screens`, `ui_web`, `*_nvs`, panel/touch backends | LVGL, NVS, temporary AP/HTTP, SPI hardware |
| Native core | `core/CMakeLists.txt` | Compiles the same physical acquisition/transport/UI C modules, without LVGL/ESP-IDF |

The embedded presentation store is not the host acquisition SignalStore or
SourceArbitrator. The current firmware vocabulary contains 92 canonical IDs,
26 physical source slots and 66 derived nodes; registering host candidates does not automatically expose them on the
device. `core.py`, `diagnostics.py` and `transport_lab.py` are retained older
simulator/lab paths. Native Mode03 selected-ECU reading is a separate bounded path;
coding/adaptation/clear are not enabled.

Safety boundaries: default live UDS uses no explicit session change; extended
session/keepalive require opt-in. Exact identity is rechecked on reconnect.
Unsupported/candidate signals are not promoted by replay. Web configuration
changes device settings only and has no diagnostic write endpoints. Driving
gates now receive native speed/context; physical correctness still needs validation.

Details: [derived-engine design](../research/derived-engine-design-2026-10-04.md),
[complete metrics and verification](../research/engine-analytics-delivery-2026-10-04.md),
[knowledge](vehicle-knowledge.md), [end-to-end runtime](../runtime/end-to-end.md),
[embedded UI](../ui/architecture.md), [configuration](../ui/configuration.md),
[targets and shared core](targets.md), [replay](../runtime/can-replay.md).
