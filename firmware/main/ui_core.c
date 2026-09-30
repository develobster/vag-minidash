#include "ui_core.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define SETTINGS_MAGIC_0 'M'
#define SETTINGS_MAGIC_1 'D'
#define SETTINGS_MAGIC_2 'U'
#define SETTINGS_MAGIC_3 'I'
#define SETTINGS_VERSION 3U
#define SETTINGS_PREVIOUS_VERSION 2U
#define SETTINGS_LEGACY_VERSION 1U
#define SETTINGS_V1_V2_HEADER_SIZE 15U
#define SETTINGS_V3_HEADER_SIZE 16U
#define SETTINGS_WARNING_WIRE_SIZE 11U
#define SETTINGS_CRC_SIZE 4U
#define SETTINGS_PAGE_COUNT \
    ((MD_UI_SETTINGS_ROWS + MD_UI_SETTINGS_ROWS_PER_PAGE - 1U) / \
     MD_UI_SETTINGS_ROWS_PER_PAGE)

const md_ui_signal_definition_t md_ui_signal_definitions[MD_SIGNAL_COUNT] = {
    {MD_SIGNAL_ENGINE_RPM, "engine.rpm", "ENGINE SPEED", "rpm", MD_UI_UNIT_NONE, 0},
    {MD_SIGNAL_VEHICLE_SPEED, "vehicle.speed", "VEHICLE SPEED", "km/h", MD_UI_UNIT_SPEED, 0},
    {MD_SIGNAL_ENGINE_COOLANT_TEMPERATURE, "engine.coolant_temperature", "COOLANT", "degC", MD_UI_UNIT_TEMPERATURE, 0},
    {MD_SIGNAL_ENGINE_OIL_TEMPERATURE, "engine.oil_temperature", "ENGINE OIL", "degC", MD_UI_UNIT_TEMPERATURE, 0},
    {MD_SIGNAL_ENGINE_LOAD, "engine.load", "ENGINE LOAD", "%", MD_UI_UNIT_PERCENT, 0},
    {MD_SIGNAL_ENGINE_CHARGE_PRESSURE, "engine.charge_pressure_actual", "CHARGE PRESSURE", "hPa", MD_UI_UNIT_PRESSURE, 0},
    {MD_SIGNAL_ENGINE_AMBIENT_PRESSURE, "engine.ambient_pressure", "AMBIENT PRESSURE", "hPa", MD_UI_UNIT_PRESSURE, 0},
    {MD_SIGNAL_ENGINE_BOOST, "engine.boost", "BOOST", "hPa", MD_UI_UNIT_BOOST, 2},
    {MD_SIGNAL_TRANSMISSION_OIL_TEMPERATURE, "transmission.oil_temperature", "TRANSMISSION OIL", "degC", MD_UI_UNIT_TEMPERATURE, 0},
    {MD_SIGNAL_TRANSMISSION_GEAR, "transmission.gear", "GEAR", "", MD_UI_UNIT_GEAR, 0},
    {MD_SIGNAL_AWD_CLUTCH_TEMPERATURE, "awd.clutch_temperature", "AWD CLUTCH", "degC", MD_UI_UNIT_TEMPERATURE, 0},
    {MD_SIGNAL_BATTERY_VOLTAGE, "vehicle.battery_voltage", "BATTERY", "V", MD_UI_UNIT_VOLTAGE, 1},
};

static bool canonical_name_valid(const char *name)
{
    if (!name || !name[0]) return false;
    size_t length = 0;
    bool previous_was_dot = true;
    for (; name[length] != '\0'; ++length) {
        const char ch = name[length];
        if (length >= MD_UI_SIGNAL_NAME_LEN - 1U) return false;
        if (ch == '.') {
            if (previous_was_dot) return false;
            previous_was_dot = true;
        } else if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_') {
            previous_was_dot = false;
        } else {
            return false;
        }
    }
    return !previous_was_dot;
}

const md_ui_signal_definition_t *md_ui_signal_definition(md_ui_signal_id_t id)
{
    if (id < 0 || id >= MD_SIGNAL_COUNT) return NULL;
    return &md_ui_signal_definitions[id];
}

int md_ui_signal_id_from_name(const char *canonical_name)
{
    if (!canonical_name) return -1;
    for (int i = 0; i < MD_SIGNAL_COUNT; ++i) {
        if (strcmp(canonical_name, md_ui_signal_definitions[i].canonical_name) == 0) {
            return i;
        }
    }
    return -1;
}

void md_ui_signal_store_init(md_ui_signal_store_t *store)
{
    if (!store) return;
    memset(store, 0, sizeof(*store));
    for (int i = 0; i < MD_SIGNAL_COUNT; ++i) {
        store->signals[i].state = MD_UI_SIGNAL_UNAVAILABLE;
    }
}

bool md_ui_signal_store_publish_id(md_ui_signal_store_t *store,
                                   md_ui_signal_id_t id, float value,
                                   md_ui_signal_state_t state, uint32_t now_ms,
                                   uint32_t ttl_ms, bool demo)
{
    if (!store || id < 0 || id >= MD_SIGNAL_COUNT ||
        state < MD_UI_SIGNAL_VALID || state > MD_UI_SIGNAL_ERROR) return false;
    if (state == MD_UI_SIGNAL_VALID && (!isfinite(value) || ttl_ms == 0)) {
        state = MD_UI_SIGNAL_ERROR;
    }
    md_ui_signal_record_t *record = &store->signals[id];
    record->state = state;
    record->value = state == MD_UI_SIGNAL_VALID ? value : 0.0f;
    record->updated_ms = now_ms;
    record->ttl_ms = ttl_ms;
    record->received = true;
    record->demo = demo;
    return true;
}

bool md_ui_signal_store_publish(md_ui_signal_store_t *store,
                                const char *canonical_name, float value,
                                md_ui_signal_state_t state, uint32_t now_ms,
                                uint32_t ttl_ms, bool demo)
{
    if (!store || !canonical_name || state < MD_UI_SIGNAL_VALID ||
        state > MD_UI_SIGNAL_ERROR) return false;
    const int id = md_ui_signal_id_from_name(canonical_name);
    if (id < 0) {
        ++store->unknown_signal_updates;
        return false;
    }
    return md_ui_signal_store_publish_id(store, (md_ui_signal_id_t)id,
                                         value, state, now_ms, ttl_ms, demo);
}

md_ui_signal_snapshot_t md_ui_signal_snapshot(const md_ui_signal_store_t *store,
                                              md_ui_signal_id_t id,
                                              uint32_t now_ms)
{
    md_ui_signal_snapshot_t snapshot = {
        .state = MD_UI_SIGNAL_UNSUPPORTED,
        .value = 0.0f,
        .age_ms = 0,
        .value_available = false,
        .demo = false,
    };
    if (!store || id < 0 || id >= MD_SIGNAL_COUNT) return snapshot;

    const md_ui_signal_record_t *record = &store->signals[id];
    if (!record->received) {
        snapshot.state = MD_UI_SIGNAL_UNAVAILABLE;
        return snapshot;
    }

    snapshot.state = record->state;
    snapshot.age_ms = now_ms - record->updated_ms;
    snapshot.demo = record->demo;
    if (record->state == MD_UI_SIGNAL_VALID) {
        if (record->ttl_ms == 0 || snapshot.age_ms >= record->ttl_ms) {
            snapshot.state = MD_UI_SIGNAL_STALE;
        } else {
            snapshot.value = record->value;
            snapshot.value_available = true;
        }
    }
    return snapshot;
}

