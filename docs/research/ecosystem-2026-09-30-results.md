# MiniDash: forensic audit, research и выполненная интеграция

Дата: 30.09.2026. Ветка: `feature/research-replay-reliability`.
База: подтверждённая пользователем ночная UI-версия `6f436dd`.

## 1. Repository state и сохранность

Проверены status, branch -a -vv, all-ref graph, reflogs, stashes, remotes и
три существующих checkout. Обычный fetch был ограничен refspec только для
`feature/diagnostic-data-import`; явный all-heads fetch обнаружил актуальные
MQB/UI/production refs. Это исправило картину аудита, без reset/rebase/clean
и без изменения рабочих файлов других checkout.

Ночная работа найдена локально и на GitHub: `6f436dd`. Пользователь подтвердил,
что именно она имелась в виду. Сравнение `git diff 6f436dd -- firmware core
docs/ui docs/architecture/targets.md` пустое: прошивка/UI/Web/core не изменены.
Старые display `dc16a90` и production `00c6317` не считались последней версией.
Локальных commits, отсутствовавших на всех remote ветках после полного fetch,
не найдено; stash во всех изученных checkout пуст.

В оригинальном checkout остались uncommitted research/MCD/profile/registry/test
изменения и untracked factory/ODIS artifacts/typed fixture. До кода сохранены
binary patch, status/history/reflog/stash snapshots и synthetic fixture вне repo.
Создан SHA-256 manifest всех 57 artifact files. После работы original status,
dirty patch и fixture byte-for-byte совпадают с сохранёнными; все 57 hashes
совпали. Factory backup не перезаписывался. Proprietary/private artifacts не
добавлялись в Git. Часть reviewed typed-scalar работы перенесена в отдельный
коммит; оригинал намеренно не очищен. Research notes изучены, но не заменили
более новые объединённые документы.

PR inventory не установлен: публичный GitHub REST ограничил запросы 403 rate
limit; git transport работал. Это не доказательство отсутствия PR. Existing
feature branches сохранены; main не менялся. Эта integration branch пока
локальная: новый push/force-push не выполнялся.

## 2. Что уже есть и что найдено

Полная component matrix, источники/лицензии и план до кода:
[ecosystem-2026-09-30.md](ecosystem-2026-09-30.md).
Существующие transport/discovery/identity/providers/applicability/planner/
scheduler/store/arbitration/CLI/trace не заменены. Нет параллельного CAN decoder,
SignalStore, UI framework или knowledge importer.

Найденная remote `feature/mqb-knowledge-import` добавляла семь ранее не
интегрированных commits: sourced MQB/Haldex/UDS candidates, pinned metadata и
license/revision, passive grace перед UDS fallback, отказ от default session
switch/keepalive, fixture/evidence tests, tagged car validation manifest.
Она объединена обычным merge с актуальным UI. Единственный конфликт был в
одинаковой Windows/Linux SocketCAN skip-декорации tests/test_can.py; устранён
повтор import socket. UI files в merge не затронуты.

После объединения host registry содержит 134 definitions: 53 CAN и 81 UDS.
Это не 134 подтверждённых живых показателя и не 134 новых сигналов. Поле
confidence=confirmed у части прежних записей не означает существование raw
codec/точного vehicle evidence. В прошивке остаётся прежняя vocabulary из 12
канонических сигналов; автоматически расширять её неподтверждёнными данными
не стали. Новые community/DBC layouts не повышены до verified.

Исследованы primary sources SavvyCAN, opendbc, cantools/python-can/can-isotp/
udsoncan/can-utils, odxtools/OpenXC/ESP32RET, RaceChrono/RealDash/Car Scanner,
Ross-Tech/OBDeleven/Clever, CANedge, ESP-IDF TWAI/HTTP/OTA/sleep и TI power
protection. GitLab/Habr/Drive2/STM32/OLED searches оставлены как research leads,
если нет надёжного первичного подтверждения. CanSensor не удалось уверенно
идентифицировать по названию — его характеристики/лицензия не выдуманы.

Не найден новый доказанный raw decoder именно для текущего автомобиля.
Польза свежего поиска: использовать existing log formats/readers, измерять
через детерминированный replay, различать job cooldown и реальный bus budget,
не путать CAN silence с ignition, рассматривать power-safe storage и OTA
rollback как отдельные hardware/boot acceptance gates.

## 3. Выполненные A/G improvements

### Короткий watch больше не теряет всё время на подготовку

С installed optional dependencies пойман fail: watch --duration .2 иногда
заканчивался без вывода, потому что план/catalog tracing занимал весь duration.
Deadline перенесён после setup; deterministic regression имитирует подготовку
дольше acquisition interval. Семь pipeline tests, включая VirtualBus, прошли.

