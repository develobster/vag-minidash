# CYD 0.4.0 Display Studio: USB-загрузка 20 октября

После подключения пользователем плата обнаружена как CH340 на COM4. Установлено
именно приложение `cyd-0.4.0-studio`, собранное из firmware исходников `03bb9a4`:
1 951 376 bytes, SHA-256
`ab9cf8890e7748e9b8ffc28c13c433fb2c8be70f0d2a8c33dc9ff292311babf9`.
Это последующая аппаратная проверка [Display Studio](../ui/display-studio-2026-10-20.md),
первоначально поставленного без подключённого COM-устройства.

## Сохранение и загрузка

До записи UART вернул display=1, touch=1, calibrated=1, Wi-Fi OFF. Экспортированы
текущие настройки: одна пользовательская страница с шестью окнами, тема 12,
rotation=0, brightness_mode=2, ручной резерв 10%. Полный исходный NerdMiner
4 MiB backup повторно проверен по SHA-256; приложение обновлялось только после
совпадения MAC подключённой платы с его manifest. MAC и содержимое NVS в Git
не включаются.

Дополнительно сохранён текущий MiniDash NVS `0x9000`, длина `0x6000`, и проверен
`verify-flash` при остановленном приложении: digest matched. Этот приватный
резерв хранится отдельно от распространяемого пакета, в
`outputs/studio-hardware-private/nvs-before-update.bin`. Он не заменяет полный
исходный NerdMiner dump и не предназначен для чужой платы.

Первый запуск updater остановился **до записи**: `chip-id` у ESP32 сообщает
штатное предупреждение об отсутствии отдельного chip ID, а Windows PowerShell
с ErrorActionPreference=Stop трактовал stderr как исключение. Проверка заменена
на `read-mac`; тот же критерий совпадения платы сохранён. Повторный запуск
выполнил app-only write на `0x10000` и отдельный `verify-flash` — digest matched,
затем аппаратный reset. Bootloader, partitions, NVS и eFuse не переписывались.

Версионированный helper — `tools/update_cyd.ps1`; он также принимает
`-FirmwareDirectory` с manifest/бинарником и `-BackupDirectory` с verified
исходным dump. В пакете helper по умолчанию читает manifest из своей папки.
`-ValidateOnly` ничего не пишет, но чтение MAC может перезапустить устройство.
Синтаксис финального helper проверен PowerShell parser.

## Фактические результаты

| Проверка на COM4 | Результат |
|---|---|
| Новое приложение / readback | PASS, весь app digest совпал |
| Запуск | display=1, touch=1, calibrated=1, DEMO RPM/speed обновляются |
| Аппаратные self-tests | 8 PASS, 0 FAIL |
| Логика UI | 27 PASS, 0 FAIL |
| Реальный LVGL | cached_text_and_single_selection PASS; font-fit diagnostics/credentials всех 17 тем EN/RU PASS |
| Новые темы | пройдены выбор 14/15/16 и навигация по экранам; пользовательская тема 12 восстановлена |
| CYD переходы | animation=off, timer_absent PASS |
| HTTP на ESP32 | 69 PASS, 0 FAIL: API/парсер, session/revision, Preview/Cancel/Save, NVS, 17 тем, компоновки, повороты |
| Captive portal | внутренние DNS/socket и HTTP redirect проверки PASS |
| Калибровка | calibration blob preserved PASS, calibrated=1 после reset |
| Настройки | common целиком, кроме ожидаемого last_screen, и platform до/после совпали |
| Реальный reset | ROM boot `rst:0x1 (POWERON_RESET)` после RTS; dashboard, Wi-Fi=0, AUTO mode=2 |
| Ошибки | Guru Meditation / watchdog / abort в журнале не обнаружены |

Тест длился 212.2 секунды, включая активную навигацию, HTTP и перезапуск;
это короткий стендовый прогон, не многочасовой soak. Минимум внутренней heap
в тяжёлом совместном HTTP client/server self-test — 6012 bytes. Watermarks:
`http_min=2284`, `selftest_min=2320`. После reset и навигации без AP:
heap=144936, largest=110592, minimum после reset=109320. Минимум после reset
не отменяет более низкий минимум предыдущей HTTP-нагрузки.

Wi-Fi включался UART-действиями через существующие Settings, строку Wi-Fi и
SELECT; self-test не обходит ручной Enable. По завершении ещё раз выполнен
этот путь и **оставлен экран QR**: screen=10, Wi-Fi=1, display/touch/calibrated=1,
heap=88904, largest=86016. AP выключится через штатные 10 минут. После любого
обычного boot он остаётся выключенным. CAN/TWAI/UDS и автомобиль не подключались.

## Границы проверки и следующий шаг

Агент не видел LCD и не касался сенсора. Font-fit и readiness не доказывают
физическую читаемость цветов или точность пяти точек. 42 Chromium проверки
из предыдущей поставки использовали native-backed fixture, а не телефон,
подключённый к новой версии. HTTP regression здесь выполнялся внутри ESP32.
Windows WLAN query отклонён из-за разрешения Location; параметры приватности
Windows и сеть ПК не изменялись. Загрузку большого HTML новым внешним клиентом
и работу JS в captive browser телефона отдельно ещё нужно наблюдать.

На телефоне: QR → Web Configurator (резерв `http://192.168.4.1`) → движение и
размер окна → Preview → Cancel → Save → reboot и повторное ручное включение AP.
Критерии: соответствует LCD, сохранённые страницы/тема/калибровка не потеряны,
нет ошибок browser/network. Далее — длинный USB-прогон AP/навигации/памяти;
риск — ограничения internal SRAM и особенности браузера телефона.

Откат: сначала сохранить старую тему ID 0–13, затем app 0.3.1 на `0x10000`.
Полный возврат NerdMiner — только исходный verified 4 MiB dump на `0x0` по
CYD guide. Новая проверенная копия MiniDash NVS приватна. Санитизированные
результаты аппаратного прогона и отчёт входят в обновлённый установленный пакет;
сырые логи и NVS резерв в него не включаются.