md_ui_signal_summary_t md_ui_signal_summary(const md_ui_signal_store_t *store,
                                            uint32_t now_ms)
{
    md_ui_signal_summary_t summary = {0};
    if (!store) return summary;
    summary.unknown_signal_updates = store->unknown_signal_updates;
    for (int i = 0; i < MD_SIGNAL_COUNT; ++i) {
        const md_ui_signal_snapshot_t snapshot =
            md_ui_signal_snapshot(store, (md_ui_signal_id_t)i, now_ms);
        const md_ui_signal_record_t *record = &store->signals[i];
        summary.sample_count += record->received ? 1U : 0U;
        if (record->received) {
            summary.has_demo_data |= record->demo;
            summary.has_live_data |= !record->demo;
        }
        switch (snapshot.state) {
        case MD_UI_SIGNAL_VALID:
            ++summary.valid_count;
            summary.has_valid_demo_data |= snapshot.demo;
            summary.has_valid_live_data |= !snapshot.demo;
            break;
        case MD_UI_SIGNAL_STALE: ++summary.stale_count; break;
        case MD_UI_SIGNAL_UNAVAILABLE: ++summary.unavailable_count; break;
        case MD_UI_SIGNAL_UNSUPPORTED: ++summary.unsupported_count; break;
        case MD_UI_SIGNAL_ERROR: ++summary.error_count; break;
        }
    }
    return summary;
}

md_ui_severity_t md_ui_signal_severity(const md_ui_settings_t *settings,
                                       md_ui_signal_id_t id, float value,
                                       md_ui_signal_state_t state)
{
    if (!settings || !settings->warnings_enabled || state != MD_UI_SIGNAL_VALID ||
        !isfinite(value) || id < 0 || id >= MD_SIGNAL_COUNT) {
        return MD_UI_SEVERITY_NORMAL;
    }
    const int32_t value_milli = (int32_t)(value * 1000.0f);
    for (unsigned i = 0; i < MD_UI_MAX_WARNING_THRESHOLDS; ++i) {
        const md_ui_warning_threshold_t *threshold = &settings->warnings[i];
        if (!threshold->enabled || threshold->signal_id != (uint8_t)id) continue;
        if (threshold->direction == MD_UI_THRESHOLD_HIGH) {
            if (value_milli >= threshold->critical_milli) return MD_UI_SEVERITY_CRITICAL;
            if (value_milli >= threshold->warning_milli) return MD_UI_SEVERITY_WARNING;
        } else {
            if (value_milli <= threshold->critical_milli) return MD_UI_SEVERITY_CRITICAL;
            if (value_milli <= threshold->warning_milli) return MD_UI_SEVERITY_WARNING;
        }
    }
    return MD_UI_SEVERITY_NORMAL;
}

void md_ui_settings_defaults(md_ui_settings_t *settings)
{
    if (!settings) return;
    memset(settings, 0, sizeof(*settings));
    settings->dashboard = MD_UI_DASHBOARD_OEM;
    settings->layout = MD_UI_LAYOUT_QUAD;
    settings->theme = MD_UI_THEME_OEM;
    settings->day_night = MD_UI_DAY_NIGHT_AUTO;
    settings->units = MD_UI_UNITS_METRIC;
    settings->temperature_unit = MD_UI_TEMP_CELSIUS;
    settings->language = MD_UI_LANGUAGE_ENGLISH;
    settings->brightness_pct = 70;
    settings->last_screen = MD_UI_SCREEN_DASHBOARD;
    settings->widget_count = 4;
    settings->warnings_enabled = false;

    settings->widgets[0] = (md_ui_widget_binding_t){
        .id = 1, .type = MD_UI_WIDGET_VALUE, .column = 0, .row = 0,
        .columns = 6, .rows = 4, .canonical_signal = "engine.rpm"};
    settings->widgets[1] = (md_ui_widget_binding_t){
        .id = 2, .type = MD_UI_WIDGET_VALUE, .column = 6, .row = 0,
        .columns = 6, .rows = 4, .canonical_signal = "vehicle.speed"};
    settings->widgets[2] = (md_ui_widget_binding_t){
        .id = 3, .type = MD_UI_WIDGET_VALUE, .column = 0, .row = 4,
        .columns = 6, .rows = 4, .canonical_signal = "engine.coolant_temperature"};
    settings->widgets[3] = (md_ui_widget_binding_t){
        .id = 4, .type = MD_UI_WIDGET_VALUE, .column = 6, .row = 4,
        .columns = 6, .rows = 4, .canonical_signal = "engine.oil_temperature"};

    settings->warnings[0] = (md_ui_warning_threshold_t){
        .signal_id = MD_SIGNAL_ENGINE_COOLANT_TEMPERATURE, .enabled = false,
        .direction = MD_UI_THRESHOLD_HIGH, .warning_milli = 105000,
        .critical_milli = 120000};
    settings->warnings[1] = (md_ui_warning_threshold_t){
        .signal_id = MD_SIGNAL_ENGINE_OIL_TEMPERATURE, .enabled = false,
        .direction = MD_UI_THRESHOLD_HIGH, .warning_milli = 130000,
        .critical_milli = 150000};
    settings->warnings[2] = (md_ui_warning_threshold_t){
        .signal_id = MD_SIGNAL_TRANSMISSION_OIL_TEMPERATURE, .enabled = false,
        .direction = MD_UI_THRESHOLD_HIGH, .warning_milli = 120000,
        .critical_milli = 140000};
    settings->warnings[3] = (md_ui_warning_threshold_t){
        .signal_id = MD_SIGNAL_BATTERY_VOLTAGE, .enabled = false,
        .direction = MD_UI_THRESHOLD_LOW, .warning_milli = 11500,
        .critical_milli = 10500};
    memset(settings->fixed_card_overrides, 0xff,
           sizeof(settings->fixed_card_overrides));
}

typedef struct {
    uint8_t column;
    uint8_t row;
    uint8_t columns;
    uint8_t rows;
} md_ui_layout_cell_t;

static unsigned layout_cells(md_ui_layout_t layout,
                             md_ui_layout_cell_t cells[MD_UI_MAX_WIDGETS])
{
    static const md_ui_layout_cell_t single[] = {{0, 0, 12, 8}};
    static const md_ui_layout_cell_t dual[] = {{0, 0, 6, 8}, {6, 0, 6, 8}};
    static const md_ui_layout_cell_t quad[] = {
        {0, 0, 6, 4}, {6, 0, 6, 4}, {0, 4, 6, 4}, {6, 4, 6, 4}};
    static const md_ui_layout_cell_t six[] = {
        {0, 0, 6, 3}, {6, 0, 6, 3},
        {0, 3, 6, 3}, {6, 3, 6, 3},
        {0, 6, 6, 2}, {6, 6, 6, 2}};
    static const md_ui_layout_cell_t eight[] = {
        {0, 0, 6, 2}, {6, 0, 6, 2},
        {0, 2, 6, 2}, {6, 2, 6, 2},
        {0, 4, 6, 2}, {6, 4, 6, 2},
        {0, 6, 6, 2}, {6, 6, 6, 2}};
    static const md_ui_layout_cell_t mixed_1_2[] = {
        {0, 0, 6, 8}, {6, 0, 6, 4}, {6, 4, 6, 4}};
    static const md_ui_layout_cell_t mixed_1_4[] = {
        {0, 0, 6, 8}, {6, 0, 6, 2}, {6, 2, 6, 2},
        {6, 4, 6, 2}, {6, 6, 6, 2}};
    static const md_ui_layout_cell_t mixed_2_4[] = {
        {0, 0, 6, 4}, {6, 0, 6, 4},
        {0, 4, 3, 4}, {3, 4, 3, 4}, {6, 4, 3, 4}, {9, 4, 3, 4}};
    const md_ui_layout_cell_t *source = NULL;
    unsigned count = 0;
    switch (layout) {
    case MD_UI_LAYOUT_SINGLE: source = single; count = 1; break;
    case MD_UI_LAYOUT_DUAL: source = dual; count = 2; break;
    case MD_UI_LAYOUT_QUAD: source = quad; count = 4; break;
    case MD_UI_LAYOUT_SIX: source = six; count = 6; break;
    case MD_UI_LAYOUT_EIGHT: source = eight; count = 8; break;
    case MD_UI_LAYOUT_MIXED_1_PLUS_2: source = mixed_1_2; count = 3; break;
    case MD_UI_LAYOUT_MIXED_1_PLUS_4: source = mixed_1_4; count = 5; break;
    case MD_UI_LAYOUT_MIXED_2_PLUS_4: source = mixed_2_4; count = 6; break;
    default: return 0;
    }
    if (cells) memcpy(cells, source, count * sizeof(*source));
    return count;
}

