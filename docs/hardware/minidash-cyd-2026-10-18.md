# MiniDash CYD — отчёт 18–19 октября 2026

CYD-сборка загружена на реальный ESP32 через COM4. Пользователь подтвердил
читаемое изображение после исправления зеркальности и базовый отклик touch.
Аппаратный HTTP прогон: 63 PASS / 0 FAIL; CYD и S3 собираются отдельно.
Внешний телефонный QR/captive тест и точная калибровка углов пока не подтверждены,
поэтому полный критерий физической приёмки ещё не закрыт. CAN/UDS отключены.

## Ветка и аудит

Новая ветка `feature/hardware-cyd-esp32` создана в изолированном Git worktree
от `origin/integration/standalone` (`b599fbc0a6fb40abe23072a0ec2258cb68253029`).
`main` (`de7f071a2d7e574970614527d4397a274fa46813`) не менялся; слияния не выполнялись.
Явное указание пользователя о feature-ветке имеет приоритет над общим правилом
AGENTS.md продолжать routine work на integration. Требования документации и push
сохранены, публикация относится только к новой feature-ветке.

Проверены все опубликованные remote refs и история display prototype → production
UI → touch zones/Web → standalone acquisition → analytics → startup/stack fixes.
Все перечисленные heads уже входят в выбранный baseline (0 новых коммитов справа).
Исторические ветки сохранены, дополнительных merge для CYD не требовалось.

| Remote ref на момент аудита | Head | Коммитов вне baseline |
|---|---|---:|
| origin | `b599fbc0a6fb` | 0 |
| origin/feature/diagnostic-data-import | `54d0ea538751` | 0 |
| origin/feature/engine-analytics | `da1e6b7ba489` | 0 |
| origin/feature/esp32-display-prototype | `dc16a907f342` | 0 |
| origin/feature/mqb-knowledge-import | `0b3121dfbf5e` | 0 |
| origin/feature/production-ui | `00c6317c08b0` | 0 |
| origin/feature/real-car-validation | `bf7e8ba67b10` | 0 |
| origin/feature/repository-consolidation | `3e5c6ca90ca7` | 0 |
| origin/feature/research-replay-reliability | `22a346bee87f` | 0 |
| origin/feature/standalone-acquisition | `ab1c09cc9ba9` | 0 |
| origin/feature/ui-ux-touch-zones | `6f436dd29810` | 0 |
| origin/fix/can-display-startup | `da1e6b7ba489` | 0 |
| origin/integration/standalone | `b599fbc0a6fb` | 0 |
| origin/main | `de7f071a2d7e` | 0 |
| origin/runtime/session-scheduler | `e62ca59bf2ba` | 0 |

Области переиспользования:

| Область | Существующая реализация и решение |
|---|---|
| UI / LVGL | firmware/main/ui_screens.c, ui_core.c, ui_theme.c, ui_model.c — тот же renderer и bindings |
| Display / touch | display_manager.c, ili9341_backend.c, xpt2046_touch.c, ui_touch_map.c/ui_touch_nvs.c — профиль ориентации и отдельный SPI touch |
| Диагностика | diagnostic_transport.c/acquisition.c/vehicle_app.c — сохранены; CAN на CYD выключен |
| Аналитика | derived_engine/window_statistics/performance_timer — S3 без изменения семантики; CYD большой engine buffer не выделяет |
| Web | ui_web.c/webui.html — тот же редактор, session/revision/schema/NVS; для CYD добавлены DNS/portal/QR |
| Симуляторы | app.py/diagnostics.py (desktop), minidash_live и fixtures/replay сохранены; новый маленький deterministic source подаёт DEMO в существующий store |

## Реальное устройство и backup

COM4, CH340 (VID:PID 1A86:7523), ESP32-D0WD-V3 revision 3.1, 40 MHz crystal,
flash 4 МБ (JEDEC manufacturer c4/device 6016), PSRAM отсутствует.
Инструменты: esptool 5.4.0, pyserial, ESP-IDF 5.5.5 и установленный Xtensa GCC 14.2.0.
eFuse summary прочитан без записи: secure boot выключен, FLASH_CRYPT_CNT=0.
Команда get-security-info на этом ESP32 не поддерживается; её неудача не выдаётся
за успешную проверку. eFuse не изменялись.

Полный оригинальный образ **сохранён до первой записи MiniDash**, включая NVS,
раздел приложения, SPIFFS, coredump и пустые области: 4 194 304 байта.
Файл `nerdminer-original-stable-4mb.bin`, SHA256:
`904a38429a8b2b0863cab48ddafa06c15b3312ea2622389ae6962ed4e9412bc9`.
После чтения с `--after no-reset` выполнен `verify-flash` без запуска майнера:
**Verification successful (digest matched)** для всех 4 МБ.
Локальные manifest/SHA256 и журнал проверки лежат рядом с дампом в поставке.
Дамп и приватные настройки не добавлены в Git.

