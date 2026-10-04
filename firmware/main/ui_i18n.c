#include "ui_i18n.h"

#include <string.h>

typedef struct {
    const char *english;
    const char *russian;
} translation_t;

static const translation_t s_translations[] = {
    {"DRIVE", "ДОРОГА"},
    {"SPORT", "СПОРТ"},
    {"MINIMAL", "МИНИМУМ"},
    {"CUSTOM", "СВОЙ"},
    {"PERFORMANCE", "ДИНАМИКА"},
    {"TEMPERATURES", "ТЕМПЕРАТУРЫ"},
    {"DRIVETRAIN", "ТРАНСМИССИЯ"},
    {"SERVICE STATUS", "СТАТУС СИСТЕМЫ"},
    {"SETTINGS", "НАСТРОЙКИ"},
    {"FOCUS", "ПАРАМЕТР"},
    {"CALIBRATE TOUCH", "КАЛИБРОВКА КАСАНИЯ"},
    {"EDGES: PREV / NEXT  |  HOLD SETTINGS", "КРАЯ: НАЗАД / ДАЛЕЕ | УДЕРЖ. НАСТР."},
    {"TAP NEXT | DOUBLE PREV | HOLD SETTINGS", "КОСНИТЕСЬ: ДАЛЕЕ | 2 РАЗА: НАЗАД"},
    {"READ ONLY  |  STALE VALUES HIDDEN", "ТОЛЬКО ЧТЕНИЕ | УСТАР. ДАННЫЕ СКРЫТЫ"},
    {"MISSING / STALE VALUES HIDDEN", "НЕТ ДАННЫХ | УСТАРЕВШИЕ СКРЫТЫ"},
    {"READINGS REQUIRE A DATA PROVIDER", "НУЖЕН ИСТОЧНИК ДАННЫХ"},
    {"N/A = SOURCE NOT PROVIDED", "Н/Д = ИСТОЧНИК НЕ ПОДКЛЮЧЕН"},
    {"TAP ROW TO SELECT", "КОСНИТЕСЬ СТРОКИ ДЛЯ ВЫБОРА"},
    {"TAP NEXT | DOUBLE CHANGE | HOLD BACK", "КОСНИТЕСЬ: ДАЛЕЕ | 2 РАЗА: ИЗМЕНИТЬ"},
    {"EDGES: PREV / NEXT  |  HOLD SETTINGS", "КРАЯ: НАЗАД / ДАЛЕЕ | УДЕРЖ. НАСТР."},
    {"ENGINE SPEED", "ОБОРОТЫ ДВИГАТЕЛЯ"},
    {"VEHICLE SPEED", "СКОРОСТЬ АВТО"},
    {"SPEED", "СКОРОСТЬ"},
    {"COOLANT", "ОХЛАЖД. ЖИДКОСТЬ"},
    {"ENGINE OIL", "МАСЛО ДВИГАТЕЛЯ"},
    {"ENGINE LOAD", "НАГРУЗКА ДВИГАТЕЛЯ"},
    {"CHARGE PRESSURE", "ДАВЛЕНИЕ НАДДУВА"},
    {"AMBIENT PRESSURE", "АТМОСФЕРНОЕ ДАВЛ."},
    {"BOOST", "НАДДУВ"},
    {"TRANSMISSION OIL", "МАСЛО КПП"},
    {"GEAR", "ПЕРЕДАЧА"},
    {"AWD CLUTCH", "МУФТА AWD"},
    {"BATTERY", "АККУМУЛЯТОР"},
    {"FIRMWARE", "ПРОШИВКА"},
    {"UPTIME", "ВРЕМЯ РАБОТЫ"},
    {"FREE HEAP", "СВОБ. ПАМЯТЬ"},
    {"MIN HEAP", "МИН. ПАМЯТЬ"},
    {"RENDERS / S", "КАДРОВ / С"},
    {"AVG RENDER", "СРЕДНИЙ КАДР"},
    {"DISPLAY", "ДИСПЛЕЙ"},
    {"TOUCH", "СЕНСОР"},
    {"BRIGHTNESS", "ЯРКОСТЬ"},
    {"TRANSPORT", "ТРАНСПОРТ"},
    {"ECUS", "БЛОКИ ЭБУ"},
    {"SAMPLES", "ИЗМЕРЕНИЯ"},
    {"STALE", "УСТАРЕЛО"},
    {"ERRORS", "ОШИБКИ"},
    {"Dashboard", "Дашборд"},
    {"Theme", "Тема"},
    {"Day / night", "День / ночь"},
    {"Speed units", "Ед. скорости"},
    {"Temperature", "Температура"},
    {"Brightness", "Яркость"},
    {"Master warnings", "Предупреждения"},
    {"Touch calibration", "Калибровка"},
    {"Coolant warning", "Предупр. ОЖ"},
    {"Engine oil warn", "Предупр. масла"},
    {"Trans oil warn", "Предупр. КПП"},
    {"Low voltage warn", "Предупр. АКБ"},
    {"Dashboard layout", "Сетка дашборда"},
    {"Wi-Fi configuration", "Настройка Wi-Fi"},
    {"BACK TO RETURN | AP TIMEOUT 10 MIN", "НАЗАД | WI-FI НА 10 МИН"},
    {"Language", "Язык"},
    {"ON", "ВКЛ"}, {"OFF", "ВЫКЛ"},
    {"Screen rotation", "Поворот экрана"},
    {"Widget 1 signal", "Сигнал виджета 1"},
    {"Widget 2 signal", "Сигнал виджета 2"},
    {"Widget 3 signal", "Сигнал виджета 3"},
    {"Widget 4 signal", "Сигнал виджета 4"},
    {"Widget 5 signal", "Сигнал виджета 5"},
    {"Widget 6 signal", "Сигнал виджета 6"},
    {"Widget 7 signal", "Сигнал виджета 7"},
    {"Widget 8 signal", "Сигнал виджета 8"},
    {"OEM", "ЗАВОД"},
    {"AUTO", "АВТО"},
    {"DAY", "ДЕНЬ"},
    {"NIGHT", "НОЧЬ"},
    {"FIXED", "ФИКС."},
    {"DISABLED", "ВЫКЛ."},
    {"ENABLED", "ВКЛ."},
    {"NO THRESHOLDS", "НЕТ ПОРОГОВ"},
    {"START", "СТАРТ"},
    {"CHANGE", "ИЗМЕНИТЬ"},
    {"INACTIVE", "НЕАКТИВНО"},
    {"N/A", "Н/Д"},
    {"UNKNOWN", "НЕИЗВЕСТНО"},
    {"SINGLE", "ОДИН"},
    {"1 WIDGET", "1 ВИДЖЕТ"},
    {"2 WIDGETS", "2 ВИДЖЕТА"},
    {"4 WIDGETS", "4 ВИДЖЕТА"},
    {"6 WIDGETS", "6 ВИДЖЕТОВ"},
    {"8 WIDGETS", "8 ВИДЖЕТОВ"},
    {"1 LARGE + 2", "1 БОЛЬШОЙ + 2"},
    {"1 LARGE + 4", "1 БОЛЬШОЙ + 4"},
    {"2 LARGE + 4", "2 БОЛЬШИХ + 4"},
    {"NO WIDGETS CONFIGURED", "ВИДЖЕТЫ НЕ НАСТРОЕНЫ"},
    {"NO DATA", "НЕТ ДАННЫХ"},
    {"OFFLINE", "НЕТ СВЯЗИ"},
    {"LIVE", "ДАННЫЕ"},
    {"DEMO", "ДЕМО"},
    {"STALE", "УСТАРЕЛО"},
    {"UNAVAILABLE", "НЕДОСТУПНО"},
    {"UNSUPPORTED", "НЕТ ПОДД."},
    {"ERROR", "ОШИБКА"},
    {"CHOOSE", "ВЫБОР"},
    {"REPLACE WIDGET", "ЗАМЕНИТЬ ВИДЖЕТ"},
    {"SIGNAL", "СИГНАЛ"},
    {"SIGNAL PICKER", "ВЫБОР СИГНАЛА"},
    {"SELECT DATA", "ВЫБЕРИТЕ ДАННЫЕ"},
    {"TAP OR HOLD TO RETURN", "КОСНИТЕСЬ ДЛЯ ВОЗВРАТА"},
    {"TAP SIGNAL TO REPLACE  |  HOLD TO CANCEL", "КОСНИТЕСЬ СИГНАЛА | УДЕРЖ. ОТМЕНА"},
    {"HOLD ANYWHERE TO CANCEL", "УДЕРЖИВАЙТЕ ДЛЯ ОТМЕНЫ"},
    {"TOUCH CALIBRATION", "КАЛИБРОВКА КАСАНИЯ"},
    {"BACK", "НАЗАД"},
    {"CANCEL", "ОТМЕНА"},
    {"PREV", "НАЗАД"},
    {"NEXT", "ДАЛЕЕ"},
    {"PREVIOUS", "НАЗАД"},
    {"PAGE", "СТР."},
    {"MORE", "ДАЛЕЕ"},
    {"READY", "ГОТОВО"},
    {"PWM", "ШИМ"},
    {"ENGLISH", "АНГЛИЙСКИЙ"},
    {"RUSSIAN", "РУССКИЙ"},
};