static void widget_binding_default(md_ui_widget_binding_t *widget, unsigned index)
{
    if (!widget || index >= MD_UI_MAX_WIDGETS) return;
    memset(widget, 0, sizeof(*widget));
    widget->id = (uint8_t)(index + 1U);
    widget->type = MD_UI_WIDGET_VALUE;
    const md_ui_signal_id_t signal = (md_ui_signal_id_t)(index % MD_SIGNAL_COUNT);
    (void)snprintf(widget->canonical_signal, sizeof(widget->canonical_signal), "%s",
                   md_ui_signal_definitions[signal].canonical_name);
}

bool md_ui_settings_set_layout(md_ui_settings_t *settings,
                               md_ui_layout_t layout)
{
    if (!settings || layout <= MD_UI_LAYOUT_CUSTOM || layout >= MD_UI_LAYOUT_COUNT)
        return false;
    md_ui_layout_cell_t cells[MD_UI_MAX_WIDGETS];
    const unsigned count = layout_cells(layout, cells);
    if (count == 0 || count > MD_UI_MAX_WIDGETS) return false;
    for (unsigned i = 0; i < count; ++i) {
        md_ui_widget_binding_t *widget = &settings->widgets[i];
        if (widget->id == 0 || !widget->canonical_signal[0])
            widget_binding_default(widget, i);
        widget->id = (uint8_t)(i + 1U);
        if (widget->type >= MD_UI_WIDGET_COUNT) widget->type = MD_UI_WIDGET_VALUE;
        widget->column = cells[i].column;
        widget->row = cells[i].row;
        widget->columns = cells[i].columns;
        widget->rows = cells[i].rows;
    }
    settings->layout = layout;
    settings->widget_count = (uint8_t)count;
    return md_ui_settings_validate(settings);
}

bool md_ui_settings_set_widget_signal(md_ui_settings_t *settings,
                                      uint8_t widget_index, int direction)
{
    if (!settings || widget_index >= settings->widget_count ||
        widget_index >= MD_UI_MAX_WIDGETS || (direction != -1 && direction != 1))
        return false;
    md_ui_widget_binding_t *widget = &settings->widgets[widget_index];
    int signal = md_ui_signal_id_from_name(widget->canonical_signal);
    if (signal < 0 || signal >= MD_SIGNAL_COUNT) signal = 0;
    signal = (signal + MD_SIGNAL_COUNT + direction) % MD_SIGNAL_COUNT;
    (void)snprintf(widget->canonical_signal, sizeof(widget->canonical_signal), "%s",
                   md_ui_signal_definitions[signal].canonical_name);
    return md_ui_settings_validate(settings);
}

bool md_ui_settings_bind_widget_signal(md_ui_settings_t *settings,
                                       uint8_t widget_index,
                                       md_ui_signal_id_t signal)
{
    if (!settings || widget_index >= settings->widget_count ||
        widget_index >= MD_UI_MAX_WIDGETS || signal < 0 || signal >= MD_SIGNAL_COUNT)
        return false;
    md_ui_widget_binding_t *widget = &settings->widgets[widget_index];
    char previous[MD_UI_SIGNAL_NAME_LEN];
    (void)snprintf(previous, sizeof(previous), "%s", widget->canonical_signal);
    (void)snprintf(widget->canonical_signal, sizeof(widget->canonical_signal), "%s",
                   md_ui_signal_definitions[signal].canonical_name);
    if (md_ui_settings_validate(settings)) return true;
    (void)snprintf(widget->canonical_signal, sizeof(widget->canonical_signal), "%s",
                   previous);
    return false;
}

static const md_ui_signal_id_t s_fixed_card_defaults[MD_UI_CARD_GROUP_COUNT]
    [MD_UI_CARD_SLOTS_PER_GROUP] = {
    {MD_SIGNAL_ENGINE_RPM, MD_SIGNAL_VEHICLE_SPEED,
     MD_SIGNAL_ENGINE_COOLANT_TEMPERATURE, MD_SIGNAL_ENGINE_BOOST},
    {MD_SIGNAL_ENGINE_RPM, MD_SIGNAL_VEHICLE_SPEED,
     MD_SIGNAL_ENGINE_LOAD, MD_SIGNAL_BATTERY_VOLTAGE},
    {MD_SIGNAL_VEHICLE_SPEED, MD_SIGNAL_ENGINE_RPM,
     MD_SIGNAL_BATTERY_VOLTAGE, MD_SIGNAL_ENGINE_COOLANT_TEMPERATURE},
    {MD_SIGNAL_ENGINE_RPM, MD_SIGNAL_VEHICLE_SPEED,
     MD_SIGNAL_ENGINE_LOAD, MD_SIGNAL_ENGINE_BOOST},
    {MD_SIGNAL_ENGINE_OIL_TEMPERATURE, MD_SIGNAL_ENGINE_COOLANT_TEMPERATURE,
     MD_SIGNAL_TRANSMISSION_OIL_TEMPERATURE, MD_SIGNAL_AWD_CLUTCH_TEMPERATURE},
    {MD_SIGNAL_TRANSMISSION_GEAR, MD_SIGNAL_TRANSMISSION_OIL_TEMPERATURE,
     MD_SIGNAL_AWD_CLUTCH_TEMPERATURE, MD_SIGNAL_COUNT},
};

md_ui_signal_id_t md_ui_settings_card_signal(const md_ui_settings_t *settings,
                                             md_ui_card_group_t group,
                                             uint8_t slot)
{
    if (!settings || group < 0 || group >= MD_UI_CARD_GROUP_COUNT ||
        slot >= MD_UI_CARD_SLOTS_PER_GROUP) return MD_SIGNAL_COUNT;
    const md_ui_signal_id_t default_signal = s_fixed_card_defaults[group][slot];
    if (default_signal >= MD_SIGNAL_COUNT) return MD_SIGNAL_COUNT;
    const uint8_t override = settings->fixed_card_overrides[
        (unsigned)group * MD_UI_CARD_SLOTS_PER_GROUP + slot];
    return override < MD_SIGNAL_COUNT ? (md_ui_signal_id_t)override : default_signal;
}

