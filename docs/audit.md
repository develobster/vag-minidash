# Аудит 2026-09-20 — v0.8 handoff continuation

Исходная версия: локальный vag-minidash v0.6; Git metadata и AGENTS.md не обнаружены.

> Historical v0.8 handoff record. Current runtime/pre-flight status is in
> `docs/runtime/end-to-end.md` and `docs/runtime/tiguan-first-live.md`.

| Компонент | До изменений | Решение |
|---|---|---|
| app.py / core.py | Tk-интерфейс и симуляция | Сохранены |
| diagnostics.py | FakeECU, сценарии диагностических операций | Сохранён как тестовая модель; не используется новым клиентом |
| transport_lab.py | python-can + ISO-TP + udsoncan, VirtualBus | Сохранён; лабораторная проверка, не VNCI |
| profiles/tiguan_czpa_dq500.json | Незаполненный профиль | Сохранён; отдельная база диагностических RDID добавлена рядом |
| firmware | Каркас ESP32 | Сохранён; на этом этапе не развиваем дисплей |
| minidash_live | v0.7 manual-profile `read` | `probe` with complete UDS exchange JSONL, replay transport, exact fingerprint matching and no raw decoder invention |
| data/ecu_profiles | Отсутствовал | Four authoritative handoff profiles: J623 engine, J743 DQ500, J104 ESC, J492 Haldex |
| data/vehicles | Отсутствовал | Tiguan test vehicle maps aliases to ECU profiles, never CAN addresses |

До этого continuation: 61 тест прошёл, 7 integration tests were skipped because optional transport libraries are not installed. After v0.8 the suite includes catalog/fingerprint/probe/replay tests. Ошибки NRC/timeout/incomplete response в выводе старых тестов ожидаемы.

Проверено: 0x22, DID echo, NRC, bounded pending, stop after timeout/desynchronization; complete request/response JSONL; unknown DID raw handling; no overwrite of trace; exact fingerprint matching that rejects the ODX-vs-ECU ASAM revision mismatch; ISO-TP with a 512-byte VirtualBus response; J2534 ABI/filter/cleanup with a test DLL substitute; JSON API; offline PE parser.

Не проверено: real Windows DLL/PowerShell output, physical CAN, VNCI, vehicle reads, raw scaling, concrete identity DID layouts, D-PDU, DoIP. VirtualBus/replay is not a vehicle test. A configured fingerprint observation can be exactly matched and logged; automatic on-wire fingerprint extraction waits for raw identity captures rather than assuming VAG DID layouts.