static const char *const s_signal_labels_en[MD_SIGNAL_COUNT] = {
    "ENGINE SPEED", "VEHICLE SPEED", "COOLANT", "ENGINE OIL",
    "ENGINE LOAD", "CHARGE PRESSURE", "AMBIENT PRESSURE", "BOOST",
    "TRANSMISSION OIL", "GEAR", "AWD CLUTCH", "BATTERY",
};

static const char *const s_signal_labels_en_compact[MD_SIGNAL_COUNT] = {
    "RPM", "SPD", "COOL", "OIL", "LOAD", "CHG",
    "AMB", "BST", "T OIL", "GEAR", "AWD", "BATT",
};

static const char *const s_signal_labels_ru[MD_SIGNAL_COUNT] = {
    "ОБОРОТЫ ДВИГАТЕЛЯ", "СКОРОСТЬ АВТО", "ТЕМПЕРАТУРА ОЖ",
    "ТЕМПЕРАТУРА МАСЛА", "НАГРУЗКА ДВИГАТЕЛЯ", "ДАВЛЕНИЕ НАДДУВА",
    "АТМОСФЕРНОЕ ДАВЛЕНИЕ", "НАДДУВ", "ТЕМПЕРАТУРА МАСЛА КПП",
    "ПЕРЕДАЧА", "ТЕМПЕРАТУРА МУФТЫ AWD", "НАПРЯЖЕНИЕ АКБ",
};

