# Первая автомобильная сессия: Tiguan II 2017 CZPA / DQ500 / 4Motion

Дата подготовки: 2026-09-30. Ветка `feature/real-car-validation`, база
`3e5c6ca90ca7a9e0fa421c9b5d41e6fcda92b38a`.

## Текущий gate: BLOCKED, автомобиль пока не подключать для MiniDash requests

На проверенном Windows-ПК установлены VW D-PDU OS/OE (x64), VAS6154 Driver и
Rockway VNCI 6154A Device Manager. В реестре PassThruSupport 04.04/05.00 записей
нет. DLL `C:/Program Files/VW_PDUAPI_OE/PDUAPI_VW.dll` и OS-эквивалент имеют
`PDU*`, а не `PassThru*` exports. D-PDU adapter MiniDash пока не реализован.
MDF описывает VAS6154(A) UDS/RAWCAN на CAN1 HS OBD6/14 и CAN2 LS OBD8/9;
это каталог ресурсов, **не перечисление подключённого VNCI и не подтверждение
доступного автомобиля/канала**. Название J2534 в MDF не является FunctionLibrary.

Нельзя подставить эти PDU DLL в J2534 config. Windows passive VNCI recorder также
отсутствует. До устранения этих blockers полноценная сессия CAN+UDS не READY.
`--uds-only` проверяет лишь возможность отдельной UDS попытки и не обходит
проверку DLL; сейчас она тоже BLOCKED. Offline PASS не доказывает connection.

## 0. Команды подготовки дома / на ПК без автомобиля

PowerShell; все команды из **существующего** worktree:

```powershell
Set-Location 'C:/Users/user/Documents/Codex/work/minidash-ux-touch'
$py = 'C:/Users/user/Documents/Codex/2026-09-29/referenced-chatgpt-conversation-this-is-an/outputs/research-venv/Scripts/python.exe'
git status --short
git branch --show-current
git rev-parse HEAD
& $py tools/real_car_preflight.py --config config/examples/tiguan-vnci-engine-read-only.template.json --artifact-dir .local/real-car/preflight
```

Ожидается сейчас exit **1**, `BLOCKED`: DLL отсутствует в config, Windows passive
capture отсутствует, layouts identity probes не подтверждены. Это корректный
результат, не неисправность скрипта. Inventory проверяет Python/dependencies,
PE exports/bitness, XML descriptors, registry, knowledge, writable artifacts,
free disk (>=100 MiB), replay import, CLI config/read-only gates. DLL не исполняется.

Подключить VNCI **к ПК**, сохранив прежний рабочий драйвер. Не запускать upgrade,
не менять serial, не подключать другую библиотеку по названию. Для read-only
inventory, желательно после обычного запуска ODIS и загрузки VCI:

```powershell
New-Item -ItemType Directory -Force .local/real-car/driver-check
& ./tools/Inspect-Vnci.ps1 -Output .local/real-car/driver-check/vnci-windows.json
& $py -m minidash_live doctor --dll 'C:/Program Files/VW_PDUAPI_OE/PDUAPI_VW.dll' --output .local/real-car/driver-check/doctor.json
```

Эти команды не открывают VCI и не инициируют запросы к машине. Отчёт Inspect
может содержать IP и локальные пути: оставить приватным. Отсутствие записей не
доказывает отсутствие всех возможных API. Следующая задача разработчика:
получить разрешённый SDK/ABI установленного provider и локальное MVCI
module/resource enumeration; проверить ComParams, event ownership, cancel/
cleanup и режим RAWCAN receive/listen-only на стенде. Только затем добавлять
адаптер под существующий `Transport`. Альтернатива — доказанный VNCI
J2534-04.04 FunctionLibrary с подходящей bitness и всеми PassThru exports.
Не заменять исправный ODIS драйвер, не угадывать ctypes ABI.

## 1. Обязательные prerequisites для гаража

1. Проверенный adapter / подключённый VNCI, явный CAN channel и listen-only
   capability; baseline traffic и cleanup проверены на стенде. D-PDU branch
   потребует отдельной реализации и своих unit/ABI/physical acceptance tests.
2. Endpoint metadata: ECU, request/response ID, bus/channel, bitrate, addressing,
   источник и дата. Существующее ODIS evidence 2026-09-22 подтверждает **Engine
   7E0/7E8, 500000, normal 11bit** для этого автомобиля; не из типового примера.
   Повторно сопоставить с текущим ECU/подключением, не распространять на DQ500.