bool md_ui_settings_bind_card_signal(md_ui_settings_t *settings,
                                     md_ui_card_group_t group, uint8_t slot,
                                     md_ui_signal_id_t signal)
{
    if (!settings || group < 0 || group >= MD_UI_CARD_GROUP_COUNT ||
        signal < 0 || signal >= MD_SIGNAL_COUNT ||
        md_ui_settings_card_signal(settings, group, slot) >= MD_SIGNAL_COUNT)
        return false;
    const unsigned index = (unsigned)group * MD_UI_CARD_SLOTS_PER_GROUP + slot;
    const uint8_t previous = settings->fixed_card_overrides[index];
    settings->fixed_card_overrides[index] = (uint8_t)signal;
    if (md_ui_settings_validate(settings)) return true;
    settings->fixed_card_overrides[index] = previous;
    return false;
}

md_ui_card_group_t md_ui_card_group_for_screen(md_ui_screen_t screen,
                                               const md_ui_settings_t *settings)
{
    if (!settings) return MD_UI_CARD_GROUP_NONE;
    if (screen == MD_UI_SCREEN_CUSTOM ||
        (screen == MD_UI_SCREEN_DASHBOARD &&
         settings->dashboard == MD_UI_DASHBOARD_CUSTOM))
        return MD_UI_CARD_GROUP_CUSTOM;
    if (screen == MD_UI_SCREEN_DASHBOARD &&
        settings->dashboard < MD_UI_DASHBOARD_CUSTOM)
        return (md_ui_card_group_t)settings->dashboard;
    switch (screen) {
    case MD_UI_SCREEN_PERFORMANCE: return MD_UI_CARD_GROUP_PERFORMANCE;
    case MD_UI_SCREEN_TEMPERATURES: return MD_UI_CARD_GROUP_TEMPERATURES;
    case MD_UI_SCREEN_DRIVETRAIN: return MD_UI_CARD_GROUP_DRIVETRAIN;
    default: return MD_UI_CARD_GROUP_NONE;
    }
}

static bool threshold_valid(const md_ui_warning_threshold_t *threshold)
{
    if (!threshold || threshold->signal_id >= MD_SIGNAL_COUNT ||
        (threshold->direction != MD_UI_THRESHOLD_HIGH &&
         threshold->direction != MD_UI_THRESHOLD_LOW)) return false;
    if (!threshold->enabled) return true;
    if (threshold->direction == MD_UI_THRESHOLD_HIGH) {
        return threshold->critical_milli >= threshold->warning_milli;
    }
    return threshold->critical_milli <= threshold->warning_milli;
}

bool md_ui_settings_validate(const md_ui_settings_t *settings)
{
    if (!settings || settings->dashboard < 0 || settings->dashboard >= MD_UI_DASHBOARD_COUNT ||
        settings->layout < 0 || settings->layout >= MD_UI_LAYOUT_COUNT ||
        settings->theme < 0 || settings->theme >= MD_UI_THEME_COUNT ||
        settings->day_night < 0 || settings->day_night >= MD_UI_DAY_NIGHT_COUNT ||
        settings->units < 0 || settings->units >= MD_UI_UNITS_COUNT ||
        settings->temperature_unit < 0 || settings->temperature_unit >= MD_UI_TEMP_COUNT ||
        settings->language < 0 || settings->language >= MD_UI_LANGUAGE_COUNT ||
        settings->brightness_pct > 100 || settings->last_screen >= MD_UI_SCREEN_FOCUS ||
        settings->widget_count > MD_UI_MAX_WIDGETS) return false;

    md_ui_layout_cell_t expected_cells[MD_UI_MAX_WIDGETS];
    const unsigned expected_count = layout_cells(settings->layout, expected_cells);
    if (settings->layout != MD_UI_LAYOUT_CUSTOM &&
        (settings->widget_count != expected_count || expected_count == 0)) return false;

    for (unsigned i = 0; i < settings->widget_count; ++i) {
        const md_ui_widget_binding_t *widget = &settings->widgets[i];
        if (widget->id == 0 || widget->type >= MD_UI_WIDGET_COUNT ||
            widget->columns < 3 || widget->rows < 2 ||
            widget->column >= MD_UI_GRID_COLUMNS || widget->row >= MD_UI_GRID_ROWS ||
            widget->columns > MD_UI_GRID_COLUMNS - widget->column ||
            widget->rows > MD_UI_GRID_ROWS - widget->row ||
            !canonical_name_valid(widget->canonical_signal)) return false;
        if (settings->layout != MD_UI_LAYOUT_CUSTOM &&
            (widget->column != expected_cells[i].column ||
             widget->row != expected_cells[i].row ||
             widget->columns != expected_cells[i].columns ||
             widget->rows != expected_cells[i].rows)) return false;
        for (unsigned j = 0; j < i; ++j) {
            if (settings->widgets[j].id == widget->id) return false;
            const md_ui_widget_binding_t *other = &settings->widgets[j];
            const bool separated = widget->column + widget->columns <= other->column ||
                other->column + other->columns <= widget->column ||
                widget->row + widget->rows <= other->row ||
                other->row + other->rows <= widget->row;
            if (!separated) return false;
        }
    }
    for (unsigned i = 0; i < MD_UI_MAX_WARNING_THRESHOLDS; ++i) {
        if (!threshold_valid(&settings->warnings[i])) return false;
        for (unsigned j = 0; j < i; ++j) {
            if (settings->warnings[i].signal_id == settings->warnings[j].signal_id) {
                return false;
            }
        }
    }
    for (unsigned i = 0; i < MD_UI_FIXED_CARD_OVERRIDES_COUNT; ++i) {
        if (settings->fixed_card_overrides[i] != UINT8_MAX &&
            settings->fixed_card_overrides[i] >= MD_SIGNAL_COUNT) return false;
    }
    return true;
}

static void write_u32_le(uint8_t *output, uint32_t value)
{
    output[0] = (uint8_t)(value & 0xffU);
    output[1] = (uint8_t)((value >> 8) & 0xffU);
    output[2] = (uint8_t)((value >> 16) & 0xffU);
    output[3] = (uint8_t)((value >> 24) & 0xffU);
}

static uint32_t read_u32_le(const uint8_t *input)
{
    return (uint32_t)input[0] | ((uint32_t)input[1] << 8) |
           ((uint32_t)input[2] << 16) | ((uint32_t)input[3] << 24);
}

static uint32_t crc32(const uint8_t *bytes, size_t length)
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