Первый быстрый проход на 460800 оборвался; его частичный результат не принят.
Два последующих полных чтения 115200 совпали по app/SPIFFS, но майнер между
перезапусками изменил 74 байта в NVS (сектор 0xa000). Поэтому выполнен финальный
стабильный проход и проверка flash digest при остановленном приложении.
В исходном boot log есть `NerdMiner v2 starting` и NerdMinerAP; в образе найдена
идентификация семейства ESP32-2432S028. Прошивка не считалась пустой платой.

ILI9341 read D3 на данной плате возвращает 00 00 00 00: это **не положительный
chip-ID**. Пользователь подтвердил рабочее изображение с ILI9341 backend и
реакцию на касания, но отметил зеркальность первой сборки. Отдельная CYD
MADCTL table исправляет MY для landscape; S3 table сохраняется. Точная маркировка
LCD/touch без осмотра/рабочего ID не считается установленной. Отдельный SPI
XPT2046 соответствует работающему профилю, IRQ аппаратно не проверялся.

## Проверки

Автоматический полный offline gate: `tools/verify_repository.py`, лог/summary
в поставке. 227 Python tests, 1 Windows AF_CAN skip; transport dependencies
установлены в отдельном venv. CTest: 11/11 (включая CYD simulation/DNS и обе
таблицы ориентации). VirtualBus, deterministic analytics replay, JSON/editor
contract и Web UI logic: PASS. Это программные проверки, не приёмка автомобиля.

ESP-IDF build: отдельные ESP32 CYD и ESP32-S3 standalone, GCC 14.2.0 / IDF 5.5.5.
Проверка стека встроена в обе сборки: UI controller 8192, nested save 5792 +
reserve 1024 <= 8192. Исходные S3 configs/partitions/dependencies.lock сохранены.
Физическая S3-плата в этом сеансе не прошивалась и не проверялась.

Начальная аппаратная сборка запустила UI 320×240, touch reader и DEMO RPM/speed;
27 portable UI tests и 8 chip/flash/internal-heap/UART tests на ESP32 прошли.
Все сцены одновременно занимали почти весь heap (~33 КБ остатка), actual-LVGL
view test не прошёл, после него отмечены watchdog warnings. Это не принято как
стабильный результат. CYD переведён на единственную видимую сцену, 240 MHz CPU,
20 MHz SPI, 5 Hz model updates; S3 сохраняет прежний lifetime сцен.

Финальный аппаратный HTTP прогон (`cyd-acceptance-http.log`): **63 PASS / 0 FAIL**.
DNS UDP socket и captive redirect, config roundtrip, registry/presets/device,
сессия/входная валидация/revision conflict, четыре страницы и типы виджетов,
Preview/Cancel/local takeover, Save/NVS/manual gauges, 11 тем, 8 layouts,
4 поворота, восстановление исходной конфигурации и сохранение calibration blob.
Нет watchdog/assert. Boot: display=1, touch=1, calibrated=0, Wi-Fi=0,
DEMO RPM/speed меняются; AP включён через те же Settings actions, QR READY
41-byte Wi-Fi payload, AP=192.168.4.1. Свободная heap boot 145976 bytes,
AP/QR 89608, минимум тяжёлого совместного HTTP теста 7272. Watermarks:
HTTP stack spare 2432 bytes, selftest spare 2324 bytes при stacks 8192.
Это реальные ESP32 HTTP/LVGL проверки, не внешний телефонный тест.

Пользователь повторно подтвердил нормальную читаемость после исправления зеркальности.
Обычный boot после lazy scenes: ~138 КБ свободной heap, 27/27 UI core,
actual-LVGL view test PASS, 8/8 hardware self-tests, без watchdog в повторном
boot/self-test прогоне. Первая попытка полного HTTP self-test обнаружила
недостаток RAM из-за дополнительных буферов тест-клиента: DNS/redirect 2 PASS,
GET config/parse 2 FAIL. Это промежуточный результат, не финальная приёмка.
После него удалён неиспользуемый CYD acquisition/trace engine (8 КБ), receive
buffer теста выделяется только по необходимости, catalogue API передаёт
те же записи потоково. Также CYD UART parser сохраняет фрагменты до newline;
прежний UART VFS иногда отдавал неполную строку. S3 console не изменена.


## Использование, схема и оставшиеся проверки

[Инструкция, GPIO, питание/CAN и восстановление](minidash-cyd.md).
[Отдельная схема питания/CAN](minidash-cyd-wiring.svg).
CAN-пины 22/27 — кандидаты для согласования и прозвонки, CAN запрещён в CYD
Kconfig. SN65HVD230 и внешнее питание не подключались/не измерялись этим агентом;
CAN передача и OBD/UDS к автомобилю не активировались.

Windows запрещает WLAN query/scan из-за выключенного разрешения геолокации
(`netsh wlan show interfaces`: Access denied). Настройки приватности Windows
не менялись. Поэтому проверка внешнего Wi-Fi клиента и телефона требует
пользователя; внутренние HTTP/DNS проверки на ESP32 отмечаются отдельно.