3. Для telemetry получить wire-layouts identity DIDs из ODIS raw request/response,
   сопоставить SW `5NA907115D/0003`, HW `06L907309B/H31`, ASAM
   `EV_ECM20TFS0115NA907115D/001004`. Шаблон содержит **только F190**. Не вписывать
   F187/F189/иные DID по догадке. F190/VIN не подтверждает ECU fingerprint.
4. Engine config в приватном `.local/real-car/config.json`. Явные
   `identity_probes` с доказанными `did/target/encoding`; `identity` содержит
   ожидаемые значения, сверяемые с живыми probes, не подменяющие observed identity.
   Значения `ecu.hardware_part_number`, `ecu.hardware_version` и остальные
   targets соответствуют `EcuIdentity`, не legacy названию `sw_part`.
5. ODIS и MiniDash не должны одновременно владеть диагностическим каналом.
   Reference записывать отдельным проходом в совпадающем устойчивом режиме;
   активный ODIS polling не называть пассивной baseline.

Для будущего доказанного J2534 пути исходный config копируется без перезаписи:

```powershell
Copy-Item config/examples/tiguan-vnci-engine-read-only.template.json .local/real-car/config.json
# Заполнить доказанный FunctionLibrary и identity layouts до подключения машины.
& $py tools/real_car_preflight.py --config .local/real-car/config.json --artifact-dir .local/real-car/driver-check --uds-only
```

STOP при любом blocker, несовпадении битности, unknown endpoint/channel/protocol.
Config без полноценной identity годится только для начального F190 identity
capture, а не для telemetry. Default полный preflight остаётся BLOCKED пока
не реализован passive VNCI путь; `--uds-only` никогда не выдаёт полный READY.

## 2. Первый read-only контакт (только после transport prerequisites)

Автомобиль стоит, P/Parking brake, ignition OFF; обеспечить питание автомобиля
и нормальную вентиляцию для запуска двигателя. Подключить VNCI к OBD, ignition
ON, двигатель OFF. Новый session directory для каждого контакта: файлы trace
создаются exclusive, никогда не перезаписываются.

```powershell
$session = '.local/real-car/' + (Get-Date -Format 'yyyyMMdd-HHmmss')
New-Item -ItemType Directory $session
& $py tools/real_car_session.py marker IGNITION_ON --directory $session
& $py tools/real_car_session.py record --config .local/real-car/config.json --output "$session/identity.jsonl"
```

Это использует production `discover_endpoint(stop_on_error=True)`, только
configured identity probes, без scan/broadcast и без авто session-control.
Ожидается `IDENTITY_OBSERVED`, summary/end и raw PDUs в приватном trace.
F190-only означает VIN observation, не подтверждение ASAM и не готовность F405.
STOP на первом NRC (кроме bounded pending 78), timeout, malformed/unrelated
response, несовпадении identity, нестабильном transport. Повторных попыток нет.

Replay начального контакта **до расширения списка targets**:

```powershell
& $py tools/real_car_session.py replay --config .local/real-car/config.json --trace "$session/identity.jsonl" --output "$session/identity-replay.jsonl"
```

Ожидается тот же observed результат с `offline:true`, `vehicle_validation:false`;
PDUs точно совпадают по порядку. Изменённый config не принимается. Хранить
version config отдельно с session. После добавления identity layouts создать
новый config/session и сохранить прежний для replay старого trace.

## 3. Engine telemetry (полная observed identity обязательна)

Каждый вызов читает каждый выбранный target **один раз** через существующие
Registry → Planner → Scheduler → decoder → SignalStore → SourceArbitrator.
Происходит новое discovery и проверка identity при открытии ECU session.
Если target не уникально применим, чтение вообще не начинается. Бюджет: четыре
targets, никаких сотен DID; каждое состояние записывается новым файлом.

Начать coolant-only на стоящей машине:

```powershell
& $py tools/real_car_session.py record --config .local/real-car/config.json --signal engine.coolant_temperature --output "$session/coolant-off.jsonl"
& $py tools/real_car_session.py replay --config .local/real-car/config.json --signal engine.coolant_temperature --trace "$session/coolant-off.jsonl" --output "$session/coolant-off-replay.jsonl"
```

Затем только после PASS и разумного сравнения coolant с reference:

```powershell
& $py tools/real_car_session.py record --config .local/real-car/config.json --signal engine.coolant_temperature --signal engine.ambient_pressure --signal engine.rpm --signal vehicle.speed --output "$session/engine-off.jsonl"
& $py tools/real_car_session.py replay --config .local/real-car/config.json --signal engine.coolant_temperature --signal engine.ambient_pressure --signal engine.rpm --signal vehicle.speed --trace "$session/engine-off.jsonl" --output "$session/engine-off-replay.jsonl"
```

F405 имеет exact-identity decoder и прежнее real ODIS/raw evidence. Новая
сессия даёт новый sample, но автоматически статус `validated` не присваивается.
F433/16F3/F40D имеют existing measured-value evidence, **без raw codec**:
ожидается `raw_only`, `value:null`, status `observed`. Это успешная raw запись,
не успешное отображение числа. Не выдумывать формулы. Engine accelerator/
throttle-position и ECU supply-voltage без подтверждённых конкретных candidate
DID в этом профиле пока исключены. Throttle-pre-pressure не положение педали;
Haldex pump-voltage не supply-voltage Engine. Transmission/4Motion — позже.

Reference: сохранить ODIS значение/единицы и режим/время рядом в приватной заметке.
Для уже decoded F405 можно прикрепить численную проверку (ввести **реальное**
значение reference, единицу **точно из reading.unit**, tolerance по согласованному
критерию; не запускать с примерными значениями):

```powershell
$referenceValue = [double](Read-Host 'ODIS same-stage coolant value')
$referenceUnit = Read-Host 'Exact unit from captured reading.unit'
$tolerance = [double](Read-Host 'Approved comparison tolerance in that unit')
& $py tools/real_car_session.py reference --trace "$session/coolant-off.jsonl" --signal engine.coolant_temperature --value $referenceValue --unit $referenceUnit --source 'ODIS Engineering 17.0.1; engine OFF, same stable stage' --tolerance $tolerance --output "$session/coolant-reference.json"
```

Создаётся отдельный hash-linked `cross-checked`/`rejected` annotation. Он не
изменяет Registry/Provenance и не подтверждает live transport. Reference для
raw-only пока только ручная заметка, не сравнение несуществующего decoded value.

## 4. Passive CAN и ручные markers

**На текущем Windows VNCI нет исполнимой capture start команды. STOP для этого
этапа, пока adapter не принят.** Не пытаться запустить SocketCAN на Windows или
использовать ISO15765 flow-control channel как пассивный recorder. Доступные MDF
RAWCAN ресурсы ещё нужно реально открыть безопасным receive-only adapter.
ODIS raw export допустим как отдельное private evidence, но его converter в
MiniDash schema-1 не реализован: нельзя просто переименовать BLF/ASC/log в JSONL.

Уже поддерживаемый альтернативный стенд — Linux с отдельным подтверждённым
SocketCAN hardware. Это **другой adapter**, не установленный VNCI. После настройки
аппаратного listen-only и физического подключения использовать первый терминал:

```sh
python can_capture.py capture --channel can0 --seconds 600 --max-frames 100000 --output .local/real-car/session/can.jsonl
```

Перед запуском проверить channel, bitrate и что hardware действительно silent;
recorder не передаёт, но его metadata специально сохраняет
`hardware_listen_only_verified:false` — socket-only код не доказывает режим железа.
Markers создавать **на том же хосте/в той же загрузке ОС**, другим терминалом:

```sh
python tools/real_car_session.py marker IGNITION_ON --directory .local/real-car/session
python tools/real_car_session.py marker ENGINE_START --directory .local/real-car/session
python tools/real_car_session.py marker IDLE --directory .local/real-car/session
python tools/real_car_session.py marker RPM_1500 --directory .local/real-car/session
python tools/real_car_session.py marker RPM_2000 --directory .local/real-car/session
python tools/real_car_session.py marker RPM_2500 --directory .local/real-car/session
python tools/real_car_session.py marker BRAKE --directory .local/real-car/session
python tools/real_car_session.py marker ACCELERATOR --directory .local/real-car/session
```

В устойчивом idle/P, без движения руля/педалей/других controls: marker BASELINE,
ждать 2 секунды; marker STEERING_BUTTON с label фактической кнопки, нажать и
удержать 2 секунды, BUTTON_RELEASE. Повторить минимум трижды для каждой
имеющейся OK/BACK/NEXT/PREV/UP/DOWN/stalk кнопки. Labels без VIN/serial.

