#include "ui_core_selftest.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ui_core.h"
#include "ui_platform_selftest.h"
#include "display_serial_protocol.h"
#include "ui_i18n.h"
#include "ui_touch_map.h"
#include "lvgl.h"
#include "font/fmt_txt/lv_font_fmt_txt.h"

extern const lv_font_t md_ui_font_ru_10;
extern const lv_font_t md_ui_font_ru_12;
extern const lv_font_t md_ui_font_ru_14;
extern const lv_font_t md_ui_font_ru_16;

typedef struct {
    unsigned passed;
    unsigned failed;
} test_totals_t;

static void report(test_totals_t *totals, const char *name, bool passed)
{
    printf("MINIDASH_UI_TEST name=%s status=%s\n", name,
           passed ? "PASS" : "FAIL");
    if (passed) ++totals->passed;
    else ++totals->failed;
}

static uint32_t test_crc32(const uint8_t *bytes, size_t length)
{
    uint32_t crc = 0xffffffffU;
    for (size_t i = 0; i < length; ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit) {
            const uint32_t mask = (uint32_t)-(int32_t)(crc & 1U);
            crc = (crc >> 1) ^ (0xedb88320U & mask);
        }
    }
    return ~crc;
}

static void test_write_u32_le(uint8_t *output, uint32_t value)
{
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8);
    output[2] = (uint8_t)(value >> 16);
    output[3] = (uint8_t)(value >> 24);
}

static bool test_settings_round_trip(void)
{
    md_ui_settings_t original;
    md_ui_settings_t decoded;
    uint8_t wire[MD_UI_SETTINGS_WIRE_SIZE];
    size_t written = 0;
    md_ui_settings_defaults(&original);
    original.language = MD_UI_LANGUAGE_RUSSIAN;
    if (!md_ui_settings_bind_card_signal(&original,
            MD_UI_CARD_GROUP_PERFORMANCE, 2U, MD_SIGNAL_BATTERY_VOLTAGE))
        return false;
    if (!md_ui_settings_validate(&original) ||
        !md_ui_settings_encode(&original, wire, sizeof(wire), &written) ||
        written != sizeof(wire) ||
        !md_ui_settings_decode(wire, written, &decoded)) return false;
    if (decoded.dashboard != original.dashboard || decoded.layout != original.layout ||
        decoded.language != original.language ||
        decoded.widget_count != original.widget_count ||
        md_ui_settings_card_signal(&decoded, MD_UI_CARD_GROUP_PERFORMANCE, 2U) !=
            MD_SIGNAL_BATTERY_VOLTAGE ||
        strcmp(decoded.widgets[0].canonical_signal,
               original.widgets[0].canonical_signal) != 0) return false;
    wire[written - 1] ^= 0x80U;
    return !md_ui_settings_decode(wire, written, &decoded);
}

static bool test_settings_v1_migration(void)
{
    md_ui_settings_t original;
    md_ui_settings_t decoded;
    uint8_t current[MD_UI_SETTINGS_WIRE_SIZE];
    uint8_t legacy[MD_UI_SETTINGS_V1_WIRE_SIZE] = {0};
    size_t written = 0;
    md_ui_settings_defaults(&original);
    if (!md_ui_settings_encode(&original, current, sizeof(current), &written) ||
        written != sizeof(current)) return false;

    memcpy(legacy, current, 15U);
    legacy[4] = 1U;
    legacy[5] = 0U;
    memcpy(legacy + 15U, current + 16U, 6U * 55U);
    memcpy(legacy + 15U + 6U * 55U, current + 16U + 8U * 55U, 44U);
    test_write_u32_le(legacy + sizeof(legacy) - 4U,
                      test_crc32(legacy, sizeof(legacy) - 4U));
    return md_ui_settings_decode(legacy, sizeof(legacy), &decoded) &&
        decoded.layout == MD_UI_LAYOUT_CUSTOM && decoded.widget_count == 4U &&
        strcmp(decoded.widgets[0].canonical_signal, "engine.rpm") == 0 &&
        md_ui_settings_validate(&decoded);
}

