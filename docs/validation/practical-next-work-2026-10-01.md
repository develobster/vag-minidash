# MiniDash: текущее состояние и практический план — 2026-10-01

## Снимок проекта

- Аудируемая копия: `C:/Users/user/Documents/Codex/work/minidash-ux-touch`.
- Текущая ветка и remote совпадают: `feature/real-car-validation`, HEAD
  `cef49fe1bfa43657dc758ea86c3b5952c2efa170`; рабочее дерево чистое.
- Ветка основана на опубликованной consolidation `3e5c6ca` и находится на 42
  коммита впереди `origin/main`, без отставания. `origin/main` — `de7f071`.
  Для этой feature-ветки открытого PR нет; в GitHub Actions пока нет workflow runs.
  Слияния в main не произошло.
- После delivery 30 сентября опубликованы четыре дальнейших изменения: GPIO4
  backlight PWM, presets/CRT startup, поворот экрана и AP настройки, затем Focus
  needles/catalogue. Документы и отдельные device bench результаты сохранены
  для них; обязательный общий acceptance ниже повторён на сегодняшнем HEAD.
- Другие worktrees не затронуты. `C:/Users/user/vag-minidash` остаётся на
  `feature/esp32-display-prototype` с локальными изменениями diagnostic import/
  registry и приватными artifact каталогами. Они не входят в HEAD настоящей
  feature-ветки; сохранить и рассматривать отдельно. `feature/production-ui`
  worktree чистый. Перенесённые при sync пользовательские изменения лежат в
  сохранённом stash `Preserve local published consolidation files before sync
  2026-09-30`, с дополнительной копией/patch в task outputs.
- README/status docs актуальны на branch tip; этот план — конкретное продолжение
  [real-car preparation](real-car-preparation-2026-09-30.md), а не новый audit
  или замена историческим исследовательским заметкам.

## Что уже работает и чего пока нет

1. **Экранный продукт**: ESP32-S3/ST7789/XPT2046, touch calibration/NVS,
   английский/русский, Settings, layouts, четыре Custom pages, browser editor,
   themes/CRT, 0/90/180/270 rotation, manual GPIO4 PWM backlight и Focus needle
   по значению. Репозитории содержат отдельные UI device/AP tests и сохранение
   настроек. Focus auto catalogue и serial display profile работают через
   существующий host-to-USB bridge. Значения шкал/reference не доказывают
   конкретный кластер owner's машины.
2. **Host runtime**: Transport→configured discovery→observed identities→VAG
   knowledge/registry/planner/scheduler→passive CAN/UDS→SignalStore→arbitration→
   CLI/display bridge; read-only UDS22 и CAN replay. Новые first-session trace,
   fail-fast STOP и UDS replay проходят synthetic offline tests.
3. **Физический ПК↔автомобильный транспорт**: BLOCKED. Инвентаризация этого
   Windows-ПК нашла D-PDU VW OS/OE provider и VAS6154A descriptors. DLL имеют
   `PDU*`, но не требуемые MiniDash J2534 `PassThru*` exports; зарегистрированного
   PassThru provider и подключённого USB PnP VNCI при проверке не наблюдали.
   D-PDU API принадлежит другому ABI, его адаптера в проекте нет. MDF resource
   list не доказывает соединение с устройством, CAN channel или listen-only.
   Текущий Windows capture через VNCI отсутствует.
4. **Engine endpoint**: ODIS 17.0.1 evidence уже поддерживает route
   `7E0 → 7E8`, 500 kbit/s, normal 11-bit на данном Engine/Tiguan. Это не
   доказывает MiniDash transport handshake и не задаёт DQ500 route.
5. **Vehicle knowledge**: 134 provider definitions (53 CAN / 81 UDS) — это
   каталог с provenance, а не 134 проверенных телеметрических сигнала. F405
   coolant — exact-identity raw/decode evidence; F433 ambient pressure, 16F3 RPM,
   F40D speed в ECU evidence пока без raw decoder, поэтому остаются raw-only.
   Throttle position/accelerator и Engine supply voltage с подтверждёнными
   profile candidates не обнаружены. DTC live read и MCU CAN acquisition не
   реализованы; clear/coding/adaptation и другие write paths запрещены.