```sh
python tools/real_car_session.py marker BASELINE --directory .local/real-car/session
python tools/real_car_session.py marker STEERING_BUTTON --label OK --directory .local/real-car/session
python tools/real_car_session.py marker BUTTON_RELEASE --directory .local/real-car/session
python tools/real_car_session.py compare-buttons --trace .local/real-car/session/can.jsonl --markers .local/real-car/session/markers.jsonl --seconds 2 --output .local/real-car/session/button-candidates.json
```

Сравнение отбирает изменения counts/payloads; статус всегда `candidate`,
`production_mapping:false`. На диагностическом OBD bus кнопочные CAN кадры могут
вообще не быть видны. Нулевая разница не доказывает отсутствие сигнала/нужный ID.
Не привязывать эти результаты к UI автоматически.

Short drive необязателен и откладывается до успешного stationary этапа. Нужен
пассажир для markers/ПК; водитель не взаимодействует с инструментами.
При наличии безопасного сценария VEHICLE_MOVING → VEHICLE_STOP, затем
IGNITION_OFF. Остановить recorder Ctrl+C (он пишет end/count) или дождаться limit.
При диагностическом STOP пассивный silent capture может продолжаться отдельно.

CAN replay использует существующий CLI и реальные configured DBC/identity:

```sh
python -m minidash_live replay-can engine.rpm vehicle.speed --config .local/real-car/session/can-config.json --identity-snapshot .local/real-car/session/identity-snapshot.json --trace .local/real-car/session/can.jsonl --output .local/real-car/session/can-replay.jsonl
```

`can-config.json` содержит `dbc_databases` с реально доступными легальными файлами;
snapshot берётся из production discovery, не генерируется из желаемого fingerprint.
Без этого — только inspect/compare, никакого fake-decoded результата. CLI legacy
`discover --debug` годится для snapshot на **отдельном** разрешённом проходе, но
не заменяет fail-fast record первой сессии.

## 5. Артефакты, replay и STOP checklist

Сохраняются private: config, raw UDS request/response (включая VIN), discovery,
transport IDs/provenance, план, sample/quality/decoder, UTC+monotonic timestamps,
NRC/timeout/fault, summary/end; CAN frame trace и sidecar markers, reference report.
Все новые vehicle outputs под `.local` (gitignored). Не делать `git add -f`.
Фабричные flash backups и прежние 57 artifacts не перемещать/не перезаписывать.

```powershell
& $py tools/real_car_session.py sanitize --trace "$session/identity.jsonl" --output "$session/identity-sanitized.jsonl"
git status --short
git check-ignore "$session/identity.jsonl"
```

Sanitize убирает metadata/decoded identity/точное время и redacts F180..F1FF
positive identity payloads. Это review aid, **не автоматически пригодный к push
fixture**: другие DID/raw CAN могут содержать serial/location/private data.
Sanitized identity trace намеренно non-replayable; original private trace —
источник faithful replay. Для public regression сделать отдельно вручную
sanitized **synthetic** identity fixture, сохранив provenance/scope.

STOP: unknown route/channel/identity; unexpected NRC; timeout/transport fault;
security access needed; неверный ответ; write/state-change needed; unstable bus.
Никаких retries, automatic reconnect, 0x10/0x3E в первом narrow milestone.
Даже допустимые вообще session/tester-present операции здесь не включены —
добавлять только после документированного требования и отдельного review.
Live DTC read 0x19 пока не реализован; не использовать simulator как live API.
Запрещены 0x14 clear, 0x11 reset, 0x27 security, 0x2E write, 0x2F actuator,
0x31 routines, flashing, coding, adaptation, basic settings/output tests.

Проверить summary: CAPTURED/IDENTITY_OBSERVED, либо точный STOP/error; для replay
проверить отсутствие unconsumed exchanges, качества/TTL/arbitration. Повреждённый
или незавершённый UDS trace отвергается целиком до replay; сохранить для ручного
fault analysis, не дорисовывать responses/end. Ошибочный trace с корректным end
можно replay для воспроизведения STOP. Replay не меняет validation status.

Следующий шаг после capture: exact identity + source/channel review → reference
сравнение и raw formula evidence → production decoder tests на sanitized fixtures
→ ручное решение о scope `validated` → только затем расширение targets/UDS budget,
DQ500/Haldex и automotive button adapter. Никакого массового promotion кандидатов.