static const char *const s_signal_labels_ru_compact[MD_SIGNAL_COUNT] = {
    "ОБ/МИН", "СКОР.", "ОЖ", "МАСЛО", "НАГР.", "НАДД.",
    "АТМ.", "БУСТ", "КПП", "ПЕР.", "AWD", "АКБ",
};

const char *md_ui_i18n_text(md_ui_language_t language, const char *english)
{
    if (!english || language != MD_UI_LANGUAGE_RUSSIAN) return english;
    for (unsigned i = 0; i < sizeof(s_translations) / sizeof(s_translations[0]); ++i) {
        if (strcmp(english, s_translations[i].english) == 0)
            return s_translations[i].russian;
    }
    return english;
}

const char *md_ui_i18n_signal_label(md_ui_language_t language,
                                    md_ui_signal_id_t signal)
{
    if (signal < 0 || signal >= MD_SIGNAL_COUNT) return "SIGNAL";
    if (!s_signal_labels_en[signal]) return md_ui_signal_definitions[signal].label;
    return language == MD_UI_LANGUAGE_RUSSIAN
        ? s_signal_labels_ru[signal] : s_signal_labels_en[signal];
}

const char *md_ui_i18n_compact_signal_label(md_ui_language_t language,
                                            md_ui_signal_id_t signal,
                                            const char *fallback)
{
    if (signal < 0 || signal >= MD_SIGNAL_COUNT)
        return fallback ? fallback : "SIGNAL";
    if (!s_signal_labels_en_compact[signal]) return md_ui_signal_definitions[signal].label;
    return language == MD_UI_LANGUAGE_RUSSIAN
        ? s_signal_labels_ru_compact[signal]
        : s_signal_labels_en_compact[signal];
}
