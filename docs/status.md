# Текущие возможности и доказательства

19 октября: [Pixel Pack](ui/pixel-pack-2026-10-19.md) добавляет три opt-in
пиксельные темы к общему UI. Прежние темы сохранены; CYD и S3 собираются.
Процедурные переходы не требуют второго framebuffer. Физическая визуальная
приёмка новых цветов/шрифта отмечается отдельно от программных проверок.

Дополнительная платформа **MiniDash CYD**, 18–19 октября: отдельный
`minidash_cyd` target для ESP32-2432S028R, без замены S3.
[Профиль и восстановление](hardware/minidash-cyd.md),
[проверки и ограничения](hardware/minidash-cyd-2026-10-18.md).
CYD работает с DEMO-источником; CAN на этой платформе запрещён до подтверждения
схемы. Доказательства CAN ниже относятся к исходной S3-платформе.

18 октября, проверка на USB после измерений: отдельный стендовый образ
передал и принял 20/20 кадров через внешний CAN-модуль, без ошибок.
Исходный canerr18 восстановлен с проверкой digest и первых 64 КиБ;
стендовый режим в кэше выключен. Кабель до автомобиля и работа под нагрузкой
этим не проверены. [Подробности стенда и ограничения](hardware/can-usb-loopback-2026-10-18.md).

Последующее обновление 2026-10-18: `canerr18` добавляет на экран «АРБ / ШИНА»
и в GET-экспорт счётчики арбитража, ошибок шины и TEC/REC. CTX→GPIO15
прозвонен пользователем. Сборка, восемь CTest-наборов, USB-запись с сохранением
первых 64 КиБ проверены. Автомобильная приёмка новых счётчиков ожидается.
См. [диагностику передачи CAN](hardware/can-transmit-diagnostics-2026-10-18.md).

Обновление 2026-10-18: установлен `wifi1018`, пароль Wi-Fi `12345678`,
добавлены этапы запуска AP, ошибки TX на экране и ограниченные повторы поиска
ECU. USB-запуск и восемь CTest-наборов проверены; настройки сохранены.
Предыдущий автомобильный тест `obd-live-20261017` не получил ответов ECU,
несмотря на приём CAN. Корневая HTTP-страница открылась с телефона;
живые автомобильные значения пока не приняты.
См. [факты, изменения и дальнейшую приёмку](hardware/wifi-obd-2026-10-18.md).

Обновление 2026-10-14: единственная база дальнейшей разработки —
**`integration/standalone`**. В неё обычным слиянием включены `0222a01` и
`da1e6b7`: крупный статус автомобиля вынесен из стека диагностики, проверка
бюджета стека встроена в сборку, добавлены UART-профиль и ручная инструкция.
Сохранены конфигуратор/настройки ПК, русский интерфейс всех 92 сигналов,
режим проверки автомобиля и экспорт, сбор/аналитика и миграции настроек.
Результаты новых проверок — в [отчёте интеграции](development/standalone-realignment-2026-10-14.md).
После отдельной задачи интеграции чистый объединённый образ **`875106b`**
записан на ESP32-S3 через COM3 в пассивном режиме Car Validation + UART.
27 проверок UI прошли; настройки и калибровка сохранены, в том числе после
программной перезагрузки. За 75,1 с UART-наблюдения повторных стартов/паники
не обнаружено. Пользователь подтвердил изображение и работу тача.
CANH/CANL отключены, питание только USB. Холодное отключение питания и
автомобиль ещё не проверены. Подробности —
[стендовая загрузка 14 октября](hardware/standalone-flash-2026-10-14.md).

Стендовые результаты 13 октября относятся к отдельной версии: пользователь
подтвердил изображение/тач, UART работал 70 секунд с согласованным загрузчиком,
CANH/CANL были отключены. Это не приёмка объединённого образа или автомобиля.
См. [историческую приёмку](hardware/can-display-acceptance-2026-10-13.md) и
[актуальную ручную инструкцию](hardware/manual-flashing-ru.md).

## Предыдущие этапы