static bool test_settings_v2_migration(void)
{
    md_ui_settings_t original;
    md_ui_settings_t decoded;
    uint8_t current[MD_UI_SETTINGS_WIRE_SIZE];
    uint8_t previous[MD_UI_SETTINGS_V2_WIRE_SIZE] = {0};
    size_t written = 0;
    md_ui_settings_defaults(&original);
    original.language = MD_UI_LANGUAGE_RUSSIAN;
    if (!md_ui_settings_bind_card_signal(&original,
            MD_UI_CARD_GROUP_TEMPERATURES, 1U, MD_SIGNAL_BATTERY_VOLTAGE) ||
        !md_ui_settings_encode(&original, current, sizeof(current), &written) ||
        written != sizeof(current)) return false;
    memcpy(previous, current, 15U);
    previous[4] = 2U;
    memcpy(previous + 15U, current + 16U, 8U * 55U);
    memcpy(previous + 15U + 8U * 55U, current + 16U + 8U * 55U, 44U);
    test_write_u32_le(previous + sizeof(previous) - 4U,
                      test_crc32(previous, sizeof(previous) - 4U));
    return md_ui_settings_decode(previous, sizeof(previous), &decoded) &&
        decoded.language == MD_UI_LANGUAGE_ENGLISH &&
        md_ui_settings_card_signal(&decoded, MD_UI_CARD_GROUP_TEMPERATURES, 1U) ==
            MD_SIGNAL_ENGINE_COOLANT_TEMPERATURE &&
        md_ui_settings_validate(&decoded);
}

static bool test_signal_freshness_and_unknowns(void)
{
    md_ui_signal_store_t store;
    md_ui_signal_store_init(&store);
    md_ui_signal_snapshot_t snapshot = md_ui_signal_snapshot(
        &store, MD_SIGNAL_ENGINE_RPM, 100U);
    if (snapshot.state != MD_UI_SIGNAL_UNAVAILABLE || snapshot.value_available) return false;

    if (!md_ui_signal_store_publish_id(&store, MD_SIGNAL_ENGINE_RPM,
        2200.0f, MD_UI_SIGNAL_VALID, UINT32_MAX - 20U, 50U, false)) return false;
    snapshot = md_ui_signal_snapshot(&store, MD_SIGNAL_ENGINE_RPM, 10U);
    if (snapshot.state != MD_UI_SIGNAL_VALID || !snapshot.value_available ||
        snapshot.value != 2200.0f) return false;
    snapshot = md_ui_signal_snapshot(&store, MD_SIGNAL_ENGINE_RPM, 29U);
    if (snapshot.state != MD_UI_SIGNAL_STALE || snapshot.value_available) return false;

    if (md_ui_signal_store_publish(&store, "engine.unknown", 1.0f,
        MD_UI_SIGNAL_VALID, 30U, 100U, false)) return false;
    if (store.unknown_signal_updates != 1U) return false;
    return md_ui_signal_summary(&store, 30U).stale_count == 1U;
}

static bool test_invalid_sample_is_error(void)
{
    md_ui_signal_store_t store;
    md_ui_signal_store_init(&store);
    if (!md_ui_signal_store_publish_id(&store, MD_SIGNAL_VEHICLE_SPEED,
        NAN, MD_UI_SIGNAL_VALID, 10U, 500U, false)) return false;
    const md_ui_signal_snapshot_t snapshot = md_ui_signal_snapshot(
        &store, MD_SIGNAL_VEHICLE_SPEED, 11U);
    return snapshot.state == MD_UI_SIGNAL_ERROR && !snapshot.value_available;
}

static bool test_widget_validation(void)
{
    md_ui_settings_t settings;
    md_ui_settings_defaults(&settings);
    if (!md_ui_settings_validate(&settings)) return false;
    settings.widgets[0].column = MD_UI_GRID_COLUMNS;
    if (md_ui_settings_validate(&settings)) return false;
    md_ui_settings_defaults(&settings);
    settings.widgets[1].column = 0;
    return !md_ui_settings_validate(&settings);
}