bool md_ui_settings_encode(const md_ui_settings_t *settings, uint8_t *output,
                           size_t output_size, size_t *written)
{
    if (written) *written = 0;
    if (!output || output_size < MD_UI_SETTINGS_WIRE_SIZE ||
        !md_ui_settings_validate(settings)) return false;

    memset(output, 0, MD_UI_SETTINGS_WIRE_SIZE);
    output[0] = SETTINGS_MAGIC_0;
    output[1] = SETTINGS_MAGIC_1;
    output[2] = SETTINGS_MAGIC_2;
    output[3] = SETTINGS_MAGIC_3;
    output[4] = SETTINGS_VERSION;
    output[5] = (uint8_t)settings->layout;
    output[6] = (uint8_t)settings->dashboard;
    output[7] = (uint8_t)settings->theme;
    output[8] = (uint8_t)settings->day_night;
    output[9] = (uint8_t)settings->units;
    output[10] = (uint8_t)settings->temperature_unit;
    output[11] = settings->brightness_pct;
    output[12] = settings->last_screen;
    output[13] = settings->widget_count;
    output[14] = settings->warnings_enabled ? 1U : 0U;
    output[15] = (uint8_t)settings->language;

    size_t offset = SETTINGS_V3_HEADER_SIZE;
    for (unsigned i = 0; i < MD_UI_MAX_WIDGETS; ++i) {
        const md_ui_widget_binding_t *widget = &settings->widgets[i];
        const size_t name_len = i < settings->widget_count
            ? strlen(widget->canonical_signal) : 0U;
        output[offset++] = widget->id;
        output[offset++] = widget->type;
        output[offset++] = widget->column;
        output[offset++] = widget->row;
        output[offset++] = widget->columns;
        output[offset++] = widget->rows;
        output[offset++] = (uint8_t)name_len;
        if (name_len > 0) memcpy(output + offset, widget->canonical_signal, name_len);
        offset += MD_UI_SIGNAL_NAME_LEN;
    }
    for (unsigned i = 0; i < MD_UI_MAX_WARNING_THRESHOLDS; ++i) {
        const md_ui_warning_threshold_t *threshold = &settings->warnings[i];
        output[offset++] = threshold->signal_id;
        output[offset++] = threshold->enabled ? 1U : 0U;
        output[offset++] = (uint8_t)threshold->direction;
        write_u32_le(output + offset, (uint32_t)threshold->warning_milli);
        offset += 4;
        write_u32_le(output + offset, (uint32_t)threshold->critical_milli);
        offset += 4;
    }
    memcpy(output + offset, settings->fixed_card_overrides,
           sizeof(settings->fixed_card_overrides));
    offset += sizeof(settings->fixed_card_overrides);
    const uint32_t checksum = crc32(output, offset);
    write_u32_le(output + offset, checksum);
    offset += SETTINGS_CRC_SIZE;
    if (offset != MD_UI_SETTINGS_WIRE_SIZE) return false;
    if (written) *written = offset;
    return true;
}

bool md_ui_settings_decode(const uint8_t *input, size_t input_size,
                           md_ui_settings_t *settings)
{
    if (!input || !settings || input_size < MD_UI_SETTINGS_V1_WIRE_SIZE ||
        input[0] != SETTINGS_MAGIC_0 || input[1] != SETTINGS_MAGIC_1 ||
        input[2] != SETTINGS_MAGIC_2 || input[3] != SETTINGS_MAGIC_3 ||
        input[14] > 1U) return false;
    const uint8_t version = input[4];
    const size_t expected_size = version == SETTINGS_LEGACY_VERSION
        ? MD_UI_SETTINGS_V1_WIRE_SIZE
        : (version == SETTINGS_PREVIOUS_VERSION
            ? MD_UI_SETTINGS_V2_WIRE_SIZE
            : (version == SETTINGS_VERSION ? MD_UI_SETTINGS_WIRE_SIZE : 0U));
    if (input_size != expected_size ||
        (version == SETTINGS_LEGACY_VERSION && input[5] != 0U) ||
        (version != SETTINGS_LEGACY_VERSION && input[5] >= MD_UI_LAYOUT_COUNT) ||
        (version == SETTINGS_VERSION && input[15] >= MD_UI_LANGUAGE_COUNT)) return false;
    const uint32_t expected_crc = crc32(input, input_size - SETTINGS_CRC_SIZE);
    if (read_u32_le(input + input_size - SETTINGS_CRC_SIZE) != expected_crc) {
        return false;
    }

    md_ui_settings_t decoded = {0};
    memset(decoded.fixed_card_overrides, 0xff,
           sizeof(decoded.fixed_card_overrides));
    decoded.dashboard = (md_ui_dashboard_t)input[6];
    decoded.layout = version == SETTINGS_LEGACY_VERSION
        ? MD_UI_LAYOUT_CUSTOM : (md_ui_layout_t)input[5];
    decoded.theme = (md_ui_theme_id_t)input[7];
    decoded.day_night = (md_ui_day_night_t)input[8];
    decoded.units = (md_ui_unit_system_t)input[9];
    decoded.temperature_unit = (md_ui_temperature_unit_t)input[10];
    decoded.brightness_pct = input[11];
    decoded.last_screen = input[12];
    decoded.widget_count = input[13];
    decoded.warnings_enabled = input[14] != 0;
    decoded.language = version == SETTINGS_VERSION
        ? (md_ui_language_t)input[15] : MD_UI_LANGUAGE_ENGLISH;

    const size_t header_size = version == SETTINGS_VERSION
        ? SETTINGS_V3_HEADER_SIZE : SETTINGS_V1_V2_HEADER_SIZE;
    size_t offset = header_size;
    const unsigned wire_widget_count = version == SETTINGS_LEGACY_VERSION
        ? 6U : MD_UI_MAX_WIDGETS;
    if (decoded.widget_count > wire_widget_count) return false;
    for (unsigned i = 0; i < wire_widget_count; ++i) {
        md_ui_widget_binding_t *widget = &decoded.widgets[i];
        widget->id = input[offset++];
        widget->type = input[offset++];
        widget->column = input[offset++];
        widget->row = input[offset++];
        widget->columns = input[offset++];
        widget->rows = input[offset++];
        const uint8_t name_len = input[offset++];
        if (name_len >= MD_UI_SIGNAL_NAME_LEN) return false;
        if (name_len > 0) memcpy(widget->canonical_signal, input + offset, name_len);
        widget->canonical_signal[name_len] = '\0';
        offset += MD_UI_SIGNAL_NAME_LEN;
    }
    for (unsigned i = 0; i < MD_UI_MAX_WARNING_THRESHOLDS; ++i) {
        md_ui_warning_threshold_t *threshold = &decoded.warnings[i];
        threshold->signal_id = input[offset++];
        const uint8_t enabled = input[offset++];
        threshold->direction = (md_ui_threshold_direction_t)input[offset++];
        if (enabled > 1U) return false;
        threshold->enabled = enabled != 0;
        threshold->warning_milli = (int32_t)read_u32_le(input + offset);
        offset += 4;
        threshold->critical_milli = (int32_t)read_u32_le(input + offset);
        offset += 4;
    }
    if (version == SETTINGS_VERSION) {
        memcpy(decoded.fixed_card_overrides, input + offset,
               sizeof(decoded.fixed_card_overrides));
        offset += sizeof(decoded.fixed_card_overrides);
    }
    if (offset != input_size - SETTINGS_CRC_SIZE || !md_ui_settings_validate(&decoded)) {
        return false;
    }
    *settings = decoded;
    return true;
}

bool md_ui_settings_set_warning(md_ui_settings_t *settings,
                                const char *canonical_signal,
                                bool enabled,
                                md_ui_threshold_direction_t direction,
                                int32_t warning_milli,
                                int32_t critical_milli)
{
    if (!settings || md_ui_signal_id_from_name(canonical_signal) < 0 ||
        (direction != MD_UI_THRESHOLD_HIGH && direction != MD_UI_THRESHOLD_LOW)) {
        return false;
    }
    const uint8_t id = (uint8_t)md_ui_signal_id_from_name(canonical_signal);
    for (unsigned i = 0; i < MD_UI_MAX_WARNING_THRESHOLDS; ++i) {
        if (settings->warnings[i].signal_id != id) continue;
        md_ui_warning_threshold_t previous = settings->warnings[i];
        settings->warnings[i] = (md_ui_warning_threshold_t){
            .signal_id = id, .enabled = enabled, .direction = direction,
            .warning_milli = warning_milli, .critical_milli = critical_milli};
        if (!md_ui_settings_validate(settings)) {
            settings->warnings[i] = previous;
            return false;
        }
        return true;
    }
    return false;
}

