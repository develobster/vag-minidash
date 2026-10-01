# MiniDash Web Configurator — реализация на host

Существующий `minidash_live.api` теперь обслуживает мобильный конфигуратор вместо
страницы с JSON. Сохранён `/api/signals` для прежних клиентов и `probe --serve`.
Tk-приложение, diagnostic profiles, CAN/UDS acquisition и дисплейная прошивка сохранены.
Frontend — статические HTML/CSS/ES modules, без runtime npm-зависимостей,
CDN и стороннего frontend-сервера. Python 3.10+ достаточно для симуляции.

## Что реализовано

Пять страниц, нижняя навигация, крупные кнопки, safe areas, светлая/тёмная/системная
тема приложения и отдельные темы физического display preview:

1. **Dashboard:** соединение Web API, CAN/vehicle state, ECU count, host uptime,
   активный экран, live values, запись и быстрые переходы.
2. **Screens:** несколько экранов; presets 1/2/3/4/6/8 и произвольная сетка до
   24 виджетов; tap → bottom sheet; сдвиг по свободным клеткам, размер, duplicate,
   delete; signal, label, units, visualization, range, warning/critical и decimals.
   Numeric, gauge, bar, compact gauge, mini graph. Preview учитывает размеры дисплея.
3. **Signals:** canonical catalog существующего VAG knowledge provider, поиск,
   source filter, значения, units, качество/availability, age, ожидаемая частота,
   известный диапазон и Add to screen с выбором экрана. Добавление идёт в первую
   свободную клетку; точная позиция меняется в редакторе.
4. **Logs:** реальная запись JSONL на host, start/stop, duration/samples/bytes/
   configured signals, список metadata, download, подтверждаемое delete и график.
5. **Device:** информация о host/runtime и ECU states, advanced metrics,
   appearance/vehicle preference/display size, configuration backup и явный статус OTA.

Темы OEM Dark / Sport / Minimal / Night / High Contrast; произвольные новые темы
через duplicate/import. Редактируются имя, пять цветов, gauge shape, typography,
spacing и grid. Есть reset и export/import.

Конфигурация изменяется как черновик. Нажатие Save атомарно сохраняет её на host.
Несохранённые изменения остаются в localStorage телефона; потеря связи их не удаляет.
При конфликте двух клиентов сервер отвечает 409. После изменения host configuration
старый черновик предлагается экспортировать, а не незаметно перезаписывать.
Destructive actions подтверждаются. Errors/success показываются в status toast.

## API и backend

| Endpoint | Назначение |
| --- | --- |
| `GET /api/signals` | Сохранённый прежний raw research API |
| `GET /api/events` | SSE snapshot каждые 500 ms, автоматический reconnect браузера |
| `GET /api/telemetry` | Тот же нормализованный snapshot и capabilities |
| `GET /api/configuration` | `{configuration, revision}` |
| `PUT /api/configuration` | Проверка и атомарное сохранение с expected revision |
| `POST /api/configuration/validate` | Проверка backup без изменения состояния |
| `GET /api/defaults` | Начальная версия конфигурации |
| `GET /api/logs` | Metadata завершённых записей |
| `POST /api/logs/start`, `POST /api/logs/stop` | Host recording |
| `GET /api/logs/{id}`, `DELETE /api/logs/{id}` | JSONL download / delete |
| `POST /api/simulation` | Пауза canonical simulation для проверки stale values |
| `GET /api/ota` | `available: false`, с причиной |

Writes требуют JSON, ограничены 256 KiB и отвергают чужой Origin.
Конфигурация имеет `schema_version: 1`: settings, screens/widgets, themes,
signal_configuration, active_screen и active_theme. Все widget thresholds включены
в backup. Неизвестная версия, overlap/out-of-bounds, неправильные цвета, duplicate IDs,
невалидные ranges/thresholds и нечисловые размеры отвергаются без изменения saved config.
Сохранённая revision переживает перезапуск. До появления версии 2 никакая автоматическая
миграция не производится; неизвестные версии явно отвергаются.

Данные лежат в `.minidash/configuration.json` и `.minidash/logs/`, каталог исключён из Git.
На диске configuration хранится вместе с revision; экспорт содержит portable configuration.
Предел одной записи: один час или 20 MB, затем запись автоматически завершается.
Аварийное выключение процесса до stop может оставить незавершённый JSONL без metadata;
он сохраняется на диске, но пока не попадает в список завершённых записей.

`watch` получил `--web-port`, `--web-host`, `--web-data-dir`. Runtime публикует
выбранные SourceArbitrator значения через `State.update_runtime`; UI не выбирает
между конкурирующими CAN/UDS источниками и не инициирует ECU-запросы.
Редактирование screens/signal_configuration/preferences не меняет acquisition plan:
набор чтения и автомобильный профиль по-прежнему задаёт `watch --config`.

## Что работает end-to-end и в simulation

Без автомобиля работает цепочка:
`SimulationState → runtime.SignalStore → Web API → SSE → Web Configurator`.
Simulation публикует RPM, speed, coolant, oil, charge pressure, ambient pressure,
battery, gear, две wheel speeds и вычисленный относительный boost. Все samples
явно имеют source `simulated`; нет новых VAG CAN IDs/DIDs.
В live runtime boost вычисляется только при валидных charge/ambient в hPa:
`(charge - ambient) / 1000`. Его отображение не подтверждает новый автомобильный decoder.
Остальной canonical catalog имеет unavailable до получения runtime readings.