База разработки (2026-10-05): **`integration/standalone`**, все серверные истории
и восстановленный локальный мобильный конфигуратор/настройки.
Режим проверки автомобиля и GET-экспорт реализованы; фирменные источники
выключены. См. [консолидацию](development/standalone-consolidation-2026-10-05.md)
и [автономную проверку автомобиля](validation/car-validation-mode.md).
Старые разделы сохраняют даты/контекст. VNCI ограничивает необязательные
исследования на ПК и не требуется для первого автономного теста.

Русские подписи всех 92 показателей, диагностика и локальные веб-настройки
дополнены. Пассивный образ `e4bb06c` загружен на USB-стенде: 27 проверок
интерфейса, перезагрузка и сохранность настроек/калибровки подтверждены.
Переполнение стека интерфейса, обнаруженное на первой загрузке, исправлено.
Автомобиль не подключался. Результаты и ограничения —
в [отдельном отчёте](development/russian-ui-flash-2026-10-05.md).

Previous continuation (2026-10-04): shared native CAN/OBD → Analytics integration and freshness
rescue scheduling; seven native groups. See [pipeline follow-up](research/analytics-pipeline-scheduling-2026-10-04.md).

Updated 2026-10-04: `feature/engine-analytics`, integrated standalone + newer
real-car/UI work on merge `6ae2ed4`. 92 canonical IDs / 66 derived nodes, six
presets, fixed windows/events/performance timers, schema 4 migration. Native,
replay and three ESP32 build modes PASS; manufacturer sources remain disabled.
No new physical-board/car acceptance. See the [analytics delivery](research/engine-analytics-delivery-2026-10-04.md),
[verification](research/engine-analytics-verification-2026-10-04.json), and
[exact read-only evidence plan](validation/engine-analytics-car-evidence-2026-10-04.md).
See [current report](research/standalone-final-2026-10-03.md) and
[first autonomous car test](../REAL_CAR_TEST.md). Historical UI hardware results
come from `6f436dd`; no new device/car test is claimed. Python modules below are
preserved development/research tools, not dependencies of standalone operation.
Current branch/acceptance and actionable next work (2026-10-01):
[project status and phased practical plan](validation/practical-next-work-2026-10-01.md).
The latest UI additions pass the common build gates; physical Focus readability,
backlight-load measurement and real VNCI transport remain acceptance blockers.

Focus update (2026-10-01): Volkswagen/Porsche now show a value-driven needle
only in single-signal Focus; ordinary cards have no instrument arcs. Five
reference vehicle profiles, Tiguan fallback and Auto/manual ranges use schema 3.
Automatic matching is connected through host discovery → USB, not autonomous
MCU CAN acquisition. See [implementation, sources and acceptance](ui/focus-instruments-2026-10-01.md).

Wi-Fi/theme update (2026-10-01): local settings remain editable with AP enabled;
Wi-Fi shows an ON/OFF indicator. Eleven themes now include Volkswagen/Porsche,
monospaced retro fonts and distinct frames/instrument artwork. See
[implementation, tests and remaining checks](ui/wifi-and-instrument-themes-2026-10-01.md).

Rotation update (2026-09-30): Settings supports 0/90/180/270 degrees, with
portrait card/grid reflow and transformed calibrated touch. Configuration schema
2 migrates old build-only orientation to relative 0 without changing common
settings. See [implementation, migration and verification](ui/rotation-and-portrait-2026-09-30.md).

Theme/startup update (2026-09-30): nine presets including Terminal, Matrix,
Fallout, Amber CRT, Cyberpunk and Arctic; boot-only CRT reveal with autonomous
LEDC fade (~2 s) to saved brightness; new-settings default is now 70%.
See [implementation and acceptance](ui/themes-and-crt-startup-2026-09-30.md).

Current backlight update (2026-09-30): GPIO4 is owner-confirmed and enabled for
LEDC PWM (5 kHz, 12 bit, gamma 2.0), using the existing Brightness setting and
NVS. Manual Settings control, 10% readable minimum, reboot persistence and cold
USB boot have passed physical bench acceptance. Prior FIXED/3V3 descriptions
below record the preceding wiring. See [backlight implementation and acceptance](hardware/backlight-pwm-2026-09-30.md).


