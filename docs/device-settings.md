# Device Settings / Advanced Settings

Расширена существующая страница Device; Dashboard/Screens/Signals/Logs сохранены.
Настройки сгруппированы в mobile cards и bottom sheets: General, Display, Wi-Fi,
Vehicle / CAN, Data Acquisition, Logging, Power, System, Advanced.
Data Acquisition и Advanced скрыты до включения Show advanced settings.
Есть поиск по названиям секций и параметров, Discard / Apply и подтверждение
reconnect, acquisition restart, reboot и destructive actions.

## Работающие функции на host

- Device name, locale preparation (en/ru; это не перевод всего UI), metric/imperial/custom,
  temperature/pressure/speed units, timezone/time format и startup/remember screen preferences.
- Общий `units.js` переводит значения для представления; canonical readings, thresholds
  и файлы записей остаются в исходных единицах. Timezone влияет на формат даты,
  но не меняет часы Windows. Device clock sync пока unavailable.
- Display brightness preview в browser; manual/day/night и auto по времени выбранной timezone.
  Live здесь означает безопасный локальный preview. Изменения сохраняются только после Apply.
- Logging enable, auto start на host startup, JSONL, selected/all active signals,
  rotation time, maximum recording storage и retention. Старые завершённые файлы
  удаляются по retention/лимиту при старте следующей записи. Запись останавливается
  при time/size/storage limit; это не автоматический бесконечный multi-file recorder.
- Configuration persistence, backup/import с миграцией, Discard, diagnostic bundle,
  reset settings и защищённый factory reset.
- Simulation: AP/Station/AP+Station preferences, виртуальный scan, brightness/rotation,
  power modes, simulated restart/reconnect. Radio/network Windows реально не изменяются.
  Default rate и stale timeout воздействуют на canonical simulation; UI rate меняет SSE interval.
  CAN/UDS rates, source preference/fallback остаются подготовленной политикой:
  simulation имеет один canonical source и не изображает автомобильный CAN/UDS traffic.

## Что ожидает ESP32 firmware

Actual AP/STA provisioning/scan/restart, физическая яркость/rotation/FPS, light sensor,
CAN/UDS reconfiguration, ignition GPIO/CAN detection, boot/shutdown power control,
device reboot и hardware metrics. Автономный backend Wi-Fi и OTA пока отсутствует.
В host-only/runtime эти controls disabled с причиной Not supported by current firmware.
Firmware не менялась и не собиралась в этой задаче.

READ ONLY обязателен. CAN bitrate выбирается только из подготовленного набора
Auto/125k/250k/500k/1M. UDS rate ограничен 0.2–2 Hz; реальный runtime profile и его
per-route budgets не перезаписываются Web settings. Новых VAG CAN IDs/DIDs нет.

Power различает manual KCD11 master switch, display off, sleep, deep sleep и
недоступный hardware power off. Deep sleep не означает физическое нулевое потребление.
В simulation sleep/deep sleep останавливают публикацию readings, но host Web API остаётся
доступным. MOSFET cutoff и ignition controls отключены.

## API / schema contract

| Endpoint | Назначение |
| --- | --- |
| `GET /api/capabilities` | Features с supported/scope/reason; wifi_modes, read_only, power limits |
| `GET /api/settings/schema` | Versioned секции/поля, типы, defaults, диапазоны, capabilities и impacts |
| `POST /api/settings/plan` | Проверка candidate config/revision/secrets; изменения и reconnect/reboot destination |
| `POST /api/settings/apply` | Атомарный commit; confirmed требуется для restart/reconnect/reboot impacts |
| `GET /api/settings/status` | Operation lifecycle, Device ID/info, только credential-present flags |
| `POST /api/wifi/scan` | Сейчас только explicitly labelled simulation networks |
| `POST /api/system/action` | restart_device/restart_wifi/reset_settings/factory_reset, с confirmation и revision |
| `GET /api/diagnostics/bundle` | Device/config/runtime/stats/recent events/errors/log metadata, без secrets |

Прежние `/api/configuration`, `/api/configuration/validate`, `/api/events`,
`/api/signals` и recording endpoints сохранены. Старый PUT configuration проходит
через тот же settings capability/impact guard и не позволяет обходить Apply protection.

Configuration schema **v2** добавляет `device_settings`. v1 files на загрузке
мигрируют без потери screens/themes/legacy preferences. При импорте v1 backup новые
device preferences, включая Wi-Fi, сохраняются: старый backup не сбрасывает поля,
которых ещё не существовало. Неизвестные версии отвергаются. Invalid fields/rates,
IP/subnet, unsafe read_only, unsupported capabilities и stale revisions не сохраняются.

UI строит controls из schema и actual feature capabilities. Scope равен host,
simulation или unavailable. Перед применением показываются Wi-Fi SSID/address и
restart/reboot impact. Operation status приходит в SSE и через status polling;
в simulation этапы помечены Simulation, без реального разрыва сети.

## Wi-Fi secrets

Пароли передаются отдельно в write-only `secrets: {ap_password, station_password}`
на plan/apply. Ответы не содержат значения; GET configuration/status возвращают только
boolean presence flags. Пустой secret удаляет сохранённое значение, отсутствие ключа
сохраняет его. WPA2 требует пароль 8–63 printable ASCII либо 64 hex digits.

На Windows secrets защищены DPAPI текущего пользователя. Зашифрованный DPAPI-block
находится вне `configuration` в storage envelope `.minidash/configuration.json`.
Один atomic replacement сохраняет settings/revision и encrypted credentials вместе.
Обычный JSON export содержит только public configuration; диагностический bundle
также удаляет secret-like keys и известные secret values из runtime/error strings.
Пароли не пишутся в localStorage draft, service-worker cache или recordings.
После перезагрузки страницы несохранённый пароль нужно ввести заново.

Portable backup не переносит credentials; после импорта на другое устройство Wi-Fi
password вводится отдельно. В среде без secure credential store Wi-Fi credential
configuration capability отключается. DPAPI storage не предназначен для переноса
файла между чужими Windows accounts.

Factory reset требует confirmed + точный полный Device ID на backend; UI требует
ввести ID и до этого отключает кнопку. Factory reset удаляет credentials и сбрасывает
screens/themes/settings; существующие recordings остаются и удаляются отдельно.
Reset settings сохраняет credentials и screens/themes. System actions запрещены во
время активной записи или незавершённой operation.

## Запуск и проверка

Обычная simulation и телефон в одной сети с ПК:

```powershell
.\tools\Run-WebConfigurator.ps1 -Lan
```

Host без simulated hardware capabilities:

```powershell
python -m minidash_live.configurator --host-only --port 8767 --data-dir work/host-only
```

Открыть `http://127.0.0.1:8765/#device`.
Tests/build:

```powershell
.\.venv-web\Scripts\python.exe -m unittest discover -s tests -q
node --test tests/frontend/model.test.js tests/frontend/units.test.js
node node_modules/@playwright/test/cli.js test
node tools/build-web.mjs
```

Проверены settings validation/persistence/migration, DPAPI/write-only passwords,
secret-free config/bundle, capability bypass rejection, host disabled controls,
brightness draft preview, mobile search, Discard/Apply, reconnect/reboot lifecycle,
factory reset guard, logging controls и прежние UI сценарии.
Размеры: 360/390/430 px, 844×390 landscape, 1280 desktop. Physical phone/ESP32 не проверялись.

Итог: Python — 152 tests, OK (151 passed, 1 SocketCAN skip на Windows);
frontend model/units — 10 passed; Playwright — 19 passed; production static build — passed.
Дополнительно проверены screenshots всех девяти settings sheets на пяти размерах.