static bool test_layout_presets_and_widget_binding(void)
{
    static const md_ui_layout_t layouts[] = {
        MD_UI_LAYOUT_SINGLE, MD_UI_LAYOUT_DUAL, MD_UI_LAYOUT_QUAD,
        MD_UI_LAYOUT_SIX, MD_UI_LAYOUT_EIGHT,
        MD_UI_LAYOUT_MIXED_1_PLUS_2, MD_UI_LAYOUT_MIXED_1_PLUS_4,
        MD_UI_LAYOUT_MIXED_2_PLUS_4};
    static const uint8_t counts[] = {1U, 2U, 4U, 6U, 8U, 3U, 5U, 6U};
    md_ui_settings_t settings;
    uint8_t wire[MD_UI_SETTINGS_WIRE_SIZE];
    size_t written = 0;
    md_ui_settings_defaults(&settings);
    for (unsigned layout = 0; layout < sizeof(layouts) / sizeof(layouts[0]); ++layout) {
        if (!md_ui_settings_set_layout(&settings, layouts[layout]) ||
            settings.widget_count != counts[layout] ||
            !md_ui_settings_validate(&settings)) return false;
        unsigned area = 0;
        for (unsigned i = 0; i < settings.widget_count; ++i) {
            area += settings.widgets[i].columns * settings.widgets[i].rows;
        }
        if (area != MD_UI_GRID_COLUMNS * MD_UI_GRID_ROWS ||
            !md_ui_settings_encode(&settings, wire, sizeof(wire), &written)) return false;
        md_ui_settings_t decoded;
        if (!md_ui_settings_decode(wire, written, &decoded) ||
            decoded.layout != layouts[layout] ||
            decoded.widget_count != counts[layout]) return false;
    }
    if (!md_ui_settings_set_layout(&settings, MD_UI_LAYOUT_EIGHT) ||
        !md_ui_settings_set_widget_signal(&settings, 7U, 1) ||
        strcmp(settings.widgets[7].canonical_signal, "transmission.oil_temperature") != 0 ||
        md_ui_settings_set_widget_signal(&settings, 8U, 1)) return false;
    return true;
}

static bool test_navigation_and_console_mapping(void)
{
    md_ui_settings_t settings;
    md_ui_state_t state;
    md_ui_input_source_t source;
    md_ui_action_t action;
    static const md_ui_screen_t next_screens[] = {
        MD_UI_SCREEN_PERFORMANCE,
        MD_UI_SCREEN_TEMPERATURES,
        MD_UI_SCREEN_DRIVETRAIN,
        MD_UI_SCREEN_DIAGNOSTICS,
        MD_UI_SCREEN_CUSTOM,
        MD_UI_SCREEN_DASHBOARD,
    };
    md_ui_settings_defaults(&settings);
    md_ui_state_init(&state, &settings);

    for (unsigned i = 0; i < sizeof(next_screens) / sizeof(next_screens[0]); ++i) {
        if (!md_ui_state_dispatch(&state, &settings, MD_UI_ACTION_NEXT_SCREEN) ||
            state.screen != next_screens[i]) return false;
    }
    if (!md_ui_state_dispatch(&state, &settings, MD_UI_ACTION_PREVIOUS_SCREEN) ||
        state.screen != MD_UI_SCREEN_CUSTOM) return false;

    if (!md_ui_input_command("settings", &source, &action) ||
        source != MD_UI_INPUT_CONSOLE || action != MD_UI_ACTION_OPEN_SETTINGS ||
        !md_ui_state_dispatch(&state, &settings, action) ||
        state.screen != MD_UI_SCREEN_SETTINGS) return false;
    if (md_ui_state_dispatch(&state, &settings, MD_UI_ACTION_NEXT_SCREEN) ||
        state.screen != MD_UI_SCREEN_SETTINGS ||
        md_ui_state_dispatch(&state, &settings, MD_UI_ACTION_PREVIOUS_SCREEN) ||
        state.screen != MD_UI_SCREEN_SETTINGS) return false;
    if (!md_ui_state_dispatch(&state, &settings, MD_UI_ACTION_NEXT_ITEM) ||
        state.selected_setting != 1U) return false;
    settings.brightness_pct = 100; /* Exercise wrap independently of boot default. */
    if (!md_ui_state_select_setting(&state, 5) ||
        state.selected_setting != 5U || settings.brightness_pct != 100U ||
        md_ui_state_select_setting(&state, MD_UI_SETTINGS_ROWS) ||
        md_ui_state_select_setting(&state, 5) ||
        !md_ui_state_dispatch(&state, &settings, MD_UI_ACTION_SELECT) ||
        settings.brightness_pct != 10U) return false;
    if (!md_ui_state_dispatch(&state, &settings, MD_UI_ACTION_BACK) ||
        state.screen != MD_UI_SCREEN_CUSTOM) return false;
    if (md_ui_state_select_setting(&state, 0)) return false;
    if (!md_ui_state_dispatch(&state, &settings, MD_UI_ACTION_SLEEP) ||
        !state.sleeping) return false;
    if (!md_ui_state_dispatch(&state, &settings, MD_UI_ACTION_WAKE) ||
        state.sleeping) return false;
    if (!md_ui_touch_action(MD_UI_SCREEN_DASHBOARD, false,
                            MD_UI_TOUCH_LONG_PRESS, &action) ||
        action != MD_UI_ACTION_OPEN_SETTINGS) return false;
    if (!md_ui_touch_action(MD_UI_SCREEN_SETTINGS, false,
                            MD_UI_TOUCH_DOUBLE_TAP, &action) ||
        action != MD_UI_ACTION_SELECT) return false;
    if (!md_ui_touch_action(MD_UI_SCREEN_DASHBOARD, true,
                            MD_UI_TOUCH_TAP, &action) ||
        action != MD_UI_ACTION_WAKE) return false;
    if (!md_ui_input_command("home", &source, &action) ||
        action != MD_UI_ACTION_HOME) return false;
    return !md_ui_input_command("vehicle-specific-command", &source, &action);
}

