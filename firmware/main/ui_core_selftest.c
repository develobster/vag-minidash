#include "ui_core_selftest.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ui_core.h"

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

static bool test_settings_round_trip(void)
{
    md_ui_settings_t original;
    md_ui_settings_t decoded;
    uint8_t wire[MD_UI_SETTINGS_WIRE_SIZE];
    size_t written = 0;
    md_ui_settings_defaults(&original);
    if (!md_ui_settings_validate(&original) ||
        !md_ui_settings_encode(&original, wire, sizeof(wire), &written) ||
        written != sizeof(wire) ||
        !md_ui_settings_decode(wire, written, &decoded)) return false;
    if (decoded.dashboard != original.dashboard || decoded.widget_count != original.widget_count ||
        strcmp(decoded.widgets[0].canonical_signal,
               original.widgets[0].canonical_signal) != 0) return false;
    wire[written - 1] ^= 0x80U;
    return !md_ui_settings_decode(wire, written, &decoded);
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

static bool test_navigation_and_console_mapping(void)
{
    md_ui_settings_t settings;
    md_ui_state_t state;
    md_ui_input_source_t source;
    md_ui_action_t action;
    md_ui_settings_defaults(&settings);
    md_ui_state_init(&state, &settings);
    if (!md_ui_input_command("settings", &source, &action) ||
        source != MD_UI_INPUT_CONSOLE || action != MD_UI_ACTION_OPEN_SETTINGS ||
        !md_ui_state_dispatch(&state, &settings, action) ||
        state.screen != MD_UI_SCREEN_SETTINGS) return false;
    if (!md_ui_state_dispatch(&state, &settings, MD_UI_ACTION_NEXT_ITEM) ||
        state.selected_setting != 1U) return false;
    if (!md_ui_state_dispatch(&state, &settings, MD_UI_ACTION_BACK) ||
        state.screen != MD_UI_SCREEN_DASHBOARD) return false;
    if (!md_ui_state_dispatch(&state, &settings, MD_UI_ACTION_SLEEP) ||
        !state.sleeping) return false;
    if (!md_ui_state_dispatch(&state, &settings, MD_UI_ACTION_WAKE) ||
        state.sleeping) return false;
    return !md_ui_input_command("vehicle-specific-command", &source, &action);
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

bool md_ui_core_selftest_run(void)
{
    test_totals_t totals = {0};
    printf("MINIDASH_UI_TEST_START\n");
    report(&totals, "settings_round_trip_and_crc", test_settings_round_trip());
    report(&totals, "signal_freshness_and_unknown", test_signal_freshness_and_unknowns());
    report(&totals, "invalid_sample_is_error", test_invalid_sample_is_error());
    report(&totals, "widget_bounds", test_widget_validation());
    report(&totals, "navigation_and_input_mapping", test_navigation_and_console_mapping());
    report(&totals, "warning_thresholds", test_warning_configuration());
    printf("MINIDASH_UI_TEST_SUMMARY pass=%u fail=%u\n", totals.passed, totals.failed);
    return totals.failed == 0;
}
