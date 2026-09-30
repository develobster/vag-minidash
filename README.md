# Экспериментальный runtime fork 0.10 от аудированной v0.9

Первый дисплейный прототип ESP32-S3 (ST7789/ILI9341 backends), граница host/firmware и проверка
проводки описаны в [display-prototype.md](docs/hardware/display-prototype.md). Текущий статус
embedded UI и следующие шаги: [docs/ui/status.md](docs/ui/status.md).
Опциональная передача канонических сигналов из host runtime в ESP32 описана в
[docs/ui/host-bridge.md](docs/ui/host-bridge.md).

## Research integration and offline CAN replay

Актуальная UI-ветка объединена с существующей MQB knowledge-веткой без замены UI.
Исследование и ограничения источников: [ecosystem-2026-09-30.md](docs/research/ecosystem-2026-09-30.md).
Записи `can_capture.py` теперь воспроизводятся через существующий decoder/store/arbitrator
командой `python -m minidash_live replay-can`; никаких CAN/VCI/UDS соединений
она не открывает. Контракт и примеры: [can-replay.md](docs/runtime/can-replay.md).

## Diagnostic data integration (feature branch)

Добавлен минимальный data-driven Signal Registry, optional `cantools` adapter
для внешнего `vw_mqb.dbc`, provenance/applicability metadata и консервативный
импорт простых scalar MWB records из локально извлечённого MCD JSON. Чужие DBC
и proprietary ODIS database в repository не включаются. Исследование источников,
лицензий, известных MQB signals и границ доверия:
[vag-diagnostic-data-sources.md](docs/research/vag-diagnostic-data-sources.md).

Optional DBC dependency: `pip install -r requirements-data.txt`.

Core identity/applicability, canonical/raw namespaces, provider boundary and
source arbitration are documented in
[vehicle-knowledge.md](docs/architecture/vehicle-knowledge.md). Tiguan/MQB/VAG
are reference vehicle/dataset/provider respectively, not core constraints.

The integrated read-only runtime, `discover` / `signals` / `watch` commands,
VirtualBus proof and first-car procedure are documented in
[end-to-end.md](docs/runtime/end-to-end.md).

The evidence-backed VNCI/Tiguan Engine route and F190-first garage pre-flight
are documented in [tiguan-first-live.md](docs/runtime/tiguan-first-live.md).
F405 coolant decoding is now vehicle-tested for one exact Engine identity; it
remains unavailable until MiniDash itself observes that identity.

Session management (0x10/0x3E), reconnect/backoff, polling scheduler,
state/freshness и passive CAN decoding уже интегрированы. История foundation и
его ограничения: [RUNTIME_FORK.md](docs/RUNTIME_FORK.md).
Offline запуск: `python -m minidash_live.runtime_demo`.
Реальный Engine endpoint и один F405 codec подтверждены ODIS/physical trace;
физический VNCI J2534 path MiniDash и VAG passive CAN всё ещё не проверены.

Ниже сохранено описание исходной версии и её исследовательского probe.

# Обновление v0.8: evidence-first UDS probe и ECU catalog

Независимый пакет `minidash_live` теперь содержит bounded read-only `probe`: UDS `0x22`, SocketCAN/ISO-TP, общий Windows J2534 адаптер, offline replay, complete raw JSONL exchanges и loopback API/UI. Каталог `data/` фиксирует подтверждённые fingerprints J623/J743/J104/J492 и RDID test vehicle; raw decoder для proprietary DID пока нигде не придуман. Совместимость VNCI ещё требует определения установленного API. Старые симулятор, лаборатория и firmware сохранены.

- [Запуск PC backend](docs/pc-backend.md)
- [Определение интерфейса VNCI](docs/vnci.md)
- [DID и журнал исследования](docs/research.md)
- [Аудит и границы проверки](docs/audit.md)
- [Глубокий технический аудит и roadmap](docs/TECHNICAL_AUDIT.md)

Быстрый offline smoke test: `python -m unittest discover -s tests -v` (66 тестов в
аудированной рабочей копии). Реальный первый шаг зависит от выбранного физического
транспорта. `python -m minidash_live doctor --output vnci-python.json` только исследует
Windows-драйверы и не открывает VCI/автомобиль; конечному MiniDash VNCI не требуется.

Ниже — документация предыдущих этапов; утверждения об отсутствии live backend относятся к той версии. Реальное чтение автомобиля новой версией пока не проверено.

---

# VAG MiniDash — прототип 0.6

Бортовой монитор VAG с планируемыми диагностикой и кодированием.
Первый профиль: Tiguan II / CZPA / DQ500. Архитектура рассчитана на дополнительные автомобили.

## Новое в 0.5: интеграция транспортных библиотек

`transport_lab.py` — UDS через can-isotp и python-can VirtualBus, с многофреймовыми
ответами и flow control. Пять сценариев и шесть интеграционных тестов.
Установка optional-зависимостей и команды Windows/Linux/macOS: TRANSPORT.md.
Проверка kernel vcan заблокирована средой; VirtualBus и реальная шина не равнозначны.