Audited 2026-09-30 against integration base `22a346b`. See the
[consolidation report](research/repository-consolidation-2026-09-30.md) for fresh
acceptance results and actual delivery state. Historical hardware results below
come from the UI delivery `6f436dd`; no new device/car test is claimed.

The worktree subsequently synced cleanly to consolidation `3e5c6ca`; see the
[real-car preparation report](validation/real-car-preparation-2026-09-30.md)
for new strict recording/replay tools, tests and driver inspection. The current
[first-session runbook](validation/first-real-car-session.md) reports **BLOCKED**:
VW D-PDU x64 DLLs are installed, J2534 FunctionLibrary is not evidenced, Windows
VNCI passive capture and exact identity probe layouts remain unavailable.

Labels are independent: **IMPLEMENTED** describes code; **OFFLINE TESTED** is a
synthetic/native contract; **HARDWARE TESTED** is a recorded bench delivery;
**REAL-CAR VALIDATED** requires exact vehicle/ECU/source evidence. **PARTIAL**,
**RESEARCH ONLY**, **NOT IMPLEMENTED** identify incomplete product paths.
Registry confidence metadata alone does not grant a validation label.

| Component | Status / evidence | Code and tests | Limits |
|---|---|---|---|
| Display | IMPLEMENTED; HARDWARE TESTED for ST7789 backend | `firmware/main/st7789_backend.c`, `display_manager.c`, `ui_screens.c`; UI delivery record | 320x240 baseline with 0/90/180/270 user rotation; LCD marking unconfirmed; ILI9341 backend exists but is not the validated current panel |
| XPT2046 touch | IMPLEMENTED; HARDWARE TESTED | `xpt2046_touch.c`, `ui_touch_map.c`, `ui_touch_nvs.c`; `core/tests.c`, device UI record | Five-point affine/NVS; calibration independent of UI config; physical edge/readability checks remain |
| Multiple screens | IMPLEMENTED; OFFLINE/HARDWARE TESTED UI | `ui_core.c`, `ui_screens.c`, `acquisition.c` | Native OBD source added; acquisition not car-validated |
| Layouts / four Custom pages | IMPLEMENTED; OFFLINE TESTED; prior device contracts | `ui_configuration.c`, `ui_model.c`, `ui_screens.c`; `core/tests.c`, `core/test_json.c` | 1/2/4/6/8 and mixed layouts; dense readability not physically accepted |
| Themes / day-night / localization | IMPLEMENTED; OFFLINE/HARDWARE TESTED contracts | `ui_theme.c`, `ui_i18n.c`, `ui_font_ru_*.c`; native/device glyph checks | English/Russian; physical labels and dim/night choices require visual acceptance |
| Theme editor | IMPLEMENTED; OFFLINE TESTED; prior AP HTTP contracts | `webui.html`, `ui_configuration_json.c`; `core/test_webui.cjs`, `ui_web.c` self-tests | Browser/phone rendering and sustained FPS not measured |
| Screen navigation / on-screen controls | IMPLEMENTED; OFFLINE/HARDWARE TESTED | `ui_core.c`, `ui_screens.c`, `display_app.c`; native/device self-tests | Semantic actions; Settings select/CHANGE separated; all dashboard card pickers |
| GPIO buttons / steering-stalk input | PARTIAL; OFFLINE TESTED abstraction | `ui_platform.c`, `ui_platform_selftest.c` | Debounce/long press/repeat/lockout model; no wired GPIO adapter or evidence-backed CAN/LIN mapping |
| BLE / phone app | PARTIAL bring-up; NOT IMPLEMENTED product app | `firmware/main/main.c` bring-up | ESP32-S3 BLE, no Classic SPP or product configuration protocol |
| Wi-Fi | IMPLEMENTED; prior HARDWARE TESTED AP contracts | `ui_web.c`, `main.c`; prior four AP cycles | Explicit temporary AP; no always-on remote service |
| Web UI | IMPLEMENTED; OFFLINE TESTED; prior HARDWARE TESTED HTTP | `ui_web.c`, `webui.html`; native JSON/Node tests | WPA2/session/revision/bounds; native speed gate still needs physical validation; local HTTP plaintext |
| Device settings | IMPLEMENTED; OFFLINE/HARDWARE TESTED persistence | `ui_configuration*.c`, `ui_settings_nvs.c`; JSON/native/device tests | Schema 2 CRC, legacy migration, separate touch key; four display orientations |
| Backlight/manual PWM | IMPLEMENTED; OWNER HARDWARE TESTED | `backlight_controller.c`, `display_manager.c`; backlight delivery report | GPIO4 LEDC 5 kHz/12 bit, 10–100%, sleep-off, saved boot fade; AUTO has no ambient source |
| Passive CAN acquisition | IMPLEMENTED host and gated native core; OFFLINE TESTED | `acquisition.c`, `passive.py`, `dbc.py`; core/MQB tests | No enabled native profile; OBD visibility/checksum/counters and hardware listen-only unverified |
| UDS polling | IMPLEMENTED host and gated native core; OFFLINE TESTED | `acquisition.c`, `uds.py`, `runtime.py` | No enabled native manufacturer profile; exact VIN/identity/endpoint/evidence required |
| Transport / J2534 / VNCI | PARTIAL; OFFLINE TESTED ABI/lifecycle | `transports.py`, `j2534.py`; `test_live.py`, `test_transport.py` | Driver inventory/evidenced routes exist; MiniDash physical VCI route not proven |
| ISO-TP | IMPLEMENTED host and native; OFFLINE TESTED | `diagnostic_transport.c`, `acquisition.c`, core tests/VirtualBus | Native RX128 bytes, single-frame requests only; header/full-length validation before FC |
| Native OBD discovery | IMPLEMENTED; OFFLINE TESTED | `acquisition.c`, `vehicle_app.c` | Explicit 11-bit/500-kbit preset; PID capabilities/VIN, not a VAG full-ECU scan |
| ECU discovery | IMPLEMENTED configured endpoints; OFFLINE TESTED | `discovery.py`, `__main__.py`; `test_pipeline.py`, `test_catalog.py` | Not blind gateway/network scanning; identity reads do not validate signal candidates |
| VehicleIdentity | IMPLEMENTED; OFFLINE TESTED | `identity.py`, `discovery.py`; vehicle-knowledge/pipeline/evidence tests | Snapshot provenance/connection point must be recorded physically |
| EcuIdentity / IdentityContext | IMPLEMENTED; OFFLINE TESTED | `identity.py`, `knowledge.py`, `discovery.py`; evidence/knowledge/pipeline tests | Exact fingerprints gate codecs and reconnect checks |
| Knowledge providers / provenance | IMPLEMENTED; OFFLINE TESTED | `knowledge.py`, `vag.py`, `registry.py`; `test_vehicle_knowledge.py` | Platform-agnostic core; canonical/raw mapping explicit; license/evidence retained |
| MQB / VAG knowledge | PARTIAL; OFFLINE TESTED; candidates require car evidence | `data/external/`, `vag.py`; `test_mqb_knowledge.py` | 134 definitions (53 CAN/81 UDS) are not 134 validated telemetry signals |
| Exact Engine coolant F405 | REAL-CAR VALIDATED codec for documented exact identity | `data/ecu_profiles/j623-czpa-5na907115d.json`; `test_real_vehicle_evidence.py` | ODIS/physical trace evidence, not proof of MiniDash J2534 live path; no other ECU/platform extrapolation |
| Signal registry | IMPLEMENTED; OFFLINE TESTED | `registry.py`, `profiles.py`; registry/scalar/knowledge tests | Conflict/schema/codec validation; candidate labels remain candidates |
| MCD import | PARTIAL; OFFLINE TESTED FLOAT32/TEXTTABLE | `mcd_import.py`, `profiles.py`; `test_scalar_codecs.py`, typed fixture | Single byte-aligned leaf only; bounded enum/no defaults, no multi-field/mux/general formulas |
| Acquisition planner / scheduler | IMPLEMENTED host/native; OFFLINE TESTED | `acquisition.c`, `pipeline.py`, `runtime.py` | Native visible/background priority, one active request, bounds; no measured physical bus-load budget |
| SignalStore | IMPLEMENTED; OFFLINE TESTED | `runtime.py`; runtime/registry/pipeline/replay tests | Quality/TTL and source provenance; host store distinct from embedded presentation store |
| Derived Metrics / events / performance | IMPLEMENTED; OFFLINE TESTED | `derived_engine`, `window_statistics`, `performance_timer`; 733 checks, 12 C/Python replays | 66 derived nodes; no manufacturer polling without evidence, user thresholds off by default |
| SourceArbitrator | IMPLEMENTED; OFFLINE TESTED | `arbitration.py`, `pipeline.py`; knowledge/pipeline/replay tests | Offline replay does not promote evidence or publish a live source |
| Replay | IMPLEMENTED; OFFLINE TESTED | `replay.py`, `CanTraceReader`, CLI; `test_can_replay.py`, `test_real_car_validation.py` | CAN uses existing decoder/store/arbitration; recorded UDS also uses production discovery/planner/scheduler with offline Transport; no DLL/bus I/O; private complete trace required |
| DTC read | IMPLEMENTED native Mode03; OFFLINE TESTED | `acquisition.c`, `display_app.c`, core/VirtualBus tests | Selected emissions ECU, fresh speed=0, max12/truncation; no descriptions/full inventory/0x19 |
| DTC clear | NOT IMPLEMENTED live product | Simulator paths only | No 0x14 production workflow/confirmation/vehicle guard acceptance |
| Coding/adaptation | RESEARCH ONLY / simulator groundwork | `core.py`, legacy profiles/journal tests | No live allowlist/write service; no Web endpoint |
| CLI / doctor / trace | IMPLEMENTED; OFFLINE TESTED | `__main__.py`, `discovery.py`, `api.py`; live/preflight/probe/pipeline tests | Doctor does not open VCI; discover/signals/watch physical acceptance still needed |
| Logging | IMPLEMENTED host; PARTIAL native RAM trace | `acquisition.c`, `md_vehicle_app_trace`, host recorder | Ring96 frames, C snapshot API; no HTTP export/persistent trip logger/SD backend |
| Real-car evidence workflow | IMPLEMENTED documents/manifest; PARTIAL execution | `docs/runtime/tiguan-mqb-*`, identity/provenance code; evidence tests | New MQB candidates, timing corrections, car buttons and illumination need synchronized physical captures |
| ESP32 firmware | IMPLEMENTED autonomous OBD/UI; OFFLINE TESTED | `firmware/main/`, core/acquisition harness | Native car path not physically tested; default CAN off, explicit presets; QUIET is not power-off |
| Display abstraction / Lite | PARTIAL; OFFLINE TESTED shared core | `ui_platform.c`, `ui_model.c`, `core/CMakeLists.txt`; portable tests | No STM32 linked firmware, OLED driver/board/pins or complete RAM/flash fit evidence |

## Priorities

**P0:** protected power/transceiver bench, physical UI and autonomous read-only
Tiguan acceptance. Firmware build recovery now has a locked-component procedure;
see the verification JSON for actual build results. No PC required at runtime.

**P1:** measured bus budget/heap/FPS, exact profile evidence/compiler, passive and
steering visibility, native ECU-scoped read-only diagnostics. DTC clear requires a
separate reviewed guard/confirmation workflow; it is not part of this release.

**P2:** controlled adaptation/coding allowlists and recovery, verified car-button
adapter, AUTO illumination source/PWM hardware, optional BLE app/OTA authenticity,
confirmed Lite target. Hardware capability bits alone do not implement these paths.