static bool test_display_serial_protocol(void)
{
    md_display_serial_sample_t sample = {0};
    if (!md_display_serial_sample_decode(
            "MDS1|engine.rpm|VALID|1500|2450", &sample) ||
        strcmp(sample.canonical_signal, "engine.rpm") != 0 ||
        sample.state != MD_UI_SIGNAL_VALID || sample.ttl_ms != 1500 ||
        sample.value != 2450.0f) return false;
    if (!md_display_serial_sample_decode(
            "MDS1|vehicle.speed|STALE|1000|-", &sample) ||
        sample.state != MD_UI_SIGNAL_STALE || sample.value != 0.0f) return false;
    if (md_display_serial_sample_decode(
            "MDS1|engine.unknown|VALID|1000|1", &sample) ||
        md_display_serial_sample_decode(
            "MDS1|engine.rpm|VALID|0|1", &sample) ||
        md_display_serial_sample_decode(
            "MDS1|engine.rpm|UNAVAILABLE|1000|0", &sample) ||
        md_display_serial_sample_decode(
            "MDS1|engine.rpm|VALID|1000|nan", &sample) ||
        md_display_serial_sample_decode(
            "MDS1|engine.rpm|VALID|1000|1|extra", &sample)) return false;
    return true;
}

static bool test_touch_mapping_and_focus(void)
{
    const md_touch_cal_point_t points[MD_TOUCH_CAL_POINTS] = {
        {300, 400, 35, 40}, {3800, 400, 285, 40},
        {3800, 3600, 285, 200}, {300, 3600, 35, 200},
        {2050, 2000, 160, 120}};
    const md_touch_cal_point_t degenerate[MD_TOUCH_CAL_POINTS] = {
        {1000, 1000, 35, 40}, {1000, 1000, 285, 40},
        {1000, 1000, 285, 200}, {1000, 1000, 35, 200},
        {1000, 1000, 160, 120}};
    md_touch_map_t map;
    int16_t x = -1, y = -1;
    if (!md_touch_map_fit(&map, points, 320, 240, 2) ||
        md_touch_map_fit(&map, degenerate, 320, 240, 2) ||
        !md_touch_map_apply(&map, 2050, 2000, &x, &y) ||
        x != 160 || y != 120 ||
        md_touch_map_apply(&map, 6000, 6000, &x, &y) ||
        md_touch_hit_zone(20, 100, 320, 240) != MD_TOUCH_HIT_PREVIOUS ||
        md_touch_hit_zone(300, 100, 320, 240) != MD_TOUCH_HIT_NEXT ||
        md_touch_hit_zone(160, 100, 320, 240) != MD_TOUCH_HIT_CONTENT ||
        md_touch_hit_zone(-1, 100, 320, 240) != MD_TOUCH_HIT_NONE) return false;
    md_ui_settings_t settings;
    md_ui_state_t state;
    md_ui_settings_defaults(&settings);
    md_ui_state_init(&state, &settings);
    if (!md_ui_state_open_details(&state, &settings, MD_SIGNAL_ENGINE_RPM) ||
        state.screen != MD_UI_SCREEN_FOCUS ||
        settings.last_screen != MD_UI_SCREEN_DASHBOARD ||
        !md_ui_state_dispatch(&state, &settings, MD_UI_ACTION_BACK) ||
        state.screen != MD_UI_SCREEN_DASHBOARD ||
        !md_ui_state_dispatch(&state, &settings, MD_UI_ACTION_NEXT_SCREEN) ||
        !md_ui_state_dispatch(&state, &settings, MD_UI_ACTION_HOME) ||
        state.screen != MD_UI_SCREEN_DASHBOARD ||
        md_ui_action_repeatable(MD_UI_ACTION_SELECT) ||
        !md_ui_action_repeatable(MD_UI_ACTION_NEXT_SCREEN)) return false;
    if (md_ui_state_set_context(&state, &settings, MD_UI_CONTEXT_DRIVING, false) ||
        !md_ui_state_set_context(&state, &settings, MD_UI_CONTEXT_DRIVING, true) ||
        md_ui_state_dispatch(&state, &settings, MD_UI_ACTION_OPEN_SETTINGS) ||
        !md_ui_state_set_context(&state, &settings, MD_UI_CONTEXT_UNKNOWN, false) ||
        !md_ui_state_dispatch(&state, &settings, MD_UI_ACTION_OPEN_SETTINGS) ||
        !md_ui_state_set_context(&state, &settings, MD_UI_CONTEXT_DRIVING, true) ||
        state.screen != MD_UI_SCREEN_DASHBOARD)
        return false;
    return true;
}

