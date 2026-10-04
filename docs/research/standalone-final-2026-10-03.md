# MiniDash standalone: итог реализации и границы проверки

Обновлено при завершении восстановления 2026-10-04 UTC / Minsk.
Ветка `feature/standalone-acquisition`. Baseline consolidation `3e5c6ca`, runtime
`18095539`, startup/ISO-TP hardening `4904233`. Main не меняется.

## Результат

На ESP32-S3 реализован автономный путь CAR/OBD → transceiver → TWAI → ISO-TP/OBD
→ samples/TTL/arbitration → физический LVGL UI. PC/Python/телефон/VNCI/ODIS/ELM
не входят в production path. Сохранены экраны, тач/NVS, темы, four Custom pages,
редактор на временном Wi-Fi AP и все host development/research tools.

Это **offline-tested firmware, не real-car PASS**. Проводка, transceiver, питание,
ток покоя и поведение на Tiguan не проверены. QUIET не electrical off; нужен KCD11.
VAG profiles выключены до точных VIN/ECU/endpoint/decoder evidence.

## Аудит и выбор архитектуры

До runtime checkpoint `app_main()` запускал UI и USB-консоль. Acquisition,
discovery, knowledge и arbitration выполнялись Python-процессом. Наличие дисплея
не означало автономность. Основной upgrade поэтому — firmware acquisition,
а не дополнительная косметика интерфейса.

| Компонент | Было | Теперь / ограничение |
|---|---|---|
| CAN HAL | Host transports | TWAI v2 task, checked TX4/RX5 installation preset |
| ISO-TP | Python | Bounded C RX128 bytes; single-frame requests |
| Discovery | Host identity workflow | Standard 11-bit/500-kbit OBD responders + capability bitmaps |
| Identity | Host models | Mode09 VIN; exact compiled ECU identity gate |
| Providers | Python knowledge | Compiled UDS/passive bindings with provenance/license/address gates; no profile enabled |
| Planner/store | Host | Visible/background demand, TTL, per-source samples, passive > UDS > OBD |
| UI | USB samples | Native canonical publishing, status/DTC touch action |
| Logs/replay | Host tools | Preserved; native RAM ring96 frames + C snapshot API |
| Sleep | UI state | QUIET stops requests/listens; no automatic power switch |
| DTC | Simulator/host plans | Read Mode03 selected emissions ECU only; max12; no clear |

```mermaid
flowchart TD
  A["TWAI vehicle task"] --> B["ISO-TP / OBD + gated UDS"]
  A --> C["Gated passive decoder"]
  B --> D["Samples / TTL / arbitration"]
  C --> D
  D --> E["Derived values / canonical UI"]
  E --> F["Visible and warning demand"]
  F --> B
```

Discovery/provider/planner/store разделены функциями portable `acquisition.c`;
HAL в `vehicle_app.c`, ISO-TP в `diagnostic_transport.c`. Это компактный runtime,
не ODX interpreter. Не переносим Python hierarchy или все host definitions.
База и прежние ветки сохранены; история объединения — в
[consolidation audit](repository-consolidation-2026-09-30.md).

## Реализованные ограничения и safety review

- Classical CAN11-bit/500 kbit, startup listen-only1.5 s, PID00 responders
  7E8…7EF, выбор поддерживающего RPM. Не угадывает адреса других VAG ECU.
- Capability blocks00/20/40, восемь OBD значений: RPM, speed, coolant, oil,
  load, MAP, baro, module voltage — только поддержанные. MAP−baro — derived boost.
  ECU voltage и MAP не подменяют battery-terminal voltage/charge-pressure DID.
- Одна transaction, ≥100 ms между application requests, no catch-up bursts.
  Видимые быстрые jobs имеют target200 ms, background≥1 s; это не измеренный load.
- Response500 ms, interframe250 ms, hard deadline2 s, bounded pending/retry.
  RX queue64, max32 frames/tick, delay10 ms, single-shot TX, bounded bus recovery.
- Manufacturer bindings: exact VIN/endpoint/identity response, provenance/license;
  `.profile=NULL` в поставке. Нет CAN FD/K-line/TP2.0/29-bit OBD/multi-frame TX,
  session switching/TesterPresent/security/routine/write/flash services.
- Startup: status mutex до UI, store до producer; engine initialization и failure
  status под lock. UI не вызывается под CAN mutex.
- ISO-TP service/PID/DID/address и declared length проверяются до Flow Control.
  Regression: matching RPM prefix с неправильной multi-frame длиной не вызывает TX.
- TTL/stale/unsupported/unknown не превращаются в нули. Profile mismatch закрывает
  manufacturer polling. DTC read требует свежей speed=0, clear отсутствует.
- QUIET после bounded failures или120 s свежих RPM=0/speed=0. Hold/manual retry,
  bus recovery или обычный трафик после паузы≥5 s могут перезапустить discovery.
  Долгий start-stop — известное ограничение, это не ignition detection.

## Проверки и восстановление среды

После очистки рабочей среды локальная копия оказалась на `18095539` с hardening
diff. Через GitHub подтверждён `4904233`, локальный tree совпал с
`7d6eab85b32c9667461bd95b7d2b1c4bc00039f6`; восстановлен точный commit object.
Ничьи изменения не сброшены. Поздний незакоммиченный HTTP export не сохранился
и **не включён** в эту поставку. Вместо заявления об утраченном результате
интеграционный test harness восстановлен и запущен заново.