Файлы: minidash_live/__main__.py, tests/test_pipeline.py.

### Реальная CAN-запись → offline existing runtime

Добавлен `python -m minidash_live replay-can` для существующего schema-1 JSONL
recorder. Использует прежние planner/DbcAdapter/store/arbitrator с injected
ReplayClock; не открывает bus/socket/VCI/ECU, не вызывает scheduler step,
не делает discovery fallback или UDS requests. Required identity snapshot и
external DBC paths не являются новым vehicle validation evidence.

Bounded streaming reader проверяет JSON/duplicates/line size/ID/DLC/flags/hex/
finite timestamps/order/end count. Interrupted missing end помечается incomplete;
malformed tail вызывает ошибку. Output exclusive-create, partial failures
помечаются replay_error без success summary. Все outputs — offline:true,
vehicle_validation:false. --final-age тестирует TTL без реального ожидания.
ASC/BLF converter остаётся следующим этапом через существующий python-can,
а не самописный второй parser.

Файлы: can_capture.py, minidash_live/replay.py, __main__.py,
tests/test_can_replay.py, docs/runtime/can-replay.md, README.md.

### Reviewed local typed MCD work сохранена в общей версии

Перенесены локальные single-leaf IEEE754 FLOAT32/TEXTTABLE и signed invalid
sentinels. Profiles и Registry используют общий validator. Новые ограничения:
unknown byte order отклоняется; enum choices bounded/non-overlapping;
unknown/default choices/nonfinite floats не выдаются как корректные значения.
Legacy profiles без signed сохраняют прежний unsigned default. Imported
records остаются candidate/database-only, raw.* names не подменяют canonical
без explicit map; автомобильные данные/проприетарная база не включены.

Файлы: minidash_live/{mcd_import,profiles,registry}.py,
tests/test_signal_registry.py, tests/test_scalar_codecs.py,
tests/fixtures/mcd-typed-scalars.json и source/fixture documentation.

## 4. Tests / measurement / regressions

| Проверка | Результат |
|---|---|
| Baseline до изменений | Python 129 tests, 13 optional/platform skips, PASS |
| Первый full suite после merge с optional deps | 139 tests, 1 failure: watch setup deadline; исправлено отдельным commit |
| Pipeline с regression и VirtualBus | 7/7 PASS |
| Replay contracts / CLI / faults | 12/12 PASS |
| Typed codec validation/compatibility | 6/6 новых tests PASS + recovered scalar test |
| Итог с pinned optional dependencies | 159 tests, 1 skip (Linux AF_CAN на Windows), PASS |
| Итог без optional dependencies | 159 tests, 18 skips, PASS |
| Portable CMake/CTest | 2/2 executables PASS |
| Node Web editor logic | PASS |
| git diff --check | PASS |
| UI/firmware/core versus 6f436dd | identical |
| Original dirty files/artifacts | patch/status/fixture + 57 hashes unchanged |

Пакеты установлены только в isolated test venv: python-can 4.6.1, can-isotp
2.0.7, udsoncan 1.26.1, cantools 41.0.2. Остальные 17 optional skips базового
Python больше не скрывают VirtualBus/DBC интеграционные проверки.

На synthetic 10,000-frame fixture измерено 1.559 s / ~6,415 frames/s
для file read + validation + decoding + snapshots/arbitration, без JSONL sample
output. Все 10,000 decoded, UDS requests=0; final-age сделал оба signals stale.
Это PC processing measurement, не CAN receive capacity и не ESP32 FPS. Не
измерены реальные bus load/ECU latency, touch/render FPS, RF coexistence,
logging power-loss или автомобильный standby current. Они требуют соответствующей
нагрузки и стенда, не заменяются synthetic throughput.

Прошивка не менялась и не перепрошивалась в этой задаче. Предыдущие on-device
25/25 UI и четыре 24/24 AP цикла относятся к commit 6f436dd и сохраняются как
исторические результаты, а не выдаются за новое аппаратное испытание.

## 5. Commits

| Commit | Логический scope |
|---|---|
| 18b74d9 | forensic state + pre-code ecosystem research report |
| 874c202 | merge существующей MQB knowledge-ветки с актуальным UI |
| 5ae64cf | fix watch duration + deterministic regression |
| fe3346f | offline real CAN replay + bounded input/tests/docs |
| 013d350 | reviewed local typed-scalar recovery + compatibility/tests/docs |

Финальный docs commit содержит этот execution report. Никаких force operations,
main merge, push или branch deletion. Original/UI branches остаются доступны.