static bool test_widget_picker(void)
{
    md_ui_settings_t settings;
    md_ui_state_t state;
    md_ui_settings_defaults(&settings);
    if (!md_ui_settings_set_layout(&settings, MD_UI_LAYOUT_EIGHT)) return false;
    md_ui_state_init(&state, &settings);
    for (unsigned i = 0; i < 5U; ++i) {
        if (!md_ui_state_dispatch(&state, &settings, MD_UI_ACTION_NEXT_SCREEN))
            return false;
    }
    if (state.screen != MD_UI_SCREEN_CUSTOM ||
        !md_ui_state_open_widget_picker(&state, &settings, 3U) ||
        state.screen != MD_UI_SCREEN_WIDGET_PICKER ||
        state.widget_picker_slot != 3U) return false;

    const md_ui_signal_id_t initial = state.widget_picker_signal;
    if (!md_ui_state_dispatch(&state, &settings, MD_UI_ACTION_NEXT_ITEM) ||
        !md_ui_state_dispatch(&state, &settings, MD_UI_ACTION_PREVIOUS_ITEM) ||
        state.widget_picker_signal != initial ||
        !md_ui_state_widget_picker_set_signal(&state, MD_SIGNAL_BATTERY_VOLTAGE) ||
        !md_ui_state_dispatch(&state, &settings, MD_UI_ACTION_SELECT) ||
        state.screen != MD_UI_SCREEN_CUSTOM ||
        md_ui_signal_id_from_name(settings.widgets[3].canonical_signal) !=
            MD_SIGNAL_BATTERY_VOLTAGE ||
        settings.widget_count != 8U || !md_ui_settings_validate(&settings))
        return false;

    uint8_t encoded[MD_UI_SETTINGS_WIRE_SIZE];
    size_t written = 0;
    md_ui_settings_t restored;
    if (!md_ui_settings_encode(&settings, encoded, sizeof(encoded), &written) ||
        !md_ui_settings_decode(encoded, written, &restored) ||
        md_ui_signal_id_from_name(restored.widgets[3].canonical_signal) !=
            MD_SIGNAL_BATTERY_VOLTAGE) return false;

    char unchanged[MD_UI_SIGNAL_NAME_LEN];
    (void)snprintf(unchanged, sizeof(unchanged), "%s",
                   settings.widgets[1].canonical_signal);
    if (!md_ui_state_open_widget_picker(&state, &settings, 1U) ||
        !md_ui_state_widget_picker_set_signal(&state, MD_SIGNAL_ENGINE_LOAD) ||
        !md_ui_state_dispatch(&state, &settings, MD_UI_ACTION_BACK) ||
        state.screen != MD_UI_SCREEN_CUSTOM ||
        strcmp(unchanged, settings.widgets[1].canonical_signal) != 0 ||
        md_ui_settings_bind_widget_signal(&settings, 8U, MD_SIGNAL_ENGINE_RPM) ||
        md_ui_settings_bind_widget_signal(&settings, 1U, MD_SIGNAL_COUNT))
        return false;

    return md_ui_state_set_context(&state, &settings, MD_UI_CONTEXT_DRIVING, true) &&
        !md_ui_state_open_widget_picker(&state, &settings, 1U);
}

