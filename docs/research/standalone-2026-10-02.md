# Standalone MiniDash — инженерный отчёт, 2026-10-02

Рабочий baseline: `3e5c6ca90ca7a9e0fa421c9b5d41e6fcda92b38a`.
Ветка: `feature/standalone-acquisition`. Основная ветка не изменяется.

## Контрольная точка реализации

Исходный `app_main()` запускал дисплей и USB-консоль. Данных CAN внутри
прошивки не было: acquisition, identity, providers и arbitration работали
в Python на ПК. Автономность была фундаментальным блокером.

Добавлены собственные portable C ISO-TP RX, OBD discovery/capabilities/VIN,
bounded scheduler/store/arbitration, manifest-gated UDS и passive decoder,
TWAI task, demand от текущего экрана, локальное чтение stored emissions DTC.
Ни один непроверенный VAG profile не включён. Производственный путь не использует
Python, телефон, ODIS, VNCI, ELM/STN или внешний dongle.

Baseline повторно проверен: Python 164 теста, CTest 2/2, Web PASS.
На этой контрольной точке: CTest 3/3, включая 8 сценариев acquisition и
50 000 malformed frames; ESP-IDF 5.5.5 standalone build PASS.
Сборка baseline требует исправления RX BA window: 12 при static RX buffers=6.
Это исправление измеряется отдельно от нового runtime.

Тестов на физической плате и автомобиле не было. Quiet прекращает TX и
включает hardware listen-only; это НЕ deep sleep и НЕ гарантия отсутствия
разряда аккумулятора. До проверки схемы питания нужен физический выключатель.

Сессия выполнения была пересоздана. Репозиторий восстановлен через GitHub API,
проверены SHA всех 181 blobs, корневой tree и исходный commit; повторно запущены
тесты и сборки. Никакие чужие локальные изменения не включены.

Полная матрица источников, safety review и итоговые результаты добавляются
следующей контрольной точкой до завершения задачи.