void md_ui_state_init(md_ui_state_t *state,
                      const md_ui_settings_t *settings)
{
    if (!state) return;
    memset(state, 0, sizeof(*state));
    state->screen = settings && settings->last_screen < MD_UI_SCREEN_COUNT
        ? (md_ui_screen_t)settings->last_screen : MD_UI_SCREEN_DASHBOARD;
    if (state->screen >= MD_UI_SCREEN_FOCUS) state->screen = MD_UI_SCREEN_DASHBOARD;
    state->settings_return_screen = state->screen == MD_UI_SCREEN_SETTINGS
        ? MD_UI_SCREEN_DASHBOARD : state->screen;
    state->focus_return_screen = state->screen;
    state->widget_picker_return_screen = state->screen;
    state->focused_signal = MD_SIGNAL_COUNT;
    state->widget_picker_signal = MD_SIGNAL_COUNT;
    state->widget_picker_slot = MD_UI_MAX_WIDGETS;
    state->widget_picker_group = MD_UI_CARD_GROUP_NONE;
}

bool md_ui_state_select_setting(md_ui_state_t *state, uint8_t row)
{
    if (!state || state->sleeping || state->screen != MD_UI_SCREEN_SETTINGS ||
        state->context == MD_UI_CONTEXT_DRIVING || row >= MD_UI_SETTINGS_ROWS)
        return false;
    const bool changed = state->selected_setting != row;
    state->selected_setting = row;
    return changed;
}

bool md_ui_state_settings_page(md_ui_state_t *state, int direction)
{
    if (!state || state->sleeping || state->screen != MD_UI_SCREEN_SETTINGS ||
        state->context == MD_UI_CONTEXT_DRIVING || (direction != -1 && direction != 1))
        return false;
    const int page = state->selected_setting / MD_UI_SETTINGS_ROWS_PER_PAGE;
    const int next_page = page + direction;
    if (next_page < 0 || next_page >= (int)SETTINGS_PAGE_COUNT) return false;
    state->selected_setting = (uint8_t)(next_page * MD_UI_SETTINGS_ROWS_PER_PAGE);
    return true;
}

static void state_set_screen(md_ui_state_t *state, md_ui_settings_t *settings,
                             md_ui_screen_t screen)
{
    state->screen = screen;
    if (settings && screen < MD_UI_SCREEN_FOCUS) settings->last_screen = (uint8_t)screen;
}

bool md_ui_state_open_details(md_ui_state_t *state,
                              md_ui_settings_t *settings,
                              md_ui_signal_id_t signal)
{
    if (!state || !settings || state->sleeping ||
        state->screen >= MD_UI_SCREEN_SETTINGS ||
        signal < 0 || signal >= MD_SIGNAL_COUNT) return false;
    state->focus_return_screen = state->screen;
    state->focused_signal = signal;
    state_set_screen(state, settings, MD_UI_SCREEN_FOCUS);
    return true;
}

bool md_ui_state_open_widget_picker(md_ui_state_t *state,
                                    const md_ui_settings_t *settings,
                                    uint8_t widget_index)
{
    if (!state || !settings || state->sleeping ||
        state->context == MD_UI_CONTEXT_DRIVING ||
        state->screen >= MD_UI_SCREEN_SETTINGS) return false;
    const md_ui_card_group_t group =
        md_ui_card_group_for_screen(state->screen, settings);
    int signal = -1;
    if (group == MD_UI_CARD_GROUP_CUSTOM) {
        if (widget_index >= settings->widget_count ||
            widget_index >= MD_UI_MAX_WIDGETS) return false;
        signal = md_ui_signal_id_from_name(
            settings->widgets[widget_index].canonical_signal);
    } else if (group >= 0 && group < MD_UI_CARD_GROUP_COUNT) {
        signal = md_ui_settings_card_signal(settings, group, widget_index);
    }
    if (signal < 0 || signal >= MD_SIGNAL_COUNT) return false;
    state->widget_picker_return_screen = state->screen;
    state->widget_picker_slot = widget_index;
    state->widget_picker_group = group;
    state->widget_picker_signal = (md_ui_signal_id_t)signal;
    state->screen = MD_UI_SCREEN_WIDGET_PICKER;
    return true;
}

bool md_ui_state_widget_picker_set_signal(md_ui_state_t *state,
                                          md_ui_signal_id_t signal)
{
    if (!state || state->screen != MD_UI_SCREEN_WIDGET_PICKER ||
        signal < 0 || signal >= MD_SIGNAL_COUNT) return false;
    state->widget_picker_signal = signal;
    return true;
}

bool md_ui_action_repeatable(md_ui_action_t action)
{
    return action == MD_UI_ACTION_NEXT_SCREEN ||
           action == MD_UI_ACTION_PREVIOUS_SCREEN ||
           action == MD_UI_ACTION_NEXT_ITEM ||
           action == MD_UI_ACTION_PREVIOUS_ITEM;
}

bool md_ui_state_set_context(md_ui_state_t *state,
                             md_ui_settings_t *settings,
                             md_ui_context_t context, bool verified)
{
    if (!state || !settings || context < MD_UI_CONTEXT_UNKNOWN ||
        context > MD_UI_CONTEXT_DRIVING ||
        (context != MD_UI_CONTEXT_UNKNOWN && !verified)) return false;
    state->context = context;
    if (context == MD_UI_CONTEXT_DRIVING &&
        (state->screen == MD_UI_SCREEN_SETTINGS ||
         state->screen == MD_UI_SCREEN_CALIBRATION ||
         state->screen == MD_UI_SCREEN_WIDGET_PICKER))
        state_set_screen(state, settings, MD_UI_SCREEN_DASHBOARD);
    return true;
}

static md_ui_screen_t next_content_screen(md_ui_screen_t screen)
{
    return screen >= MD_UI_SCREEN_DASHBOARD && screen < MD_UI_SCREEN_CUSTOM
        ? (md_ui_screen_t)(screen + 1)
        : MD_UI_SCREEN_DASHBOARD;
}

static md_ui_screen_t previous_content_screen(md_ui_screen_t screen)
{
    return screen > MD_UI_SCREEN_DASHBOARD && screen <= MD_UI_SCREEN_CUSTOM
        ? (md_ui_screen_t)(screen - 1)
        : MD_UI_SCREEN_CUSTOM;
}

