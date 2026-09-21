# Транспортный стенд 0.5

Выполнена интеграция udsoncan → PythonIsoTpConnection → can-isotp → python-can
VirtualBus → ISO-TP симулятора ECU. Это реальные библиотеки с очередями и потоками,
не mock их методов. Автомобиля или аппаратного CAN-интерфейса в этом стенде нет.

## Запуск

Из папки проекта создать отдельное окружение Python 3.10+:

```sh
python -m venv .venv
```

Windows PowerShell:

```powershell
.\.venv\Scripts\python.exe -m pip install -r requirements-transport.txt
.\.venv\Scripts\python.exe transport_lab.py
.\.venv\Scripts\python.exe -m unittest discover -s tests -v
```

Linux/macOS:

```sh
.venv/bin/python -m pip install -r requirements-transport.txt
.venv/bin/python transport_lab.py
.venv/bin/python -m unittest discover -s tests -v
```

Зависимости требуются только новому стенду. Старый GUI и диагностика в памяти
по-прежнему запускаются стандартным Python без pip-пакетов. Новый транспортный стенд
запускается из командной строки; в GUI он пока не встроен.

## Сценарии

```sh
python transport_lab.py --scenario normal
python transport_lab.py --scenario pending
python transport_lab.py --scenario negative
python transport_lab.py --scenario silent
python transport_lab.py --scenario truncated
```

Команды выполняются Python из созданного окружения. Можно активировать окружение
или использовать полный путь, как выше. Нормальный/pending сценарии завершаются кодом 0;
сценарии отказа/таймаута/повреждения — кодом 1 с диагностическим JSON на stdout.
Библиотека также может писать ожидаемые сообщения об ошибках в stderr.

Пример результата успешного запуска при разработке:

```json
{
  "source": "python_can_virtual",
  "hardware_connected": false,
  "socketcan_tested": false,
  "vin": "WVWZZZ00000000000",
  "large_payload_bytes": 180,
  "error": null,
  "frame_counts": {
    "single": 2,
    "first": 2,
    "consecutive": 28,
    "flow_control": 5
  }
}
```

VIN и данные вымышлены. DID F1A0 здесь исключительно адрес тестового наполнителя:
его содержимое нельзя переносить на автомобиль. CAN-ID 700/708 — адреса стенда,
не найденные адреса двигателя или коробки пользователя.

## Как устроена проверка

На каждый запуск создаётся уникальный канал VirtualBus. Три отдельных endpoint:
клиент, ECU и монитор кадров. Никаких аргументов выбора physical backend нет;
ignore_config=True исключает подстановку другого интерфейса из настроек python-can.
Каждый ISO-TP стек владеет своим endpoint, поэтому CanStack не отнимает кадры у
постороннего получателя. Монитор читает отдельную очередь.

Сначала клиент запрашивает VIN (17 байтов), затем тестовые данные (180 байтов).
Ответы требуют ISO-TP сегментации; blocksize=8 и STmin=1 мс. Получатель выдаёт flow control.
Сравнивается полное содержимое длинного ответа, проверяются типы кадров и отсутствие
повторного запроса при NRC 78. Ресурсы закрываются через ExitStack; поток ECU останавливается
до закрытия его стека и шин. Тест проверяет завершение рабочего потока.

В отличие от лаборатории 0.4 здесь используется реальное время выполнения Python.
Этот стенд UDS не опрашивает OBD PID: декодеры и PID discovery 0.4 остаются отдельными.

## Версии и ограничения

Проверено на Python 3.12 с python-can 4.6.1, can-isotp 2.0.7, udsoncan 1.26.1.
Три прямые зависимости закреплены; это не полный lock транзитивных зависимостей.

6 новых интеграционных тестов. Без optional-зависимостей они явно пропускаются,
а не считаются прошедшими. Для полной проверки запускать тесты из transport-окружения.

Linux netlink в среде запрещён: `ip -details link show` вернул Operation not permitted.
Настроить/проверить kernel vcan не удалось; запрет не обходился. VirtualBus не моделирует
электрический уровень, арбитраж, bus-off, загрузку реальной шины и аппаратные timestamps.
Испытания на USB-CAN, VNCI, ESP32-S3 и автомобиле ещё не проведены.

Живой backend — следующий самостоятельный этап. До него нужны проверенные адреса,
разрешённый набор чтений, ограничение частоты, обработка disconnect и тесты потерь/сбоев.
Стенд не реализует кодирование, SecurityAccess, сбросы, прошивку или смену сессии.

## Первичные источники

- [can-isotp implementation](https://can-isotp.readthedocs.io/en/latest/isotp/implementation.html)
- [python-can VirtualBus](https://python-can.readthedocs.io/en/stable/interfaces/virtual.html)
- [udsoncan connections](https://udsoncan.readthedocs.io/en/latest/udsoncan/connection.html)