## Acceptance на HEAD 2026-10-01

Повторно запущено после четырёх feature commits:

- `tools/verify_repository.py ... --output-dir .local/acceptance/current-2026-10-01`:
  Python **191 tests run, 190 passed, 1 Windows AF_CAN skip**; native build PASS; CTest 4/4 PASS (`portable-core`,
  `configuration-json`, `backlight-driver-contract`, `instrument-profile-contract`);
  web logic PASS; summary `hardware_validation:false`, `vehicle_validation:false`.
- ESP-IDF 5.5.5 `idf.py -B build-ui-platform-managed -DCCACHE_ENABLE=0
  reconfigure build`: **PASS**; app `1,892,976` bytes, SHA-256
  `36B6B654890F9EEA977AA733E096CB03039CEAB3B94671C937DED32A36BD7553`, ~40%
  free in the 3 MiB app partition. Log:
  `firmware/.local/current-state-build-2026-10-01.log`. Build only; no flash.
- `tools/real_car_preflight.py --config
  config/examples/tiguan-vnci-engine-read-only.template.json`: BLOCKED/exit1,
  что ожидаемо для незаполненного J2534 DLL и отсутствующих layout identity
  probes. Скрипт не грузит DLL и не открывает VCI.
- Свежий GitHub API query: открытого/закрытого PR для
  `feature/real-car-validation` не найдено. `gh` CLI на данном ПК не установлен;
  PR статус проверен через GitHub API.
- Прошивка не загружалась в ESP32, конфигурация/NVS не менялась; CAN/UDS на
  машине не выполнялся.

## Практический план работ

### P0 — физически принять текущий экранный build на столе

**Зачем:** native/LVGL проверки гарантируют геометрию, но не читаемость и работу
XPT2046 на установленном дисплее. В Focus report последняя открытая проверка —
физическая читаемость шкал.

1. Сохранить локальный `config-export` и сфотографировать исходные Theme,
   Dashboard, page/layout, brightness, language и rotation. Проверить резервные
   копии NVS/calibration до обновления.
2. После успешной IDF build сверить app hash/размер и записать только app
   partition на плату; bootloader/partition/NVS/calibration не стирать. Затем
   reboot, проверить `config-export` и `ui-test`.
3. На 320×240 экране проверить русский, Settings→More page flow, Theme,
   Day/Night, AP on/off и сохранение after reboot. На 0/90/180/270 проверить
   углы touch, Prev/Next, Back/Home/Select, все настройки и сохранение
   calibration. В каждой ориентации проверить Focus RPM/speed/coolant в
   Volkswagen/Porsche theme и numeric value/unit/needle/stale indicator.
   Посмотреть viewport близко к 10% brightness и штатным 80%; обычные
   Dashboard/Performance/Custom cards должны оставаться без needle.
4. В Focus записать фото/видео каждой ориентации и результат таблицей
   «экран/действие/ожидание/факт». Настройки после теста восстановить и
   сравнить с исходным export. Не включать реальный источник данных; известные
   synthetic bridge frames помечать synthetic.

**PASS:** читаемость и hit targets подтверждены человеком; rotation/touch
совпадают; Focus правильно скрывает needle для stale/invalid и сохраняет zero;
резервная конфигурация совпадает после reboot. **STOP/rollback:** панель blank,
touch перевёрнут/смещён, UI stack/panic, NVS/calibration повреждены — выключить,
не стирать NVS, вернуть app-only ранее проверенного firmware.

### P0 — проверить электрическую нагрузку подсветки

