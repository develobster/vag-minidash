# Аудит CI/CD и доступов MiniDash — 27 октября 2026

Репозиторий: `develobster/vag-minidash`. Ветка по умолчанию:
`integration/standalone`. Срез: `a720abadab9887958736012f78378e4fda13fc81`.
Часовой пояс отчёта: Europe/Minsk (UTC+03:00).

Проверены все 7 запусков, возвращённых актуальным repository Actions API,
28 job-результатов, 8 коммитов после `f73a81a` и 76 изменённых файлов.
Это аудит CI/интеграции и доступных настроек репозитория, не полный аудит
уязвимостей исходного C/Python-кода и не аппаратная приёмка.

## Состояние сборок

| SHA | Run | host | standalone | listen-only | sleep-ili9341 |
|---|---|---|---|---|---|
| `0103d66` | [40506696521](https://github.com/develobster/vag-minidash/actions/runs/40506696521) | success | failure | failure | failure |
| `5e7628b` | [40507537836](https://github.com/develobster/vag-minidash/actions/runs/40507537836) | success | success | success | success |
| `9397342` | [40508391364](https://github.com/develobster/vag-minidash/actions/runs/40508391364) | success | success | success | success |
| `ff5a70b` | [40542662692](https://github.com/develobster/vag-minidash/actions/runs/40542662692) | failure | success | success | success |
| `41853f9` | [40543389157](https://github.com/develobster/vag-minidash/actions/runs/40543389157) | failure | cancelled | cancelled | cancelled |
| `4fa9073` | [40543634519](https://github.com/develobster/vag-minidash/actions/runs/40543634519) | cancelled | cancelled | cancelled | cancelled |
| `dfc1b53` | [40543929493](https://github.com/develobster/vag-minidash/actions/runs/40543929493) | success | success | success | success |

У `41853f9` итог всего run — `cancelled`, но host уже завершился с
`failure`: общий статус скрывает подтверждённый сбой отдельной среды.
Workflow содержит `concurrency.cancel-in-progress: true`; новые push вытесняют
незавершённые прогоны. Отменённые firmware на `41853f9` и все job на
`4fa9073` не являются ни успешно проверенными, ни подтверждённо сломанными.

У текущего HEAD `a720aba` `check_runs.total_count=0`. Коммит содержит
`[skip ci]`; [diff от dfc1b53](https://github.com/develobster/vag-minidash/compare/dfc1b53a4dd3aa351570663cb8dbcd62410c72e8...a720abadab9887958736012f78378e4fda13fc81)
меняет только `docs/development/vag-platforms-drag-ui-2026-10-27.md`.
Проверенным является код `dfc1b53`, а не новый независимый запуск для HEAD.

Последний полный run: 19/19 native CTest, 241 Python-тест (240 pass,
1 platform skip), 10/10 JS, clang-tidy девяти выбранных модулей,
16/16 ASan/UBSan. Новые LVGL-тесты проверили пять экранов в двух ориентациях.

Время последнего полного запуска (wall time из job/step API):

| Среда | Вся job | Основной шаг |
|---|---:|---|
| host | 111 с | Portable C: 19 с; Static C: 11 с; Python/web: 21 с; sanitizers: 4 с |
| standalone | 184 с | Build ESP32-S3: 85 с |
| listen-only | 210 с | Build ESP32-S3: 136 с |
| sleep-ili9341 | 186 с | Build ESP32-S3: 85 с |

Это не чистое время компилятора: job включает подготовку runner/container,
а firmware-шаг — IDF export/configure/build. В рассмотренных падениях нет
признака timeout. Разница host 68 → 111 с между `9397342` и `dfc1b53`
сама по себе не доказывает регрессию производительности: состав native проверок
расширен с 15 до 19 групп, добавлены сборка LVGL и отдельный checkout зависимости.

## Root Cause Analysis

### 1. Xtensa/PSRAM против native atomic contract

Коммит `0103d66`, шаг **Build ESP32-S3 (never flash)**, все три firmware.

[Лог standalone job 122197702094](https://github.com/develobster/vag-minidash/actions/runs/40506696521/job/122197702094),
2026-10-27 15:51:37 Europe/Minsk:

```text
firmware/main/can_pipeline.c:4:1: error: static assertion failed:
"RX requires lock-free 32-bit atomics"
_Static_assert(ATOMIC_INT_LOCK_FREE == 2, ...);
ninja failed with exit code 1
##[error]Process completed with exit code 2.
```

Та же ошибка проверена в listen-only job `122197702295` и sleep job
`122197702442`. Основные шаги падали за 89/107/75 с соответственно.
Host на том же SHA прошёл: native результат не подтверждал переносимость
условия `ATOMIC_INT_LOCK_FREE == 2` на ESP-IDF/Xtensa.

[Исправление 5e7628b](https://github.com/develobster/vag-minidash/compare/0103d66ac4a83a9b27be36746594a2434f852100...5e7628b107baa194ec12c45b3cb320800079d629):
для ESP_PLATFORM+XTENSA проверяется `XCHAL_HAVE_S32C1I`, сохраняется
PSRAM workaround; `rx_ring` и UI mailbox размещены с `DRAM_ATTR`.
[IDF v5.5.5 atomic shim](https://github.com/espressif/esp-idf/blob/v5.5.5/components/newlib/priv_include/esp_stdatomic.h)
подтверждает отдельную dispatch-ветку для слов вне EXTRAM.
Все четыре job на `5e7628b` прошли. Это устранённая регрессия target-сборки.

### 2. LVGL Drag: повторная запись одного label

Коммит `ff5a70b`, host, шаг **Portable C (warnings are errors)**.
C-компиляция завершилась; падение произошло в CTest `automotive-lvgl-render`.

[Job 122324873583](https://github.com/develobster/vag-minidash/actions/runs/40542662692/job/122324873583):

```text
AUTO UI FAIL 50: flushes==before
95% tests passed, 1 tests failed out of 19
##[error]Process completed with exit code 8.
```

Шаг занял 17 с. Все три firmware собраны успешно.
На `41853f9` добавленная диагностика не устранила дефект
([host job 122327430390](https://github.com/develobster/vag-minidash/actions/runs/40543389157/job/122327430390),
шаг 12 с):

```text
AUTO UI 320x240 page=3 startup_flushes=15 settled=1
AUTO UI FAIL 60: flushes==before
```

Причина подтверждается [diff 4fa9073](https://github.com/develobster/vag-minidash/compare/41853f973b3383faa569107de73ef16f78e7e8cc...4fa90730ca6dd7e1cb636c0a0fd0f6ce716186ce):
`md_automotive_update()` сначала записывал общий текст в `p->source`,
затем заменял его строкой Drag. Конечный текст оставался прежним, но две
записи вызывали лишнюю инвалидацию. Общая запись исключена для Drag.

Собственный run `4fa9073` отменён. Исправление подтверждено полным зелёным
run потомка `dfc1b53`. Сборка firmware не обнаруживает эту регрессию
перерисовки; нужен исполняемый host-тест с настоящим LVGL.

### Ошибки, которые не являются падением

В зелёной host-job есть строка `File exists: .../existing.jsonl`.
Она относится к `test_trace_is_never_overwritten`, после неё стоит `ok`,
общий unittest результат — `OK (skipped=1)`. Это ожидаемая проверка отказа
от перезаписи файла; считать её инцидентом по одному слову `error:` нельзя.

Структурированный annotations API проверен, но endpoint
`check-runs/{id}/annotations` отвергнут allowlist коннектора (HTTP 400),
а не вернул пустой список от GitHub. Выше приведены реальные runner-маркеры
`##[error]` и compiler/CTest сообщения из полных job logs. Полнота
отдельного annotations feed не подтверждена.

## Безопасность и изменения доступов

| Проверка | Наблюдение / ограничение |
|---|---|
| Default branch | API: `integration/standalone`, `protected=false`; required status contexts/checks пусты |
| Rulesets | API `rulesets?includes_parents=true` вернул `[]` |
| Workflow permissions | В YAML `contents: read`; лог последнего host подтверждает `Contents: read`, `Metadata: read` |
| CI-инфраструктура за 8 коммитов | Добавлен один workflow `.github/workflows/validation.yml`; в текущем каталоге workflows только он |
| SSH/secrets в изменённом workflow | Нет `ssh-key`, `secrets.*`, PAT/deploy-key setup, `pull_request_target`, `id-token: write` |
| Immutable dependencies | Actions используют major tags; cJSON/LVGL — release tags; IDF container — `espressif/idf:v5.5.5`, без digest |
| Runtime warning | Runner предупреждает: checkout@v4, setup-node@v4, setup-python@v5, upload-artifact@v4 рассчитаны на Node20 и принудительно запускаются на Node24 |
| Deploy/runtime SSH keys, PAT/App tokens | Инвентарь и история добавления/удаления недоступны в текущем коннекторе |
| Изменения collaborator permissions | Исторический административный журнал недоступен; изменение прав не подтверждено и не опровергнуто |

Репозиторий принадлежит personal account `develobster`, не GitHub organization.
Метаданные текущего аккаунта/репозитория не заменяют историю Security Log.
Секретные значения не запрашивались и не сохранялись. Отсутствие SSH/secrets
шагов в workflow **не доказывает**, что ключи или токены не добавлялись через
Settings/API за пределами git. Public events endpoint также не предоставлен
allowlist коннектора. Нельзя выдавать этот результат за «изменений доступов нет».

Существенный обнаруженный пробел — неприкрытая default branch:
успех четырёх job не обеспечен обязательными проверками перед обновлением ветки.
Это наблюдение о текущем состоянии; данных, когда или кем отключалась защита,
нет. Токен CI в последнем run не получил write-доступа по проверенным логам.

## Action items

| Приоритет | Действие | Критерий приёмки |
|---|---|---|
| P1 | Включить защиту/ruleset для `integration/standalone` и требовать host, firmware (standalone), firmware (listen-only), firmware (sleep-ili9341) | Нельзя интегрировать runtime/workflow-изменение без четырёх success на соответствующем SHA; bypass ограничен явно назначенными лицами |
| P1 | Выполнить административную сверку Deploy keys, account SSH keys, PAT/App grants и Security Log | Для каждого нового/удалённого runtime/deploy-ключа известны ID/fingerprint, title, actor, created/deleted time, read-only/write; неожиданные записи расследованы. Не передавать private keys или token values |
| P2 | Обновить перечисленные Node20 Actions на проверенные Node24-совместимые версии, затем закрепить Actions и зависимости immutable SHA/digest | Нет runner Node20 warning; новая pinned матрица полностью зелёная |
| P2 | Сохранить atomic target checks и реальный LVGL dirty-update тест | При возврате unconditional atomic assert падает target; повторная запись Drag source выявляется исполняемым тестом |
| P2 | Регулярный отчёт должен различать job failure, cancelled, skipped, ожидаемые negative-test ошибки и непроверенные admin-данные | Не теряется host failure внутри cancelled run; неизвестные права/ключи не маркируются clean |

Активных сломанных сред на последнем **полностью завершённом** SHA не обнаружено.
Возврат на `0103d66`, `ff5a70b` или `41853f9` не является исправлением:
нужны сохранённые фиксы `5e7628b` и `4fa9073`, проверенные в `dfc1b53`.
Не следует снимать assert или ослаблять LVGL тест ради зелёной сборки.

## Воспроизводимость и границы

Источники: GitHub Actions repository runs/jobs, полные логи шести выбранных job,
GitHub commit compare, branch metadata, rulesets и текущий YAML; primary IDF
shim для объяснения Xtensa dispatch. Машиночитаемый срез:
[ci-access-baseline-2026-10-27.json](ci-access-baseline-2026-10-27.json).

В рамках аудита не запускались повторные сборки, не менялись ключи, токены,
права, rulesets или firmware. Отчёт и baseline сохраняются documentation-only
коммитом с `[skip ci]`; это не новый протестированный runtime SHA.
Открытые аппаратные проверки CAN/sleep/current/temperature/drag остаются за
рамками успешной компиляции.

Повторный аудит использует текущие API-данные и предыдущий срез; фиксированные
исторические SHA здесь не являются заменой проверки нового HEAD. Регулярный
запуск не расширяет права коннектора и не превращает недоступные admin-events
в проверенные данные.
