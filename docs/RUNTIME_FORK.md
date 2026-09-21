# Runtime fork — экспериментальная ветка 0.10

> Historical implementation note for the session/scheduler foundation. The
> current integrated runtime and CLI are documented in
> [runtime/end-to-end.md](runtime/end-to-end.md); where this file describes a
> missing integration, the end-to-end document is authoritative.

Основа: аудированная v0.9, отдельная директория `vag-minidash-runtime`.
Это отдельная копия исходников, не GitHub fork: исходный проект без Git metadata.
Основная v0.9 не изменена. Технический аудит описывает исходную версию, а этот
документ — дельту runtime. Автомобиль/адаптер при разработке не подключались.

## Реализовано

- `uds.py`: общий bounded exchange для 0x22, 0x10 01/03, 0x3E 00;
  проверка SID/echo/длины, NRC 0x78 без resend, тайминги ответа 0x50:
  P2 в миллисекундах, P2* в 10 мс. Modern UDS response ровно 6 байтов.
  Нулевые тайминги и значения сверх локального total deadline отвергаются.
  Programming/safety sessions и write/reset/security/routine services запрещены.
  Extended session требует явного `allow_extended=True` и должна быть
  предварительно подтверждена для целевого ECU: смена сессии не является
  пассивным чтением и может менять поведение блока.
- `runtime.EcuSession`: transport factory, reconnect с экспоненциальным
  backoff, повторный identity verification callback, истечение S3, TesterPresent
  с ответом (не suppression). Ошибка транспорта закрывает endpoint.
- `runtime.SignalStore`: раздельные источники UDS/CAN, monotonic time, TTL,
  quality, thread-safe snapshot; stale возвращается с value=null.
- `runtime.Scheduler`: один I/O owner, явные интервалы без универсальных Hz,
  per-ECU cooldown, oldest-due scheduling, отсутствие catch-up burst,
  ON_DEMAND, отключение job после NRC 0x31, counters, bounded run и cleanup.
  FAST/MEDIUM/SLOW пока метки конфигурации: частоту задаёт period,
  приоритет определяется deadline, а не названием класса.
- `passive.PassiveDecoder`: CAN-ID 11/29 bit, Classical CAN DLC, byte offset,
  length, signedness, endian, scale/bias, invalid sentinel, TTL, evidence gate.
  Error/RTR frames не декодируются. receive_once интегрируется с python-can Bus
  и не вызывает send. Аппаратный listen-only должен быть настроен отдельно.
- `runtime_demo`: исполняемый offline пример, связывающий session → scheduler
  → store и passive decoder → тот же store. Все байты синтетические.

## Запуск

Из корня отдельной копии:

```sh
python -m unittest discover -s tests -v
python -m minidash_live.runtime_demo
```

78 тестов с transport dependencies, включая прежние VirtualBus integration
tests. Без optional transport dependencies часть старых тестов пропускается.
Новый runtime использует стандартную библиотеку Python.

Существующий `python -m minidash_live probe` сохранён как bounded исследовательский
reader. Новый `discover` / `signals` / `watch` path строит `EcuSession`,
`PollJob` и `Scheduler` через acquisition planner; `probe` намеренно остаётся
отдельной низкоуровневой исследовательской командой.
Factory должна каждый раз возвращать новый чистый PDU transport и освобождать
частично созданные ресурсы при ошибке конструктора. Проверенный SocketCAN/J2534
adapter может быть такой фабрикой; автоматического выбора оборудования нет.

`verify(client)` вызывается на каждом подключении до session control. Это
точка интеграции настоящего считывания identity, не готовый VAG identity decoder.
Без callback, подтвердившего identity, scheduler не запускает decoder. Никогда
не копировать `verify=lambda client: True` из synthetic demo в автомобильный config.
PassiveDecoder.profile_verified также должен происходить из доказанной привязки
профиля к машине, а не из имени автомобиля.

## Ограничения и следующий этап

- Cooperative scheduler: один молчащий ECU блокирует tick до total timeout,
  но не завершает процесс и не закрывает другие sessions. Для строгих deadlines
  нужны отдельные workers на независимые физические шины и общий bus coordinator.
- S3 и keepalive задаёт оператор по ECU evidence; defaults не доказывают
  пригодность к конкретному автомобилю. Длительный exchange другого ECU может
  привести к expiry: следующий вызов закрывает session и открывает новую.
- Каждый transport.send должен соблюдать timeout; зависшую native DLL этот
  Python layer принудительно остановить не может. Нужен process isolation.
- Local total deadline остаётся верхней границей даже при длинном server P2*.
  Транспортный запас сверх P2 отдельно не реализован: перед hardware validation
  нужно измерить задержки выбранного адаптера и согласовать timing policy.
- NRC 0x31 отключает job до явного изменения конфигурации; NRC 0x21/0x22
  повторяются только на следующем плановом period, не немедленно. Сложной
  capability rediscovery и восстановления disabled jobs после ignition пока нет.
- VAG provider содержит mappings для внешнего openDBC и exact ECU DID/name
  associations. Сам `vw_mqb.dbc` не vendored; bitfield/checksum coverage зависит
  от подключённого DBC и видимости frames через gateway.
- Store сохраняет implementations отдельно; `SourceArbitrator` выбирает live
  source и делает CAN-to-UDS fallback. Старый probe API не является runtime API.
- Frame timestamp для freshness — время подачи в store; задержанные записи
  replay нельзя считать свежими live samples. Live receiver обязан ограничивать
  backlog и учитывать hardware timestamps перед publication.
- ESP32 firmware остаётся исходным каркасом. Эта ветка реализует PC runtime,
  но не USB bridge, физическую CAN-проверку или готовый автомобильный прибор.

## Безопасная последовательность интеграции

1. Подключить реальный свежий transport factory с явными CAN ID и bitrate.
2. Реализовать verify callback на основании подтверждённых identity RDID/layout.
3. На стенде проверить session/keepalive/reopen и отсутствие старых PDU.
4. Подключить decoder callbacks только с regression raw evidence.
5. Задать периоды/TTL по измерениям latency и динамике сигнала.
6. Добавить hardware frame trace, bus budget и GUI/API adapter.
7. Только затем тест на автомобиле с ограниченным набором RDID.

Протокольная справка: [udsoncan services](https://udsoncan.readthedocs.io/en/latest/udsoncan/services.html).
