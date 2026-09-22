# VNCI 6154A: определение интерфейса

## Подтверждено / неизвестно

По реальной сессии 2026-09-22 ODIS Engineering 17.0.1 успешно работал с
VNCI/VAS6154A через Volkswagen D-PDU API. Для Engine подтверждены
`LL_EnginContrModul1UDS`, 500 kbit/s, 11-bit Normal ISO-TP и physical route
`7E0/7E8`. Это подтверждает vehicle-side CAN/ISO-TP route, но не доступность или
совместимость J2534 FunctionLibrary для MiniDash.

На текущем Linux-хосте драйверы VNCI и сам прибор отсутствуют. Доступ сторонней
программы **не определён**. Общий J2534 ISO15765 adapter реализован для complete
UDS PDU и Normal 11-bit addressing; совместимость установленного VNCI-драйвера
с ним ещё не доказана. Raw CAN ISO-TP PCI bytes из ODIS trace нельзя передавать
этому adapter: segmentation/flow control выполняет PassThru ISO15765.

## Следующее действие на Windows

Открыть ODIS на компьютере с установленным рабочим драйвером; запуск скрипта возможен без поездки и не инициирует соединение с автомобилем. Для списка загруженных модулей полезно, чтобы ODIS уже загрузил VCI.

```powershell
powershell -NoProfile -File .\tools\Inspect-Vnci.ps1 -Output .\vnci-windows.json
```

Скрипт не меняет execution policy. Если политика системы запрещает запуск, не обходить её: можно выполнить просмотр реестра/модулей вручную или получить разрешённый способ запуска. Скрипт не открывает VCI и не сканирует сеть. Результат: реестр PassThruSupport 04.04/05.00 и D-PDU, пути/версии загруженных диагностических модулей, сетевые интерфейсы и существующие TCP соединения соответствующих процессов. Пустой список не доказывает отсутствия API; ограничения доступа отражаются в Errors. Отчёт содержит локальные пути и IP.

При наличии Python дополнительно `python -m minidash_live doctor --output vnci-python.json`. Конкретную найденную DLL можно передать через --dll, без её исполнения.

| Найденное свидетельство | Следующий шаг |
|---|---|
| FunctionLibrary + PassThruOpen/Connect/ReadMsgs/WriteMsgs | Проверить API версию, разрядность и принадлежность VNCI; затем явное ISO15765 соединение |
| PDUConstruct/PDUGetModuleIds/PDUCreateComLogicalLink и MVCI root XML | Исследовать D-PDU API, module/resource/protocol и ComParams; реализовать отдельный адаптер |
| Только частная VCI DLL | Изучить поставляемый SDK/документацию и exports; не угадывать ABI |
| IP/TCP соединение | Определить процесс и протокол; само по себе не основание применять DoIP |
| Подтверждённый DoIP endpoint и logical addresses | Добавить DoIP адаптер с routing activation и диагностическими ACK |

Если найден только D-PDU, J2534Transport не будет использован как подмена. Не скачиваем и не заменяем рабочие драйверы ради исследования.

## Источники и чужие реализации

[J2534 developer guide Quantex](https://quantexlab.com/en/develop/j2534.html): последовательность открытия, соединения, фильтра и обмена; [FLOW_CONTROL_FILTER](https://quantexlab.com/en/develop/j2534/pt_start_msgfilt.html): раздельные RX pattern и TX flow-control адреса. Использованы при реализации адаптера, не как свидетельство поддержки VNCI.

[can-isotp implementation](https://can-isotp.readthedocs.io/en/latest/isotp/implementation.html): использован NotifierBasedCanStack для SocketCAN, с ограниченными ожиданиями. [python-doipclient](https://python-doipclient.readthedocs.io/en/latest/): кандидат библиотеки только после подтверждения DoIP. Старый transport_lab.py использует udsoncan; его тесты сохранены. Новый узкий read-only клиент выделяет транспорт и raw/evidence pipeline, не создавая произвольные codec для известных только по названию DID.

Поиск VNCI-специфичных публичных сведений не дал проверяемого SDK/ABI именно установленного у пользователя драйвера. Поэтому окончательное решение зависит от локального отчёта, а не от названия прибора.