Читаемость текста после исправления зеркальности и базовый отклик touch
подтверждены пользователем. Окончательная физическая приёмка ещё требует
цветов, пяти точек touch, краёв/hold/double-tap, Wi-Fi QR сканирования,
DHCP/captive popup телефона, Web Save/Cancel, reboot и длительной проверки питания.
Проведение console/native проверок не заменяет эти наблюдения.

Приоритеты после поставки:

1. Закрыть физическую приёмку пяти точек калибровки (ориентация подтверждена) (LCD/touch,
   критерий — все углы/центр совпадают после reboot). Нужен доступ пользователя
   к плате; риск — отличия touch calibration/ревизии панели.
2. Внешний телефон: QR → WPA2 AP → DHCP/DNS → captive popup/192.168.4.1 →
   Preview/Save/Cancel → выключение AP. Риск — особенности ОС/HTTPS probes;
   fallback адрес обязателен даже при работающем popup.
3. Длительная стабильность с навигацией/всеми темами, AP on/off и памятью;
   отдельно реальные токи и температура. Критерий — нет reset/WDT/heap trend.
4. CAN только после согласования схемы, прозвонки 22/27 и измерения терминатора
   SN65HVD230; сначала USB isolated bench/listen-only, затем отдельное разрешение
   автомобиля. Не переносить CAN preset S3 на CYD.
5. Производная 60 s аналитика CYD потребует отдельного обоснованного memory budget;
   не уменьшать окна молча и не отображать unavailable как zero.

## Дополнительные проверки 19 октября

Обычный Component Manager ESP-IDF успешно выполнил reconfigure CYD с отдельным
`dependencies.cyd.lock`; S3 lock остался неизменённым. После устранения
одновременных JSON/LVGL аллокаций аппаратный HTTP прогон дал 55 PASS / 8 FAIL:
перезагрузок/WDT нет, все темы/layouts/повороты проходят, но четырёхстраничные
ответы ещё превышали свободную память. Это промежуточная отклонённая сборка.
Следующее исправление переиспользует статические имена JSON-полей и исключает
финальное копирование ответа при уменьшении буфера cJSON. Формат API сохранён.
Native CTest после него: 11/11 PASS; отдельные CYD и S3 build PASS.

Проверка следующего промежуточного варианта выявила, что статических ключей
недостаточно (52 PASS / 11 FAIL, minimum heap 16 bytes), поэтому этот образ
также отклонён. Финальный подход использует общий `md_json_stream`: обход
существующего cJSON-дерева, чанки не более 512 bytes, штатное cJSON escaping,
без полной текстовой копии ответа. Native контракт сравнивает вывод с обычным
cJSON для четырёх страниц, UTF-8, escaped key/value, длинной строки и отказа
callback; все 11 CTest PASS. CYD custom-page rebuild освобождает старую сцену
перед созданием новой; S3 сохраняет прежнюю последовательность.

Потоковая версия провела первый четырёхстраничный Preview, но повторная
валидация в совместно работающем тест-клиенте всё ещё исчерпывала heap
(55 PASS / 8 FAIL). Проверены реальные compiler stack-usage: parser 3104 bytes,
mutation handler 128, HTTP client wrapper 304, web selftest frame 224. На CYD
HTTP и selftest stacks снижены с 12288 до 8192 bytes каждый; S3 не меняется.
Встроенные watermarks фиксируют фактический остаток обоих стеков.

Финальная повторная локальная проверка (`cyd-nvs-reboot.log`): 8/8 chip/flash/
internal heap/UART/watchdog self-tests, 27/27 UI core, actual LVGL view test PASS.
Две программные перезагрузки после сохранения яркости: выгруженные JSON
показывают `[71, 71, 70, 70]` (до/после каждого reboot), rotation=0, страницы
сохранены. После reboot Wi-Fi=0, загружается dashboard без setup wizard.

Последовательная консоль: `sta` + задержка + `tus\n` обработаны как один status;
строка 200 bytes отвергнута один раз с LINE_TOO_LONG, следующий status выполнен.
Два AP on/off цикла завершились с работающими DEMO данными и Wi-Fi=0 после
выключения; свободная память после выключения 135572 и 135532 bytes. Разница
40 bytes в коротком тесте не считается доказательством утечки либо её отсутствия.
После третьего ручного включения оставлен QR-экран для телефона.

Короткий soak с включённым AP и QR (`cyd-cycles-soak.log`): 120 секунд,
7 выборок, heap **89408 bytes во всех выборках**, largest block 73728,
DEMO RPM/speed продолжают меняться. Нет reset/WDT/assert. Это не длительный
термотест и не измерение тока/потребления. Проверка QR/страницы телефоном
запрошена у пользователя; к моменту поставки подтверждения ещё нет.