Firmware использует GPIO4 LEDC 5 kHz/12 bit; программный/визуальный тест ранее
проходил. В актуальной hardware заметке не измерены ток нагрузки на GPIO,
форма PWM и температура. До долгого/автомобильного питания отдельно измерить
плату по её документации и сверить нагрузку со спецификациями модуля/SoC и
контроллера TFT. Подтвердить, что контакт LED является входом управления или
безопасно ограниченной нагрузкой, а не силовым питанием LED. Если силовая
цепь/ток не соответствует допустимым параметрам GPIO — использовать подходящий
MOSFET/транзисторный драйвер с рассчитанными gate/base resistor, pull state и
общей землёй; выбирать тип полярности после измерений. Проверить startup,
sleep/wake, reset/heat и осциллографом PWM. Не соединять LED одновременно с
3V3 и GPIO. Auto brightness пока отключён: датчик не подключён.

**PASS:** на документации платы/измерениях установлен тип входа и ток, cold
boot не мигает, PWM устойчив, сон выключает экранную подсветку, GPIO/драйвер не
перегревается и перезагрузок нет. **Зависимости:** схема/маркировка платы,
мультиметр, при необходимости осциллограф; без безопасной схемы продолжить
ручной программный тест допустимо только под наблюдением на столе.

### P1 — установить реальный software API VNCI, не подключаясь к ECU

**Действие на существующем Windows-ПК:** сохранить локальные outputs и запустить
ODIS/VNCI Device Manager с подключённым к ПК интерфейсом; собрать свежий
`.local/real-car/driver-check/vnci-windows.json` командой
`tools/Inspect-Vnci.ps1`. В отчёт оставить registry32/64, загруженные process
modules, provider path/version, device PnP/model/firmware и vendor MVCI root/MDF/
CDF references. Не прикладывать приватный отчёт в public issue; отдельно
предъявить ODIS exit/close и освободить VCI перед MiniDash. Сегодняшний снимок
был без загруженных ODIS modules и не обнаружил подключённое USB PnP VNCI.

- Если подтверждена разрешённая D-PDU SDK: записать документированные ABI,
  ownership, module/resource IDs, protocol/baud/ComParams, event/cancel/timeout
  sequence, dynamic library bitness, concurrency/session locks и supported RAWCAN
  receive direction; проверить license/redistribution scope.
- Создать узкий adapter под существующий Transport, только для доказанного
  ресурса. Сначала enumerates/connects/disconnects на столе без диагностического
  запроса; затем VirtualBus/replay tests NRC/timeout/multiframe/cleanup. Не
  вызывать неизвестные DLL exports вручную и не подменять D-PDU через generic
  J2534 ctypes ABI.
- Отдельно доказать пассивный RAWCAN capture и отсутствие transmit на VCI; D-PDU
  UDS ISO-TP flow control не считать пассивным слушателем. Если доступного
  verified receive-only ресурса нет — использовать отдельный CAN recorder или
  экспорт ODIS после реализации проверенного конвертера; не смешивать его с
  автомобильной записью.

**PASS:** output перечисляет именно подключённый VNCI, library bitness/provider
соответствуют интерфейсу, safe open/close повторяется, канал/скорость выбираются
из доказанной конфигурации, transmit/read разделены и пассивный режим проверен.
Только после этого preflight может разрешить контролируемую попытку соединения.

### P1 — подготовить ECU identity layouts и телеметрический host route

1. Из ODIS Engineering 17.0.1 raw/log capture извлечь фактические engine identity
   read transactions и encoding: SW `5NA907115D/0003`, HW `06L907309B/H31`, ASAM
   `EV_ECM20TFS0115NA907115D/001004`. Не назначать F187/F189/другие стандартные
   DID по предположению; source/log version и target semantics сохранить.
2. Добавить exact layout в локальный private config только после evidence; VIN
   F190 записывать в `.local`, не в fixture. Discovery обязана наблюдать
   expected values на конкретном ECU, иначе STOP до F405. Повторить discovery
   на том же route 7E0/7E8, явно зафиксировать физический bus/channel и transport.