static bool test_warning_configuration(void)
{
    md_ui_settings_t settings;
    md_ui_settings_defaults(&settings);
    settings.warnings_enabled = true;
    if (!md_ui_settings_set_warning(&settings, "engine.coolant_temperature", true,
        MD_UI_THRESHOLD_HIGH, 105000, 120000)) return false;
    if (md_ui_signal_severity(&settings, MD_SIGNAL_ENGINE_COOLANT_TEMPERATURE,
        110.0f, MD_UI_SIGNAL_VALID) != MD_UI_SEVERITY_WARNING) return false;
    if (md_ui_signal_severity(&settings, MD_SIGNAL_ENGINE_COOLANT_TEMPERATURE,
        121.0f, MD_UI_SIGNAL_VALID) != MD_UI_SEVERITY_CRITICAL) return false;
    return md_ui_signal_severity(&settings, MD_SIGNAL_ENGINE_COOLANT_TEMPERATURE,
        121.0f, MD_UI_SIGNAL_STALE) == MD_UI_SEVERITY_NORMAL;
}

static bool test_settings_pages_and_warning_toggles(void)
{
    md_ui_settings_t settings;
    md_ui_state_t state;
    md_ui_settings_defaults(&settings);
    md_ui_state_init(&state, &settings);
    if (!md_ui_state_dispatch(&state, &settings, MD_UI_ACTION_OPEN_SETTINGS) ||
        (MD_UI_SETTINGS_ROWS + MD_UI_SETTINGS_ROWS_PER_PAGE - 1U) /
            MD_UI_SETTINGS_ROWS_PER_PAGE != 6U ||
        md_ui_state_settings_page(&state, -1)) return false;
    for (uint8_t page = 1U; page < 6U; ++page) {
        if (!md_ui_state_settings_page(&state, 1) ||
            state.selected_setting != page * MD_UI_SETTINGS_ROWS_PER_PAGE) return false;
    }
    if (md_ui_state_settings_page(&state, 1) || state.selected_setting != 20U) return false;
    for (int page = 4; page >= 0; --page) {
        if (!md_ui_state_settings_page(&state, -1) ||
            state.selected_setting != (uint8_t)(page * MD_UI_SETTINGS_ROWS_PER_PAGE)) return false;
    }
    if (md_ui_state_settings_page(&state, -1)) return false;

    if (!md_ui_state_select_setting(&state, 8) || settings.warnings[0].enabled ||
        !md_ui_state_dispatch(&state, &settings, MD_UI_ACTION_SELECT) ||
        !settings.warnings[0].enabled || settings.warnings_enabled ||
        settings.warnings[0].warning_milli != 105000 ||
        settings.warnings[0].critical_milli != 120000) return false;
    if (!md_ui_state_dispatch(&state, &settings, MD_UI_ACTION_SELECT) ||
        settings.warnings[0].enabled) return false;
    if (!md_ui_state_select_setting(&state, 12U) ||
        !md_ui_state_dispatch(&state, &settings, MD_UI_ACTION_SELECT) ||
        settings.layout != MD_UI_LAYOUT_SIX || settings.widget_count != 6U) return false;
    if (!md_ui_state_select_setting(&state, 14U)) return false;
    const char *before = settings.widgets[0].canonical_signal;
    char before_copy[MD_UI_SIGNAL_NAME_LEN];
    (void)snprintf(before_copy, sizeof(before_copy), "%s", before);
    return md_ui_state_dispatch(&state, &settings, MD_UI_ACTION_SELECT) &&
        strcmp(before_copy, settings.widgets[0].canonical_signal) != 0 &&
        md_ui_settings_validate(&settings);
}