## 6. TOP NEXT IMPROVEMENTS: конкретные решения

| Приоритет / категория | Зачем и что есть | Exact gap / основание | Сложность/риск | Зависимость / компоненты |
|---|---|---|---|---|
| P0 B/G: tagged MQB validation | 134 definitions и manifest уже есть; расширить реальные данные | Проверить gateway visibility/units/qbits/counters/checksums и сопоставить с ODIS; pinned opendbc | Средняя / ложная интерпретация | Car raw trace, exact identity; data/external, validation evidence/fixtures |
| P0 A/G: ASC/BLF/candump adapter | Existing reader/replay + python-can | Export разных tools пока нельзя непосредственно replay; official LogReader | Низкая / timestamp/channel | Без машины на recorded fixtures; can_capture/replay CLI |
| P0 A/G: diagnostic replay preservation | Synthetic UDS replay и raw redacted traces есть | Нужен bounded sequential request/pending/error fixture compiler, не один response dictionary | Средняя / VIN redaction и ordering | Local traces; replay/uds fixtures, preserve allowlist |
| P1 D/G: screen/log subscriptions | Watch принимает fixed requested signals; UI knows pages | Protocol subscription versioning, background minima для warnings/logging, no catch-up bursts | Средняя / забытые safety signals | Existing planner/runtime/display bridge; fake clock first, then car load |
| P1 B/G: bus/ECU budgets | Cooldown/backoff/passive grace уже есть | Dynamic feedback/priority без guessed rate; can-isotp limiter и canbusload | Средняя / ECU overload | Bus measurement + trace; scheduler/transports/metrics |
| P1 B/D: read-only DTC report | 0x22 и identity path есть, FakeECU DTC недостаточен | Scoped 0x19 status/snapshot/extended data, exact ECU fixtures; udsoncan reference | Средняя / variant parsing | Bench ECU replies → car; uds/model/CLI, отдельный plan до кода |
| P1 A/G: firmware health telemetry | Watchdog/reset/heap primitives есть | Bounded status contract, boot reason/heap/latency/drop counters; don't write NVS per frame | Низкая–средняя / wear и overhead | Existing main/Web/diagnostics; bench timing, no new CAN IDs |
| P1 B/C: power + sleep | ESP/display sleep есть; BL fixed | Protected OBD frontend/transients/crank/standby + verified wake/ignition; TI/IDF docs | Высокая / battery drain, reset | Actual BOM/schematic/current measurement; power/transceiver/display |
| P2 C/D: signed A/B OTA | IDF API есть, installed factory-only layout | Slots/otadata, backup/repartition migration, first-boot health, keys/config/assets compatibility | Высокая / brick/irreversible keys | Dedicated backup/bench; partitions/bootloader/update/status |
| P2 D: RaceChrono/RealDash | Canonical bridge уже есть | Optional protocol adapter, BLE security/capability and phone compatibility | Средняя / BLE transport constraints | BLE/bridge adapter only, no arbitrary UDS; phone tests |
| P2 C/D: Lite | 8 common C modules and target budgets есть | Confirm MCU/OLED; link map/stack/button/renderer, no project fork | Средняя / 20 KiB RAM limits | New approved hardware; existing shared model/config/input |

## 7. Safety/security/hardware gates

Read-only is separate from UI config writes and future diagnostic writes.
Current explicit temporary AP uses WPA2 random session credentials/token/revision
and bounded HTTP JSON; HTTP is local AP plaintext, not authenticated Internet
OTA. Product BLE commands/OTA authenticity are absent, not assumed safe by default.
No raw ECU/DTC clear/coding endpoint was introduced. Unknown vehicle still cannot
promote a candidate or turn on generic write operations. Root project LICENSE
is not present; project owner should choose redistribution terms before copying
third-party code. This batch reuses existing code/data and external APIs; no
unknown-license external implementation was copied.

Автомобиль требуется для CAN visibility, identity/raw decode evidence, ECU and
bus-load, steering/stalk mappings, ignition timing and DTC variants. Current
UI sleep / CAN silence не доказывают выключение машины. Hardware требуется для
protected OBD power/current measurement, verified transceiver/wake pins, BL
control/MOSFET и OLED target. Не выбраны новые pins/controllers/vehicle IDs.
Purchased ESP32/TFT/XPT2046 известны; остальные BOM/power схемы неподтверждены.

Первый следующий самостоятельный software batch: optional log-format conversion
и sequential UDS trace replay с redaction/fault fixtures. Первый автомобильный
batch: существующий tagged MQB validation manifest на подтверждённой проводке,
пассивная запись/identity → replay → проверка against reference; без clear/coding.