static bool adjust_setting(md_ui_state_t *state, md_ui_settings_t *settings,
                           int direction)
{
    switch (state->selected_setting) {
    case 0:
        settings->dashboard = (md_ui_dashboard_t)
            ((settings->dashboard + MD_UI_DASHBOARD_COUNT + direction) % MD_UI_DASHBOARD_COUNT);
        break;
    case 1:
        settings->theme = (md_ui_theme_id_t)
            ((settings->theme + MD_UI_THEME_COUNT + direction) % MD_UI_THEME_COUNT);
        break;
    case 2:
        settings->day_night = (md_ui_day_night_t)
            ((settings->day_night + MD_UI_DAY_NIGHT_COUNT + direction) % MD_UI_DAY_NIGHT_COUNT);
        break;
    case 3:
        settings->units = (md_ui_unit_system_t)
            ((settings->units + MD_UI_UNITS_COUNT + direction) % MD_UI_UNITS_COUNT);
        break;
    case 4:
        settings->temperature_unit = (md_ui_temperature_unit_t)
            ((settings->temperature_unit + MD_UI_TEMP_COUNT + direction) % MD_UI_TEMP_COUNT);
        break;
    case 5: {
        const int next = (int)settings->brightness_pct + direction * 10;
        settings->brightness_pct = (uint8_t)(next < 10 ? 100 : (next > 100 ? 10 : next));
        break;
    }
    case 6:
        settings->warnings_enabled = !settings->warnings_enabled;
        break;
    case 7:
        state_set_screen(state, settings, MD_UI_SCREEN_CALIBRATION);
        break;
    case 8:
    case 9:
    case 10:
    case 11:
        settings->warnings[state->selected_setting - 8U].enabled =
            !settings->warnings[state->selected_setting - 8U].enabled;
        break;
    case 12: {
        static const md_ui_layout_t layouts[] = {
            MD_UI_LAYOUT_SINGLE, MD_UI_LAYOUT_DUAL, MD_UI_LAYOUT_QUAD,
            MD_UI_LAYOUT_SIX, MD_UI_LAYOUT_EIGHT,
            MD_UI_LAYOUT_MIXED_1_PLUS_2, MD_UI_LAYOUT_MIXED_1_PLUS_4,
            MD_UI_LAYOUT_MIXED_2_PLUS_4};
        int selected = -1;
        for (unsigned i = 0; i < sizeof(layouts) / sizeof(layouts[0]); ++i) {
            if (settings->layout == layouts[i]) selected = (int)i;
        }
        const int layout_count = (int)(sizeof(layouts) / sizeof(layouts[0]));
        if (selected < 0) selected = direction > 0 ? -1 : 0;
        selected = (selected + layout_count + direction) % layout_count;
        if (!md_ui_settings_set_layout(settings, layouts[selected])) return false;
        break;
    }
    case 13:
        settings->language = settings->language == MD_UI_LANGUAGE_ENGLISH
            ? MD_UI_LANGUAGE_RUSSIAN : MD_UI_LANGUAGE_ENGLISH;
        break;
    default:
        if (state->selected_setting >= 14U &&
            state->selected_setting < 22U) {
            return md_ui_settings_set_widget_signal(settings,
                (uint8_t)(state->selected_setting - 14U), direction);
        }
        return false;
    }
    return true;
}

bool md_ui_state_dispatch(md_ui_state_t *state,
                          md_ui_settings_t *settings,
                          md_ui_action_t action)
{
    if (!state || !settings || action < MD_UI_ACTION_NEXT_SCREEN ||
        action > MD_UI_ACTION_OPEN_DETAILS) return false;
    if (state->context == MD_UI_CONTEXT_DRIVING &&
        (action == MD_UI_ACTION_OPEN_SETTINGS ||
         (state->screen == MD_UI_SCREEN_SETTINGS &&
          action != MD_UI_ACTION_BACK && action != MD_UI_ACTION_HOME) ||
         (state->screen == MD_UI_SCREEN_WIDGET_PICKER &&
          action != MD_UI_ACTION_BACK && action != MD_UI_ACTION_HOME) ||
         (state->screen == MD_UI_SCREEN_CALIBRATION &&
          action != MD_UI_ACTION_BACK && action != MD_UI_ACTION_HOME)))
        return false;
    const md_ui_screen_t previous_screen = state->screen;
    const bool previous_sleeping = state->sleeping;
    const md_ui_settings_t previous_settings = *settings;
    const uint8_t previous_selection = state->selected_setting;
    const md_ui_signal_id_t previous_picker_signal = state->widget_picker_signal;
    const uint8_t previous_picker_slot = state->widget_picker_slot;
    const md_ui_card_group_t previous_picker_group = state->widget_picker_group;

    switch (action) {
    case MD_UI_ACTION_NEXT_SCREEN:
        if (!state->sleeping && state->screen < MD_UI_SCREEN_SETTINGS) {
            state_set_screen(state, settings, next_content_screen(state->screen));
        }
        break;
    case MD_UI_ACTION_PREVIOUS_SCREEN:
        if (!state->sleeping && state->screen < MD_UI_SCREEN_SETTINGS) {
            state_set_screen(state, settings, previous_content_screen(state->screen));
        }
        break;
    case MD_UI_ACTION_OPEN_SETTINGS:
        if (!state->sleeping && state->screen < MD_UI_SCREEN_SETTINGS) {
            state->settings_return_screen = state->screen;
            state_set_screen(state, settings, MD_UI_SCREEN_SETTINGS);
            state->selected_setting = 0;
        }
        break;
    case MD_UI_ACTION_BACK:
        if (state->screen == MD_UI_SCREEN_CALIBRATION) {
            state_set_screen(state, settings, MD_UI_SCREEN_SETTINGS);
        } else if (state->screen == MD_UI_SCREEN_WIDGET_PICKER) {
            state_set_screen(state, settings, state->widget_picker_return_screen);
            state->widget_picker_slot = MD_UI_MAX_WIDGETS;
            state->widget_picker_signal = MD_SIGNAL_COUNT;
            state->widget_picker_group = MD_UI_CARD_GROUP_NONE;
        } else if (state->screen == MD_UI_SCREEN_FOCUS) {
            state_set_screen(state, settings, state->focus_return_screen);
        } else if (state->screen == MD_UI_SCREEN_SETTINGS) {
            state_set_screen(state, settings, state->settings_return_screen);
        } else if (state->sleeping) {
            state->sleeping = false;
        } else {
            state_set_screen(state, settings, MD_UI_SCREEN_DASHBOARD);
        }
        break;
    case MD_UI_ACTION_HOME:
        if (!state->sleeping) state_set_screen(state, settings, MD_UI_SCREEN_DASHBOARD);
        break;
    case MD_UI_ACTION_NEXT_ITEM:
        if (!state->sleeping && state->screen == MD_UI_SCREEN_WIDGET_PICKER &&
            state->widget_picker_signal >= 0 &&
            state->widget_picker_signal < MD_SIGNAL_COUNT) {
            state->widget_picker_signal = (md_ui_signal_id_t)
                ((state->widget_picker_signal + 1) % MD_SIGNAL_COUNT);
        } else if (!state->sleeping && state->screen == MD_UI_SCREEN_SETTINGS) {
            state->selected_setting = (uint8_t)((state->selected_setting + 1U) % MD_UI_SETTINGS_ROWS);
        }
        break;
    case MD_UI_ACTION_PREVIOUS_ITEM:
        if (!state->sleeping && state->screen == MD_UI_SCREEN_WIDGET_PICKER &&
            state->widget_picker_signal >= 0 &&
            state->widget_picker_signal < MD_SIGNAL_COUNT) {
            state->widget_picker_signal = (md_ui_signal_id_t)
                ((state->widget_picker_signal + MD_SIGNAL_COUNT - 1) % MD_SIGNAL_COUNT);
        } else if (!state->sleeping && state->screen == MD_UI_SCREEN_SETTINGS) {
            state->selected_setting = (uint8_t)((state->selected_setting + MD_UI_SETTINGS_ROWS - 1U) % MD_UI_SETTINGS_ROWS);
        }
        break;
    case MD_UI_ACTION_SELECT:
        if (!state->sleeping && state->screen == MD_UI_SCREEN_WIDGET_PICKER) {
            const bool saved = state->widget_picker_group == MD_UI_CARD_GROUP_CUSTOM
                ? md_ui_settings_bind_widget_signal(settings,
                    state->widget_picker_slot, state->widget_picker_signal)
                : md_ui_settings_bind_card_signal(settings,
                    state->widget_picker_group, state->widget_picker_slot,
                    state->widget_picker_signal);
            if (saved) {
                state_set_screen(state, settings,
                                  state->widget_picker_return_screen);
                state->widget_picker_slot = MD_UI_MAX_WIDGETS;
                state->widget_picker_signal = MD_SIGNAL_COUNT;
                state->widget_picker_group = MD_UI_CARD_GROUP_NONE;
            }
        } else if (!state->sleeping && state->screen == MD_UI_SCREEN_SETTINGS) {
            (void)adjust_setting(state, settings, 1);
        }
        break;
    case MD_UI_ACTION_INCREMENT:
        if (!state->sleeping && state->screen == MD_UI_SCREEN_WIDGET_PICKER &&
            state->widget_picker_signal >= 0 &&
            state->widget_picker_signal < MD_SIGNAL_COUNT) {
            state->widget_picker_signal = (md_ui_signal_id_t)
                ((state->widget_picker_signal + 1) % MD_SIGNAL_COUNT);
        } else if (!state->sleeping && state->screen == MD_UI_SCREEN_SETTINGS) {
            (void)adjust_setting(state, settings, 1);
        }
        break;
    case MD_UI_ACTION_DECREMENT:
        if (!state->sleeping && state->screen == MD_UI_SCREEN_WIDGET_PICKER &&
            state->widget_picker_signal >= 0 &&
            state->widget_picker_signal < MD_SIGNAL_COUNT) {
            state->widget_picker_signal = (md_ui_signal_id_t)
                ((state->widget_picker_signal + MD_SIGNAL_COUNT - 1) % MD_SIGNAL_COUNT);
        } else if (!state->sleeping && state->screen == MD_UI_SCREEN_SETTINGS) {
            (void)adjust_setting(state, settings, -1);
        }
        break;
    case MD_UI_ACTION_SLEEP:
        state->sleeping = true;
        break;
    case MD_UI_ACTION_WAKE:
        state->sleeping = false;
        break;
    case MD_UI_ACTION_OPEN_DETAILS:
        /* Requires a signal payload; md_ui_state_open_details validates it. */
        break;
    }
    bool warning_config_changed = false;
    for (unsigned i = 0; i < MD_UI_MAX_WARNING_THRESHOLDS; ++i) {
        const md_ui_warning_threshold_t *before = &previous_settings.warnings[i];
        const md_ui_warning_threshold_t *after = &settings->warnings[i];
        warning_config_changed |= before->signal_id != after->signal_id ||
            before->enabled != after->enabled || before->direction != after->direction ||
            before->warning_milli != after->warning_milli ||
            before->critical_milli != after->critical_milli;
    }
    const bool changed = previous_screen != state->screen ||
        previous_sleeping != state->sleeping ||
        previous_settings.dashboard != settings->dashboard ||
        previous_settings.theme != settings->theme ||
        previous_settings.layout != settings->layout ||
        previous_settings.day_night != settings->day_night ||
        previous_settings.units != settings->units ||
        previous_settings.temperature_unit != settings->temperature_unit ||
        previous_settings.language != settings->language ||
        previous_settings.brightness_pct != settings->brightness_pct ||
        previous_settings.last_screen != settings->last_screen ||
        previous_settings.warnings_enabled != settings->warnings_enabled ||
        warning_config_changed ||
        previous_selection != state->selected_setting ||
        previous_picker_signal != state->widget_picker_signal ||
        previous_picker_slot != state->widget_picker_slot ||
        previous_picker_group != state->widget_picker_group ||
        previous_settings.widget_count != settings->widget_count ||
        memcmp(previous_settings.widgets, settings->widgets,
               sizeof(settings->widgets)) != 0 ||
        memcmp(previous_settings.fixed_card_overrides,
               settings->fixed_card_overrides,
               sizeof(settings->fixed_card_overrides)) != 0;
    return changed;
}