static bool test_fixed_dashboard_card_picker(void)
{
    md_ui_settings_t settings;
    md_ui_state_t state;
    md_ui_settings_defaults(&settings);
    md_ui_state_init(&state, &settings);

    if (!md_ui_state_open_widget_picker(&state, &settings, 2U) ||
        state.widget_picker_group != MD_UI_CARD_GROUP_DASHBOARD_OEM ||
        !md_ui_state_widget_picker_set_signal(&state, MD_SIGNAL_BATTERY_VOLTAGE) ||
        !md_ui_state_dispatch(&state, &settings, MD_UI_ACTION_SELECT) ||
        md_ui_settings_card_signal(&settings, MD_UI_CARD_GROUP_DASHBOARD_OEM, 2U) !=
            MD_SIGNAL_BATTERY_VOLTAGE ||
        md_ui_settings_card_signal(&settings, MD_UI_CARD_GROUP_DASHBOARD_SPORT, 2U) !=
            MD_SIGNAL_ENGINE_LOAD) return false;

    if (!md_ui_state_dispatch(&state, &settings, MD_UI_ACTION_NEXT_SCREEN) ||
        state.screen != MD_UI_SCREEN_PERFORMANCE ||
        !md_ui_state_open_widget_picker(&state, &settings, 3U) ||
        state.widget_picker_group != MD_UI_CARD_GROUP_PERFORMANCE ||
        !md_ui_state_widget_picker_set_signal(&state, MD_SIGNAL_ENGINE_LOAD) ||
        !md_ui_state_dispatch(&state, &settings, MD_UI_ACTION_SELECT) ||
        md_ui_settings_card_signal(&settings, MD_UI_CARD_GROUP_PERFORMANCE, 3U) !=
            MD_SIGNAL_ENGINE_LOAD) return false;

    if (!md_ui_state_dispatch(&state, &settings, MD_UI_ACTION_NEXT_SCREEN) ||
        state.screen != MD_UI_SCREEN_TEMPERATURES ||
        !md_ui_state_open_widget_picker(&state, &settings, 0U) ||
        !md_ui_state_widget_picker_set_signal(&state, MD_SIGNAL_BATTERY_VOLTAGE) ||
        !md_ui_state_dispatch(&state, &settings, MD_UI_ACTION_SELECT) ||
        md_ui_settings_card_signal(&settings, MD_UI_CARD_GROUP_TEMPERATURES, 0U) !=
            MD_SIGNAL_BATTERY_VOLTAGE) return false;

    if (!md_ui_state_dispatch(&state, &settings, MD_UI_ACTION_NEXT_SCREEN) ||
        state.screen != MD_UI_SCREEN_DRIVETRAIN ||
        md_ui_state_open_widget_picker(&state, &settings, 3U) ||
        !md_ui_state_open_widget_picker(&state, &settings, 2U) ||
        !md_ui_state_dispatch(&state, &settings, MD_UI_ACTION_BACK)) return false;
    return md_ui_settings_validate(&settings);
}

static bool test_language_setting_and_text(void)
{
    md_ui_settings_t settings;
    md_ui_state_t state;
    md_ui_settings_defaults(&settings);
    md_ui_state_init(&state, &settings);
    if (!md_ui_state_dispatch(&state, &settings, MD_UI_ACTION_OPEN_SETTINGS) ||
        !md_ui_state_select_setting(&state, 13U) ||
        !md_ui_state_dispatch(&state, &settings, MD_UI_ACTION_SELECT) ||
        settings.language != MD_UI_LANGUAGE_RUSSIAN ||
        strcmp(md_ui_i18n_text(settings.language, "Language"), "Язык") != 0 ||
        strcmp(md_ui_i18n_signal_label(settings.language,
            MD_SIGNAL_ENGINE_RPM), "ОБОРОТЫ ДВИГАТЕЛЯ") != 0) return false;
    uint8_t wire[MD_UI_SETTINGS_WIRE_SIZE];
    md_ui_settings_t restored;
    size_t written = 0;
    return md_ui_settings_encode(&settings, wire, sizeof(wire), &written) &&
        md_ui_settings_decode(wire, written, &restored) &&
        restored.language == MD_UI_LANGUAGE_RUSSIAN;
}