Настройки/layouts/themes, backup, запись/скачивание/удаление/графики и SSE идут
через реальные host API и файловое хранилище. Disconnected/stale/nonvalid значения
не показываются текущими; mini graphs очищаются при разрыве связи.
Pause simulation позволяет проверить TTL без сетевого disconnect.

PWA имеет manifest, PNG 192/512, SVG icon, standalone и basic offline shell.
Service worker кэширует только оболочку, никогда `/api/*`. Offline launch показывает
недоступность устройства либо сохранённый локальный draft без текущих показаний.
Установка PWA/service worker требуют secure context: localhost или HTTPS.
Обычный HTTP по Wi-Fi работает как Web UI, но не получает PWA-install/offline caching.

## Что ещё требует автомобиля и firmware

Это работающий **host Web Configurator**, не автономный ESP32 Web server.
Открыть его напрямую на Wi-Fi самого MiniDash сейчас нельзя: в существующем firmware
нет AP/provisioning, Web API и OTA. В host preview показан этот предел явно.

Автомобиль требуется для проверки существующих подтверждённых CAN/UDS routes,
актуальности сигналов, ECU availability и reconnect при реальных сбоях адаптера.
VirtualBus и simulation не подтверждают эти свойства на настоящей машине.

Firmware необходимо расширить Wi-Fi/API/SSE, хранением versioned configuration,
проектированием произвольных widgets/themes на физический ST7789/ILI9341,
device metrics/storage/logger и безопасным OTA с проверкой image/rollback.
Существующая прошивка сохраняет свои текущие фиксированные display pages.
Preview пока не синхронизируется с физическим MiniDash, thresholds не отправляются
на ESP32, device preferences не меняют firmware. Flash/PSRAM/heap/RSSI/RX/TX/errors
не выдаются за реальные показатели, OTA явно unavailable.
ESP-IDF build не запускался: firmware в этой задаче не менялась.

## Запуск на Windows и телефон

Из `C:\Users\user\vag-minidash`:

```powershell
python -m minidash_live.configurator
# http://127.0.0.1:8765 — simulation, без автомобиля
```

Для телефона в одной Wi-Fi сети с ПК:

```powershell
.\tools\Run-WebConfigurator.ps1 -Lan
# либо:
python -m minidash_live.configurator --host 0.0.0.0 --port 8765
```

Launcher выводит адреса интерфейсов ПК. Откройте на телефоне `http://<IPv4-ПК>:8765`.
На проверенном ПК адрес интерфейса в момент реализации: `192.168.3.18`;
он может измениться при переподключении. Если Windows показывает запрос Firewall,
разрешите Python для используемой частной сети. На телефоне localhost означает сам телефон.
Остановка — Ctrl+C. Другой каталог настроек: `--data-dir <directory>`.

С существующим runtime (пример формы команды; не подтверждение подключения машины):

```powershell
python -m minidash_live watch engine.coolant_temperature --config <runtime-config.json> --web-port 8765 --web-host 0.0.0.0 --duration 3600
```

Используйте ранее проверенный транспортный профиль проекта, не simulation config.
Профиль-заготовка с непроверенными значениями не становится пригодным к машине от
добавления `--web-port`. Сервер живёт столько же, сколько команда `watch`.

## Проверки и build

Frontend tests и production asset packaging:

```powershell
pnpm install --frozen-lockfile
pnpm test
pnpm test:ui
pnpm build
```

На этом ПК `npm` отсутствует в PATH; использован bundled `pnpm.cmd`.
Эквивалент уже установленными зависимостями:

```powershell
node --test tests/frontend/model.test.js
node node_modules/@playwright/test/cli.js test
node tools/build-web.mjs
```

Build копирует и проверяет production assets в `dist/web`; нет дополнительного
frontend dev server или отдельной несовместимой сборки. Host обслуживает тот же
source assets directory `minidash_live/web`.

Python с optional transport/data dependencies установлен отдельно в `.venv-web`:

```powershell
.\.venv-web\Scripts\python.exe -m unittest discover -s tests -q
```

Результат: 138 тестов, 137 passed, один skipped (Linux SocketCAN на Windows).
Без optional dependencies: те же 138, 124 passed / 14 skipped.
Frontend: 7 model tests; 9 Chromium integration tests. Все passed.
Build passed. Проверены 360/390/430 px и 1280 desktop: все пять страниц без
горизонтального overflow. Проверен landscape editor. Сохранены 21 screenshot.
Отдельный VirtualBus → RuntimeEngine → API тест проверяет смену UDS/CAN источника и не позволяет candidate codec выглядеть подтверждённым live значением.
Браузерные сценарии включают touch editor, 8 widgets, resize/duplicate/delete,
theme apply, запись/график, import/save, отказ неизвестной версии, offline shell,
disconnect/reconnect и восстановление draft. Физический iOS/Android телефон
и прошивка ESP32 этими тестами не проверены.

## Следующие пять практических улучшений

1. ESP32 Wi-Fi provisioning и тот же versioned Web contract: прямой доступ с телефона.
2. Embedded renderer произвольных screens/widgets/themes с live preview и rollback config.
3. Vehicle evidence для нужных CAN/UDS signals и реальные rate/freshness/RX/TX counters.
4. Flash/SD logging с восстановлением незавершённых записей и экспортом CSV.
5. OTA image validation, progress, firmware rollback и reboot/reconnect.