bool md_ui_input_valid(md_ui_input_source_t source,
                       md_ui_action_t action)
{
    return source >= MD_UI_INPUT_TOUCH && source < MD_UI_INPUT_SOURCE_COUNT &&
           action >= MD_UI_ACTION_NEXT_SCREEN && action <= MD_UI_ACTION_OPEN_DETAILS;
}

bool md_ui_input_command(const char *command,
                         md_ui_input_source_t *source,
                         md_ui_action_t *action)
{
    if (!command || !source || !action) return false;
    *source = MD_UI_INPUT_CONSOLE;
    if (strcmp(command, "next") == 0) *action = MD_UI_ACTION_NEXT_SCREEN;
    else if (strcmp(command, "prev") == 0) *action = MD_UI_ACTION_PREVIOUS_SCREEN;
    else if (strcmp(command, "select") == 0) *action = MD_UI_ACTION_SELECT;
    else if (strcmp(command, "back") == 0) *action = MD_UI_ACTION_BACK;
    else if (strcmp(command, "settings") == 0) *action = MD_UI_ACTION_OPEN_SETTINGS;
    else if (strcmp(command, "up") == 0) *action = MD_UI_ACTION_PREVIOUS_ITEM;
    else if (strcmp(command, "down") == 0) *action = MD_UI_ACTION_NEXT_ITEM;
    else if (strcmp(command, "plus") == 0) *action = MD_UI_ACTION_INCREMENT;
    else if (strcmp(command, "minus") == 0) *action = MD_UI_ACTION_DECREMENT;
    else if (strcmp(command, "sleep") == 0) *action = MD_UI_ACTION_SLEEP;
    else if (strcmp(command, "wake") == 0) *action = MD_UI_ACTION_WAKE;
    else if (strcmp(command, "home") == 0) *action = MD_UI_ACTION_HOME;
    else return false;
    return true;
}

bool md_ui_touch_action(md_ui_screen_t screen, bool sleeping,
                        md_ui_touch_gesture_t gesture,
                        md_ui_action_t *action)
{
    if (!action || screen < MD_UI_SCREEN_DASHBOARD ||
        screen >= MD_UI_SCREEN_COUNT || gesture < MD_UI_TOUCH_TAP ||
        gesture >= MD_UI_TOUCH_GESTURE_COUNT) return false;
    if (sleeping) {
        *action = MD_UI_ACTION_WAKE;
        return true;
    }
    if (screen == MD_UI_SCREEN_CALIBRATION) {
        if (gesture != MD_UI_TOUCH_LONG_PRESS) return false;
        *action = MD_UI_ACTION_BACK;
    } else if (screen == MD_UI_SCREEN_FOCUS || screen == MD_UI_SCREEN_CONFIG_MODE) {
        *action = MD_UI_ACTION_BACK;
    } else if (screen == MD_UI_SCREEN_SETTINGS) {
        switch (gesture) {
        case MD_UI_TOUCH_TAP: *action = MD_UI_ACTION_NEXT_ITEM; break;
        case MD_UI_TOUCH_DOUBLE_TAP: *action = MD_UI_ACTION_SELECT; break;
        case MD_UI_TOUCH_LONG_PRESS: *action = MD_UI_ACTION_BACK; break;
        default: return false;
        }
    } else if (screen == MD_UI_SCREEN_WIDGET_PICKER) {
        switch (gesture) {
        case MD_UI_TOUCH_TAP: *action = MD_UI_ACTION_NEXT_ITEM; break;
        case MD_UI_TOUCH_DOUBLE_TAP: *action = MD_UI_ACTION_SELECT; break;
        case MD_UI_TOUCH_LONG_PRESS: *action = MD_UI_ACTION_BACK; break;
        default: return false;
        }
    } else {
        switch (gesture) {
        case MD_UI_TOUCH_TAP: *action = MD_UI_ACTION_NEXT_SCREEN; break;
        case MD_UI_TOUCH_DOUBLE_TAP: *action = MD_UI_ACTION_PREVIOUS_SCREEN; break;
        case MD_UI_TOUCH_LONG_PRESS: *action = MD_UI_ACTION_OPEN_SETTINGS; break;
        default: return false;
        }
    }
    return true;
}