static bool russian_font_has_required_glyphs(const lv_font_t *font)
{
    static const uint32_t codepoints[] = {
        'A', '1', 0x042fU, 0x0436U, 0x0451U,
    };
    if (!font || !font->dsc) return false;

    const lv_font_fmt_txt_dsc_t *font_data = font->dsc;
#if !LV_USE_FONT_COMPRESSED
    if (font_data->bitmap_format != LV_FONT_FMT_TXT_PLAIN) return false;
#endif

    for (unsigned i = 0; i < sizeof(codepoints) / sizeof(codepoints[0]); ++i) {
        lv_font_glyph_dsc_t glyph = {0};
        if (!lv_font_get_glyph_dsc(font, &glyph, codepoints[i], 0) ||
            glyph.resolved_font != font || glyph.box_w == 0 || glyph.box_h == 0)
            return false;
    }
    return true;
}

static bool test_russian_font_assets(void)
{
    return russian_font_has_required_glyphs(&md_ui_font_ru_10) &&
        russian_font_has_required_glyphs(&md_ui_font_ru_12) &&
        russian_font_has_required_glyphs(&md_ui_font_ru_14) &&
        russian_font_has_required_glyphs(&md_ui_font_ru_16);
}

bool md_ui_core_selftest_run(void)
{
    test_totals_t totals = {0};
    printf("MINIDASH_UI_TEST_START\n");
    report(&totals, "settings_round_trip_and_crc", test_settings_round_trip());
    report(&totals, "settings_v1_migration", test_settings_v1_migration());
    report(&totals, "settings_v2_migration", test_settings_v2_migration());
    report(&totals, "signal_freshness_and_unknown", test_signal_freshness_and_unknowns());
    report(&totals, "invalid_sample_is_error", test_invalid_sample_is_error());
    report(&totals, "widget_bounds", test_widget_validation());
    report(&totals, "layout_presets_and_widget_binding",
           test_layout_presets_and_widget_binding());
    report(&totals, "navigation_and_input_mapping", test_navigation_and_console_mapping());
    report(&totals, "display_serial_protocol", test_display_serial_protocol());
    report(&totals, "warning_thresholds", test_warning_configuration());
    report(&totals, "settings_pages_and_warning_toggles",
           test_settings_pages_and_warning_toggles());
    report(&totals, "touch_mapping_and_focus", test_touch_mapping_and_focus());
    report(&totals, "widget_picker_assign_and_cancel", test_widget_picker());
    report(&totals, "fixed_dashboard_card_picker", test_fixed_dashboard_card_picker());
    report(&totals, "language_setting_and_russian_text",
           test_language_setting_and_text());
    report(&totals, "russian_font_assets", test_russian_font_assets());
    report(&totals, "widget_precision_and_units", md_ui_test_widget_precision());
    report(&totals, "configuration_four_pages", md_ui_test_configuration());
    report(&totals, "configuration_validation", md_ui_test_configuration_validation());
    report(&totals, "configuration_crc_and_truncation", md_ui_test_configuration_crc());
    report(&totals, "configuration_json_contract", md_ui_test_configuration_json());
    report(&totals, "history_freshness_and_demo", md_ui_test_history());
    report(&totals, "button_debounce_long_repeat_lockout", md_ui_test_buttons());
    report(&totals, "brightness_capability_and_ramp", md_ui_test_brightness());
    report(&totals, "theme_presets_and_crt_startup", md_ui_test_themes_and_startup());
    report(&totals, "rotation_migration_touch", md_ui_test_rotation());
    report(&totals, "target_capabilities", md_ui_test_target_capabilities());
    printf("MINIDASH_UI_TEST_SUMMARY pass=%u fail=%u\n", totals.passed, totals.failed);
    return totals.failed == 0;
}