3. Начать на стоящей машине, ignition ON, engine OFF; один F405 read + replay;
   сравнить с ODIS same-stage/reference. Затем F433,16F3,F40D по одному/малому
   ограниченному набору на OFF/idle/RPM markers. Для F433/RPM/speed сохранять
   raw bytes и reference; ничего не отображать как физическую единицу до
   подтверждения формулы. Дополнительные DIDs, throttle/pedal/voltage,
   transmission откладываются до отдельного catalogue evidence.
4. Через существующий host-to-USB bridge проверить Focus freshness/stale/expiry,
   value arbitration и profile expiry. Профиль VW/Tiguan scale пока не равен
   подтверждённому заводскому кластеру 2017 конкретной комплектации. Screen
   должен явно показывать fallback/manual, если распознан только двигатель.

**STOP:** другой ECU fingerprint, неизвестный DID, любой неожиданный NRC кроме
bounded `0x78` pending, timeout, неверный echo, повторные ошибки, необходимость
session/security/write. Никаких test drives для снятия базовых identity.
Ожидаемый capture: private ordered UDS JSONL + discovery + endpoint/provenance +
replay + reference annotations; `validated` только ручным identity-scoped review.

### P2 — passive traffic / controls correlation

После отдельного silent adapter acceptance снять stationary CAN: ignition on,
engine off → engine start/idle → фиксированные обороты → по одной педаль/кнопка
baseline/press/release. Markers в sidecar той же monotonic clock. На steering,
stalk, NEXT/PREV/OK/BACK снять несколько повторов; отчёт byte/time deltas — только
candidate. Повторить на отдельном CAN сегменте, если OBD diagnostic connector не
видит кузовной bus. Ни одного CAN ID не связывать с UI до bus/source/behavior
cross-check; ожидаемый MVP пока может остаться touch-only.

**PASS:** recorder proven non-transmit, complete trace replay/DBC counter checks,
повторяемая корреляция с ручными markers на правильном bus. Сохранить сырые
файлы приватно; для коммита — только вручную sanitized synthetic fixtures.

### P2/P3 — расширение продукта после telemetry acceptance

1. Реализовать live Engine DTC read `0x19` отдельным read-only milestone: subfunction
   / status mask / response layouts по evidence, bounded response/count, VIN privacy,
   per-ECU trace/replay, malformed/NRC/timeout tests. Сейчас production `0x19`
   отсутствует. Не добавлять clear `0x14` в этот milestone.
2. Стабилизировать host pipeline и device display bridge в длинном parked run:
   cadence/load/dropout/p95 latency/store TTL/thermal/heap/reconnect; отобразить
   quality/stale и recorder/safety state. Никаких поездок, пока надёжность данных
   и пассивное поведение не измерены.
3. Только после host gate проектировать ESP32 автономный CAN transport: доказать
   установленный transceiver/connector/power protection/pins и silent mode,
   отделить MCU acquisition/identity/provider от UI, ограничить read schedule,
   watchdog и privacy-safe local logging. Сегодня MCU CAN disabled — PC данные
   по USB не означают встроенный автомобильный adapter.
4. DTC clear, reset, security, routines, actuators, coding/adaptation и basic
   settings — отдельные более поздние safety milestones с явно ограниченным
   whitelist/recovery/owner approval и реальным evidence. Эта feature не создаёт
   разрешения на такие запросы.

## Следующие действия по порядку

1. Завершить свежую прошивочную build, сохранить build hash/partition report.
2. Выполнить P0 столовую visual/touch проверку нового Focus/theme/rotation на
   плате и приложить фотографии + config before/after.
3. Разобраться с PWM electrical measurement/drive topology до длительной работы.
4. Получить свежий локальный VNCI process/module inventory с работающим
   Device Manager и решить: D-PDU adapter или доказанный J2534 provider.
5. Подтвердить silent CAN path, затем собрать identity DID layouts и начать
   first stationary read-only trace.
6. После проверенного Engine path оценить DTC read; drivetrain, автономный MCU
   и write features не делать параллельно.

Owner inputs нужны для физической проверки экрана/проводки и локального запуска
vendor-программ; software preparation, native/test runs, документация и анализ
следующих приватных отчётов можно выполнить в existing feature branch.
