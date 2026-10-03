# Standalone: источники, лицензии и сравнение

Исследование 2026-10-03. Восстановлено из результатов предыдущей сессии после
очистки среды; внешние checkout/build artifacts не входят в репозиторий.
Pinned revisions ниже фиксируют исследованный код. Коммерческие сайты — claims
производителя, не результаты наших испытаний. Лицензия отсутствует ≠ public domain.
Новый внешний код, assets и DID tables в firmware не копировались.

## Исходники и практические выводы

| Источник / revision | Что изучено / полезно | Лицензия и решение |
|---|---|---|
| [AIVS STM32 OLED](https://github.com/aivs/OBD2_STM32_OLED_MONITOR), `1c668d274e9da6996da2746c23836f0c4820f9f4` | `Core/Src/main.c`, `obd2.c`: STM32F1 HAL CAN, u8g2 SSD1322/SH1122, round-robin Mode01; UDS example commented. RX handler не использует length, unknown PID mapping небезопасен для переноса | Root blanket license не установлена; ST driver terms не лицензируют весь авторский код. Архитектурная идея direct CAN, без копирования |
| [AIVS blackCockpit](https://github.com/aivs/blackCockpit), `8175c9e440878f400e4ccef5d7f71b5b936c28f7` | `blackCockpit.py`: Python/RPi/Kivy/SocketCAN; fixed UDS endpoints/DIDs, RPM every10 ms, остальные round-robin; data indexing без полных проверок | Root license не установлена. Не standalone ESP32 template и не доказательство адресов Tiguan. Список DID не импортируется |
| [DPF Load Monitor Wide](https://github.com/v-cu/dpf-load-monitor-wide), `f902045bfedbd54d6e3c7fa0d69ec55e868ed1a8` | P4/TWAI, `can_dpf.c`, `can_driver.c`, 18 DID entries, fast/slow4:1,35 ms; fixed7E0. RX callback передаёт bytes без достаточных ID/length/service gates | CC BY-NC-SA4.0 в LICENSE.txt. Не копировать restricted tables/code. Diesel != CZPA. SD присутствует, logging не реализован; bare P4 без radio |
| [OpenGauge](https://github.com/OB-Ryan/OpenGauge), `92f8426f03f9a44e0a1438603a18509adadfe09e` | ESP32/TWAI, MCP2551/level shift, LCD16x2/button; IDLE/WAITING timeout1.2 s, response mode/PID gates | Root license не установлена. Полезна минимальная state machine, не электроника для прямого копирования |
| [RejsaCAN ESP32](https://github.com/MagnusThome/RejsaCAN-ESP32), `0e05c0eb0c659a1f709439fbcd767ad04506eedb` | README/pinout/BOM: отдельная power strategy, FORCE_ON/SENSE/CAN_RS, voltage thresholds | Hardware reference; root license не установлена. Power supervisor нужен отдельно от firmware QUIET |
| [OVMS](https://github.com/openvehicles/Open-Vehicle-Monitoring-System-3), `86dfa0afe4a8c5b2978ef069caedac661627b180` | `vehicle/OVMS.V3/components/poller/src/vehicle_poller*.cpp`: state-aware polling/throttling и ISO-TP validation | MIT core/header, third-party components отдельно. Идеи bounded/state-aware polling, код не переносился |

Итог переноса идей: один активный запрос, разные периоды по demand, bounded buffers,
проверка адреса/service/PID/DID/длины **до** FC, отдельная power архитектура.
Не переносить high-rate polling, unchecked offsets, fixed manufacturer addressing
или ignore-timeout patterns. AIVS и Clever нельзя считать одним проектом без evidence.

## Продукты и дополнительные первичные источники

| Источник | Установлено / полезная идея | Граница |
|---|---|---|
| [Clever OBD2](https://clever-obd2.ru/offer/1367204/) | OLED1–8 параметров, engine DTC read/clear, Wi-Fi config/update, buttons/steering OK, ignition/off claim, VAG/DQ500/Haldex5 | Public firmware source не установлен. Closed product, не источник кода/DID evidence |
| [CANchecked](https://www.canchecked.de/downloads/mfd28-mfd32-mfd32s-mfa28-obd2/) | 11/29-bit250/500, supported PID scan, DSS setup, installation/ignition feed | Product/model-specific; pin1 не универсальное зажигание |
| [ScanGauge III](https://www.scangauge.com/products/productsscangauge-iii/), [manual](https://www.scangauge.com/wp-content/uploads/2022/10/SG3_User_Manual_1.2.pdf) | До9 gauges, touch, XGauge autodetect, read/clear, Wi-Fi updates, sleep settings | Useful setup/sleep UX, внутренние протоколы не выдумываются |
| [Banks iDash](https://official.bankspower.com/owners_manual/97654-idash-1-8-datamonster-and-super-gauge/) | Gauge ecosystem, DataMonster logging variant/manual | Logging зависит от модели; closed firmware |
| [UltraGauge](https://www.ultra-gauge.com/ultragauge/Ultragauge_Features_OBDII.html) | OBD gauges/warnings/trip features | Vehicle-dependent capabilities, не VAG decoder source |
| [FIS-Control](https://www.fis-control.de/handbuch.html), [PolarFIS](https://autopolar2.com/wp-content/uploads/2023/06/Polar_Fis_Guide.pdf) | OEM-display UX, diagnostic/logging/install references | Совместимость legacy cluster не доказывает пригодность для MQB Tiguan |
| [RaceChrono](https://racechrono.com/article/2572), [BLE DIY](https://github.com/aollin/racechrono-ble-diy-device) | Optional telemetry export protocol | Будущий consumer, телефон не acquisition dependency; license check до reuse |
| [RealDash extras](https://github.com/janimm/RealDash-extras) | CAN/OBD definitions/export examples | Root Unlicense; applicability и file-level/third-party terms проверять отдельно |
| [SavvyCAN](https://github.com/collin80/SavvyCAN), [ESP32RET](https://github.com/collin80/ESP32RET) | Development capture/replay/ESP32 telemetry leads, ранее исследованы | Исторический MIT provenance; новый код здесь не импортирован |
| [ESP-IDF5.5.5 TWAI](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/api-reference/peripherals/twai.html), [SN65HVD230](https://www.ti.com/lit/ds/symlink/sn65hvd230.pdf) | TWAI требует transceiver; выбран3.3 V CAN PHY, не CAN FD | Проверять свой модуль/проводку, а не только datasheet чипа |
| CanSensor | Поиск не установил однозначный первичный проект | Все capabilities неизвестны; не подменять похожим названием |

Старые Drive2/Habr/GitLab/forum leads сохранены в
[source register](vag-diagnostic-data-sources.md) и
[ecosystem audit](ecosystem-2026-09-30.md). Недоступный исходник не становится
проверенным от повторения ссылки. Внешние проекты нами не собирались и не
проверялись на автомобиле; licenses перечислены для решения о reuse, не как
юридическое заключение. Root license MiniDash пока не выбрана владельцем.

## Матрица

`Y` — код/документ подтверждает; `P` — частично/зависит от модели/профиля;
`N` — отсутствует в изученном path; `?` — не установлено. MiniDash — firmware
результат с offline tests, не hardware acceptance. AIVS = STM32 OLED, DPF = Wide.

| Возможность | MiniDash | Clever | AIVS | CanSensor | DPF | CANchecked | ScanGauge III |
|---|---|---|---|---|---|---|---|
| Live data | Y OBD | Y | Y | ? | Y | Y | Y |
| Passive CAN | P gated | ? | N | ? | N | P | ? |
| UDS | P gated | ? | P commented | ? | Y | P | ? |
| Standard OBD-II | Y11/500 | ? modes | Y01 | ? | P UDS connector | Y | Y |
| VAG-specific | N enabled | Y claim | N enabled | ? | Y diesel | P | P XGauge |
| Signals/screens | 8 OBD+derived;14 UI IDs | 1–8 visible | PID list | ? | 18 DID entries | profile-dependent | ≤9 visible |
| DTC read | P selected ECU03 | Y engine | N found | ? | N found | ? | Y |
| DTC clear | N | Y | N | ? | N | ? | Y |
| Logging | P RAM96 | ? | ? | ? | N SD unused | P | P trips |
| Graphs | Y bounded history | ? | ? | ? | P | Y | ? |
| Warnings | Y fresh thresholds | Y claim | ? | ? | Y regen | Y | Y |
| Derived metrics | MAP−baro | P maxima | P scaling | ? | P scaling | P | Y trip/fuel |
| Custom screens | Y4 pages | P1–8 | compile-time | ? | compile-time EEZ | Y DSS | Y favorites |
| Widgets | numeric/bar/status/graph | ? | N editor | ? | N runtime editor | Y | P |
| Themes | Y3+palette | ? | N editor | ? | ? | P | P colors |
| Touch | Y | N listed | N listed | ? | Y | P model | Y |
| Buttons | P abstraction | Y | ? | ? | ? | P | ? |
| Steering controls | N mapping | Y supported cars | N found | ? | N found | ? | ? |
| Wi-Fi | Y temporary AP | Y | N found | ? | N bare P4 | ? | Y update |
| BLE | N product protocol | ? | N found | ? | N bare P4 | ? | ? |
| Web config | Y ESP32 | Y | N | ? | N | ? DSS PC | ? |
| OTA | N | Y update | N | ? | ? | P update | Y |
| Profiles | P compiled evidence | P order | fixed code | ? | fixed definitions | Y | Y XGauge |
| Auto detection | P OBD responders | ? | N fixed | ? | N fixed | P PID scan | Y protocol/XGauge |
| Sleep | P QUIET only | Y claim | ? | ? | P hardware | P installation | Y settings |
| Ignition detection | N verified | Y claim | ? | ? | P pin1 assumption | P external feed | P heuristics |
| Power strategy | KCD11 now | zero standby claim | ? | ? | switch/wake option | installation | sleep |
| Standalone | Y code/HW gate | Y claim | Y design | ? | Y design | Y | Y |

## Hidden gold: candidates, не включённые definitions

| Источник | Ценность | Gate |
|---|---|---|
| [opendbc](https://github.com/commaai/opendbc), existing VW MQB JSON | Wheels, brakes, steering, yaw/accel/engine/gear CAN layouts | Existing MIT provenance; OBD visibility, exact ECU, counters/checksum/invalid encoding |
| [OpenHaldex-C6 UDS](https://github.com/Forbes-Automotive/OpenHaldex-C6/blob/86cd553546cfbb812c96e525722597c496250e34/src/OpenHaldexC6_UDS.cpp) | Existing pinned Gen5 diagnostic candidates | MIT/third-party notice retained; не actuator/control code, поколения не взаимозаменяемы |
| [MQB steering controller](https://github.com/adamforbes92/MQBSteeringWheelController), [MQB→PQ](https://github.com/v-ivanyshyn/mqb-steering-wheel-adapter) | LIN/steering electronics, retrofit mapping leads | License/version review и capture именно в нашей точке; не копировать TX injection |
| [MQB-FPA](https://github.com/jilleb/MQB-FPA) | Dataset XML / drive profiles | Study only, никаких vehicle dataset writes |
| [Cluster bench IDs](https://github.com/ALagrandeur/mk7-cluster-bench-controller/blob/main/can_ids/mqb_can_ids.md) | Bench-test methods/mapping leads | Synthetic bench traffic не доказывает OBD visibility Tiguan |
| Existing local VAG/ODIS knowledge | CZPA/DQ500 identities, F405 evidence, DSG/clutch/temp candidates | Proprietary databases остаются локальными; exact endpoint/raw response gate |

Ни один найденный CAN ID/DID не повышен до production trust. TPMS/Haldex/DSG/
wheel/brake/steering требуют capture/reference evidence. Следующий результат
должен быть проверкой на машине, а не ростом числа недоказанных definitions.