| Gate | Текущее доказательство |
|---|---|
| Python | Свежий acceptance run; точные результаты в committed verification JSON |
| Native core/JSON/acquisition | CTest3/3, включая 8 сценариев, 50,000 malformed frames и hardening regression |
| Firmware integration | Actual C engine + python-can VirtualBus: VIN/DTC ISO-TP, loss, no-TX quiet, fault/reconnect, exact synthetic replay |
| Web | Node + C-derived fixture; не browser/device visual test |
| Memory safety | Portable ASan/UBSan; leak detector disabled из-за ограничений task metadata |
| Firmware | Свежий ESP-IDF5.5.5 PASS 2026-10-04: standalone, listen-only, CAN-disabled; SHA256/config в verification JSON |
| Board/car | Не выполнено; ни одна плата этим продолжением не прошивалась |

Предыдущие build artifacts/toolchain утрачены; старые размеры не используются
как измерение текущего дерева. 2026-10-04 toolchain восстановлен, компоненты
скачаны строго по lockfile и сверены по hash. `prepare_locked_components.py`
использует download/hash API менеджера2.5.2, без version solver. Сборка через
официальные `IDF_COMPONENT_MANAGER=0`/`EXTRA_COMPONENT_DIRS` не требует инспекции
процесса CMake и не меняет firmware dependencies. Подтверждены cmake_utilities0.5.3,
esp_lcd_ili9341 2.0.2, esp_lvgl_port2.9.0 и LVGL9.5.0. Команды — в
[development](../development.md), результаты — в
[verification JSON](standalone-verification-2026-10-03.json).

Неудачные промежуточные configure/build не считаются PASS: manager2.5.2/2.4.11
не видел process metadata, 2.2.2 не понимал optional Kconfig, в опытах с2.4.2
resolver создавал смешанное дерево LVGL9.5/9.6 и менял lockfile. Менять pydantic
оказалось недостаточно. Эти пути не включены в рекомендованную процедуру; SDK
и project sources не патчились, исходный lockfile сохранён без изменения.

Свежие образы (app version `f86224b`; последующие изменения только tools/docs):

| Preset | Binary bytes | Free factory bytes | CAN / OBD / USB injection |
|---|---:|---:|---|
| standalone | 1627680 | 1518048 | on / on / off |
| listen-only | 1627568 | 1518160 | on / off / off |
| default | 1719616 | 1426112 | off / off / on |

Standalone link: DIRAM227611/341760 bytes, BSS111792, flash code1058446,
flash rodata436616. Хеши относятся к этим конкретным сборкам, не обещают
побитовую воспроизводимость при другой дате/toolchain/пути сборки.

Native acquisition state был измерен как3504 bytes (host ABI), включая RAM trace
96×24=2304 bytes. CAN task stack6144 bytes; queue/status/UI/AP добавляют память.
Нет per-frame heap/flash writes. Runtime heap/high-water/FPS/coexistence на ESP32
не измерены. Factory app3 MiB, без A/B OTA. Размер режима с host self-tests нельзя
выдавать за чистую стоимость нового acquisition.

## Источники, файлы и продолжение

[Source/license review и матрица](standalone-sources-and-matrix.md) фиксируют
результаты исследования без импорта restricted/unlicensed кода и непроверенных DID.
[REAL_CAR_TEST](../../REAL_CAR_TEST.md) содержит wiring, presets, usage/recovery.

Runtime: acquisition/diagnostic_transport/vehicle_app .c/.h, main/display_app,
Kconfig/CMake/presets. Hardening: main/vehicle_app/acquisition/core regression.
Завершение: `core/acquisition_harness.c`, CMake, `tools/verify_standalone.py`,
обязательный gate в `verify_repository.py`, `prepare_locked_components.py`, README/status/architecture/development,
UI usage, research reports, verification JSON, REAL_CAR_TEST. NVS schema и
partition table не менялись; factory backup не тронут.

| Приоритет | Цель / компоненты | Acceptance / риск / входные данные |
|---|---|---|
| P0 | Power/transceiver/physical UI | ON/QUIET/OFF currents, no backfeed, crank/transient bench; правильная схема |
| P0 | Tiguan read-only validation | Cold boot→LIVE, supported PIDs vs reference, disconnect/reconnect; exact version/trace |
| P1 | Verified profile compiler | VIN/identity/endpoint/raw evidence + offline regressions до enabling DID |
| P1 | Passive/steering | OBD visibility, counter/checksum/invalid encoding, debounce; no button injection |
| P1 | Native diagnostics | Bounded ECU-scoped identity/0x19, licensed descriptions; clear отдельным guarded этапом |
| P2 | Trace export/logger | Bounded snapshot, VIN privacy, power-loss storage; текущий ring не trip logger |
| P2 | OTA/optional phone/Lite | Signed A/B rollback, memory budget, display/input HAL; телефон не runtime dependency |

Все safe offline изменения сохраняются в feature-ветке, без force/merge. Финальный
commit SHA проверяется после публикации, не встраивается как самоссылка в дерево.