## Решение по аппаратной платформе 0.6

ESP32-P4 и MIPI-DSI AMOLED исключены из базовой версии как избыточные. Разработка разделена:

1. **V1:** Windows/Linux-приложение с реальным диагностическим адаптером — проверяем блоки,
   DID, DTC и безопасные read-only операции.
2. **V2:** автономный ESP32-S3, внешний CAN-трансивер, небольшой SPI/I²C OLED и кнопки —
   компактный экран в духе Clever.
3. **Premium:** цветной MIPI-дисплей только при появлении требований к сложной графике или touch.

Такой порядок не привязывает протокол к конкретному экрану и позволяет сначала получить
реальные данные автомобиля.

## Новое в 0.4: диагностическая лаборатория

`python diagnostics.py` — UDS-чтение вымышленного VIN и определение/чтение поддерживаемых
OBD-параметров у FakeECU. В GUI: **Инструменты → Диагностическая лаборатория**.
Семь сценариев: норма, задержка, бесконечный pending, молчание, отказ, неверный и обрезанный ответ.
Экспорт отчёта JSON из окна лаборатории. Подробности: DIAGNOSTICS.md.

Это полный PDU-обмен в памяти, без CAN/ISO-TP или соединения с автомобилем.

## Новое в 0.3: универсальный режим

Вкладка «Каталог VAG»: 12 исследовательских карточек с требованиями и причинами блокировки.
Можно импортировать инвентаризацию по `profiles/vehicle-template.json`. Это ручной JSON,
не импорт ODIS и не подключение к автомобилю. Ни одна карточка не разрешает реальную запись.
Вкладка «Сравнение HEX»: офлайн-различия кодировок по байтам и битам.

```sh
python catalog.py
python catalog.py --vehicle profiles/vehicle-template.json
```

Сравнение десяти проектов/источников, границы универсального режима и план: RESEARCH.md.
GUI-приборы и учебное кодирование по-прежнему работают отдельно на симуляторе.

## Запуск сейчас

Установить Python 3.10+ с Tk. Распаковать архив, открыть терминал в папке проекта:

```sh
python app.py
```

На macOS/Linux команда может называться `python3`. Проверка Tk: `python -m tkinter`.
Windows: Tk обычно входит в установщик Python; Linux может требовать пакет python3-tk.
Зависимости pip отсутствуют. Интерфейс запускается на компьютере, мобильной сборки пока нет.

1. «Приборы» → «Начать поездку»: циклическая симуляция разгона/торможения.
2. «Чёрный ящик»: событие D2→D1; экспорт JSON с отметкой simulation.
3. Остановить поездку → «Кодирование»: предпросмотр, запись учебного значения, readback.
4. «Вернуть последнее изменение»: отдельная проверяемая операция.
5. Журнал кодирования: `~/VAG-MiniDash/coding-journal.jsonl`.

## Что реализовано

- Настольный Tk-интерфейс: приборы, кодирование, чёрный ящик, статус подключения.
- Симулятор, кольцевой буфер на 300 отсчётов, 50 отсчётов после события.
- Кодирование симулятора: проверка исходного значения/идентичности, остановки и напряжения,
  сохранение исходного значения до записи, проверка результата, возврат последнего изменения.
- 41 базовый тест и 6 интеграционных тестов с optional-зависимостями; шаблоны профилей; минимальная точка входа ESP-IDF.

## Границы текущей версии

Добавлен отдельный Linux SocketCAN-регистратор (см. HARDWARE.md), ещё не проверенный на оборудовании.
Нет ISO-TP, UDS, считывания/сброса DTC, LVGL, драйвера дисплея,
связи с VNCI, прошивки ECU/TCU или реального кодирования. Показатели генерируются.
Учебные названия функций не задают номера блоков, байты, биты или DID реального VAG.
Профиль JSON пока документирует будущий формат и не загружается приложением.
При перезапуске учебные значения сбрасываются; журнал сохраняется, возврат через UI — только в текущем сеансе.
ESP-IDF-каркас не собирался в этом окружении и не является готовой прошивкой устройства.
Интервалы буфера номинальные при 10 Гц; Tk не обеспечивает точного времени.

## Проверка

```sh
python -m unittest discover -s tests -v
```

## Следующий этап

1. Через ODIS снять инвентаризацию блоков 01/02/09/42/52/17/19: HW/SW, ASAM/dataset,
   кодировки и доступные идентификаторы.
2. Импортировать отчёт в профиль Tiguan и подключить реальный транспорт через отдельный backend.
3. Проверить read-only: идентификация, DTC, измерительные данные и логирование.
4. Подтвердить одну безопасную функцию с preview/readback/rollback.
5. Только после этого выбрать точную OLED-панель, питание и корпус для ESP32-S3.

См. ARCHITECTURE.md для требований к реальной записи и предварительной комплектации.

## Новое в 0.2

`can_capture.py` — приём Classical CAN и статистика JSONL на Linux.
Симулятор GUI запускается отдельно и остаётся без связи с автомобилем.
Аппаратные решения, статус проверки и команды стенда: HARDWARE.md.
