#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MD_UI_MAX_WIDGETS 8
#define MD_UI_SIGNAL_NAME_LEN 48
#define MD_UI_MAX_WARNING_THRESHOLDS 4
#define MD_UI_SETTINGS_ROWS 24U
#define MD_UI_SETTINGS_ROWS_PER_PAGE 4U
#define MD_UI_GRID_COLUMNS 12
#define MD_UI_GRID_ROWS 8
#define MD_UI_SETTINGS_V1_WIRE_SIZE 393U
#define MD_UI_SETTINGS_V2_WIRE_SIZE 503U
#define MD_UI_CARD_GROUP_COUNT 6U
#define MD_UI_CARD_SLOTS_PER_GROUP 4U
#define MD_UI_FIXED_CARD_OVERRIDES_COUNT \
    (MD_UI_CARD_GROUP_COUNT * MD_UI_CARD_SLOTS_PER_GROUP)
#define MD_UI_SETTINGS_WIRE_SIZE 528U

typedef enum {
    MD_UI_SIGNAL_VALID = 0,
    MD_UI_SIGNAL_STALE,
    MD_UI_SIGNAL_UNAVAILABLE,
    MD_UI_SIGNAL_UNSUPPORTED,
    MD_UI_SIGNAL_ERROR,
} md_ui_signal_state_t;

typedef enum {
    MD_SIGNAL_ENGINE_RPM = 0,
    MD_SIGNAL_VEHICLE_SPEED,
    MD_SIGNAL_ENGINE_COOLANT_TEMPERATURE,
    MD_SIGNAL_ENGINE_OIL_TEMPERATURE,
    MD_SIGNAL_ENGINE_LOAD,
    MD_SIGNAL_ENGINE_CHARGE_PRESSURE,
    MD_SIGNAL_ENGINE_AMBIENT_PRESSURE,
    MD_SIGNAL_ENGINE_BOOST,
    MD_SIGNAL_TRANSMISSION_OIL_TEMPERATURE,
    MD_SIGNAL_TRANSMISSION_GEAR,
    MD_SIGNAL_AWD_CLUTCH_TEMPERATURE,
    MD_SIGNAL_BATTERY_VOLTAGE,
    MD_SIGNAL_ENGINE_MANIFOLD_PRESSURE,
    MD_SIGNAL_ECU_SUPPLY_VOLTAGE,
#include "analytics_signal_ids.inc"
    MD_SIGNAL_COUNT,
} md_ui_signal_id_t;

typedef enum {
    MD_UI_UNIT_NONE = 0,
    MD_UI_UNIT_SPEED,
    MD_UI_UNIT_TEMPERATURE,
    MD_UI_UNIT_PRESSURE,
    MD_UI_UNIT_BOOST,
    MD_UI_UNIT_PERCENT,
    MD_UI_UNIT_VOLTAGE,
    MD_UI_UNIT_GEAR,
    MD_UI_UNIT_TEMP_DELTA,
} md_ui_unit_family_t;

typedef struct {
    md_ui_signal_id_t id;
    const char *canonical_name;
    const char *label;
    const char *canonical_unit;
    md_ui_unit_family_t unit_family;
    uint8_t decimals;
} md_ui_signal_definition_t;

typedef struct {
    md_ui_signal_state_t state;
    double value; /* Exact 32-bit ECU counters; presentation may round separately. */
    uint32_t updated_ms;
    uint32_t ttl_ms;
    bool received;
    bool demo;
} md_ui_signal_record_t;

typedef struct {
    md_ui_signal_record_t signals[MD_SIGNAL_COUNT];
    uint32_t unknown_signal_updates;
} md_ui_signal_store_t;

typedef struct {
    md_ui_signal_state_t state;
    double value;
    uint32_t age_ms;
    bool value_available;
    bool demo;
} md_ui_signal_snapshot_t;

typedef struct {
    uint8_t sample_count;
    uint8_t valid_count;
    uint8_t stale_count;
    uint8_t unavailable_count;
    uint8_t unsupported_count;
    uint8_t error_count;
    bool has_demo_data;
    bool has_live_data;
    bool has_valid_demo_data;
    bool has_valid_live_data;
    uint32_t unknown_signal_updates;
} md_ui_signal_summary_t;

typedef enum {
    MD_UI_WIDGET_VALUE = 0,
    MD_UI_WIDGET_BAR,
    MD_UI_WIDGET_STATUS,
    MD_UI_WIDGET_GRAPH,
    MD_UI_WIDGET_COUNT,
} md_ui_widget_type_t;

typedef struct {
    uint8_t id;
    uint8_t type;
    uint8_t column;
    uint8_t row;
    uint8_t columns;
    uint8_t rows;
    char canonical_signal[MD_UI_SIGNAL_NAME_LEN];
} md_ui_widget_binding_t;

typedef enum {
    MD_UI_LAYOUT_CUSTOM = 0,
    MD_UI_LAYOUT_SINGLE,
    MD_UI_LAYOUT_DUAL,
    MD_UI_LAYOUT_QUAD,
    MD_UI_LAYOUT_SIX,
    MD_UI_LAYOUT_EIGHT,
    MD_UI_LAYOUT_MIXED_1_PLUS_2,
    MD_UI_LAYOUT_MIXED_1_PLUS_4,
    MD_UI_LAYOUT_MIXED_2_PLUS_4,
    MD_UI_LAYOUT_COUNT,
} md_ui_layout_t;

typedef enum {
    MD_UI_DASHBOARD_OEM = 0,
    MD_UI_DASHBOARD_SPORT,
    MD_UI_DASHBOARD_MINIMAL,
    MD_UI_DASHBOARD_CUSTOM,
    MD_UI_DASHBOARD_COUNT,
} md_ui_dashboard_t;

typedef enum {
    MD_UI_THEME_OEM = 0,
    MD_UI_THEME_SPORT,
    MD_UI_THEME_MINIMAL,
    MD_UI_THEME_TERMINAL,
    MD_UI_THEME_MATRIX,
    MD_UI_THEME_FALLOUT,
    MD_UI_THEME_AMBER,
    MD_UI_THEME_CYBERPUNK,
    MD_UI_THEME_ARCTIC,
    MD_UI_THEME_VOLKSWAGEN,
    MD_UI_THEME_PORSCHE,
    MD_UI_THEME_COUNT,
} md_ui_theme_id_t;

typedef enum {
    MD_UI_DAY_NIGHT_AUTO = 0,
    MD_UI_DAY_NIGHT_DAY,
    MD_UI_DAY_NIGHT_NIGHT,
    MD_UI_DAY_NIGHT_COUNT,
} md_ui_day_night_t;

typedef enum {
    MD_UI_UNITS_METRIC = 0,
    MD_UI_UNITS_IMPERIAL,
    MD_UI_UNITS_COUNT,
} md_ui_unit_system_t;

typedef enum {
    MD_UI_TEMP_CELSIUS = 0,
    MD_UI_TEMP_FAHRENHEIT,
    MD_UI_TEMP_COUNT,
} md_ui_temperature_unit_t;

typedef enum {
    MD_UI_LANGUAGE_ENGLISH = 0,
    MD_UI_LANGUAGE_RUSSIAN,
    MD_UI_LANGUAGE_COUNT,
} md_ui_language_t;

typedef enum {
    MD_UI_CARD_GROUP_DASHBOARD_OEM = 0,
    MD_UI_CARD_GROUP_DASHBOARD_SPORT,
    MD_UI_CARD_GROUP_DASHBOARD_MINIMAL,
    MD_UI_CARD_GROUP_PERFORMANCE,
    MD_UI_CARD_GROUP_TEMPERATURES,
    MD_UI_CARD_GROUP_DRIVETRAIN,
    MD_UI_CARD_GROUP_CUSTOM = MD_UI_CARD_GROUP_COUNT,
    MD_UI_CARD_GROUP_NONE = 0xff,
} md_ui_card_group_t;

typedef enum {
    MD_UI_THRESHOLD_HIGH = 0,
    MD_UI_THRESHOLD_LOW,
} md_ui_threshold_direction_t;

typedef enum {
    MD_UI_SEVERITY_NORMAL = 0,
    MD_UI_SEVERITY_INFO,
    MD_UI_SEVERITY_WARNING,
    MD_UI_SEVERITY_CRITICAL,
} md_ui_severity_t;

typedef struct {
    uint8_t signal_id;
    bool enabled;
    md_ui_threshold_direction_t direction;
    int32_t warning_milli;
    int32_t critical_milli;
} md_ui_warning_threshold_t;

typedef struct {
    md_ui_dashboard_t dashboard;
    md_ui_layout_t layout;
    md_ui_theme_id_t theme;
    md_ui_day_night_t day_night;
    md_ui_unit_system_t units;
    md_ui_temperature_unit_t temperature_unit;
    md_ui_language_t language;
    uint8_t brightness_pct;
    uint8_t rotation; /* Quarter turns clockwise from board default orientation. */
    uint8_t last_screen;
    uint8_t widget_count;
    bool warnings_enabled;
    md_ui_widget_binding_t widgets[MD_UI_MAX_WIDGETS];
    md_ui_warning_threshold_t warnings[MD_UI_MAX_WARNING_THRESHOLDS];
    uint8_t fixed_card_overrides[MD_UI_FIXED_CARD_OVERRIDES_COUNT];
} md_ui_settings_t;

typedef enum {
    MD_UI_SCREEN_DASHBOARD = 0,
    MD_UI_SCREEN_PERFORMANCE,
    MD_UI_SCREEN_TEMPERATURES,
    MD_UI_SCREEN_DRIVETRAIN,
    MD_UI_SCREEN_DIAGNOSTICS,
    MD_UI_SCREEN_CUSTOM,
    MD_UI_SCREEN_SETTINGS,
    MD_UI_SCREEN_FOCUS,
    MD_UI_SCREEN_WIDGET_PICKER,
    MD_UI_SCREEN_CALIBRATION,
    MD_UI_SCREEN_CONFIG_MODE,
    /* Append-only: existing persisted screen IDs and modal IDs never move. */
    MD_UI_SCREEN_SPORT_BOOST,
    MD_UI_SCREEN_THERMALS_HEALTH,
    MD_UI_SCREEN_DPF_DIESEL,
    MD_UI_SCREEN_DRAG,
    MD_UI_SCREEN_NIGHT_MINIMAL,
    MD_UI_SCREEN_COUNT,
} md_ui_screen_t;
bool md_ui_screen_is_content(md_ui_screen_t screen);

typedef enum {
    MD_UI_ACTION_NEXT_SCREEN = 0,
    MD_UI_ACTION_PREVIOUS_SCREEN,
    MD_UI_ACTION_SELECT,
    MD_UI_ACTION_BACK,
    MD_UI_ACTION_OPEN_SETTINGS,
    MD_UI_ACTION_NEXT_ITEM,
    MD_UI_ACTION_PREVIOUS_ITEM,
    MD_UI_ACTION_INCREMENT,
    MD_UI_ACTION_DECREMENT,
    MD_UI_ACTION_SLEEP,
    MD_UI_ACTION_WAKE,
    MD_UI_ACTION_HOME,
    MD_UI_ACTION_OPEN_DETAILS,
} md_ui_action_t;

typedef enum {
    MD_UI_TOUCH_TAP = 0,
    MD_UI_TOUCH_DOUBLE_TAP,
    MD_UI_TOUCH_LONG_PRESS,
    MD_UI_TOUCH_GESTURE_COUNT,
} md_ui_touch_gesture_t;

typedef enum {
    MD_UI_INPUT_TOUCH = 0,
    MD_UI_INPUT_BUTTON,
    MD_UI_INPUT_STEERING_WHEEL,
    MD_UI_INPUT_STALK,
    MD_UI_INPUT_BLE,
    MD_UI_INPUT_CONSOLE,
    MD_UI_INPUT_SOURCE_COUNT,
} md_ui_input_source_t;

typedef enum {
    MD_UI_CONTEXT_UNKNOWN = 0,
    MD_UI_CONTEXT_PARKED,
    MD_UI_CONTEXT_DRIVING,
} md_ui_context_t;

typedef struct {
    md_ui_screen_t screen;
    md_ui_screen_t settings_return_screen;
    md_ui_screen_t focus_return_screen;
    md_ui_screen_t widget_picker_return_screen;
    md_ui_signal_id_t focused_signal;
    md_ui_signal_id_t widget_picker_signal;
    md_ui_card_group_t widget_picker_group;
    uint8_t selected_setting;
    uint8_t widget_picker_slot;
    bool sleeping;
    md_ui_context_t context;
} md_ui_state_t;

extern const md_ui_signal_definition_t md_ui_signal_definitions[MD_SIGNAL_COUNT];

const md_ui_signal_definition_t *md_ui_signal_definition(md_ui_signal_id_t id);
int md_ui_signal_id_from_name(const char *canonical_name);
void md_ui_signal_store_init(md_ui_signal_store_t *store);
bool md_ui_signal_store_publish_id(md_ui_signal_store_t *store,
                                   md_ui_signal_id_t id, double value,
                                   md_ui_signal_state_t state, uint32_t now_ms,
                                   uint32_t ttl_ms, bool demo);
bool md_ui_signal_store_publish(md_ui_signal_store_t *store,
                                const char *canonical_name, double value,
                                md_ui_signal_state_t state, uint32_t now_ms,
                                uint32_t ttl_ms, bool demo);
md_ui_signal_snapshot_t md_ui_signal_snapshot(const md_ui_signal_store_t *store,
                                              md_ui_signal_id_t id,
                                              uint32_t now_ms);
md_ui_signal_summary_t md_ui_signal_summary(const md_ui_signal_store_t *store,
                                            uint32_t now_ms);
md_ui_severity_t md_ui_signal_severity(const md_ui_settings_t *settings,
                                       md_ui_signal_id_t id, float value,
                                       md_ui_signal_state_t state);

void md_ui_settings_defaults(md_ui_settings_t *settings);
bool md_ui_settings_validate(const md_ui_settings_t *settings);
bool md_ui_settings_set_layout(md_ui_settings_t *settings,
                               md_ui_layout_t layout);
bool md_ui_settings_set_widget_signal(md_ui_settings_t *settings,
                                      uint8_t widget_index, int direction);
bool md_ui_settings_bind_widget_signal(md_ui_settings_t *settings,
                                       uint8_t widget_index,
                                       md_ui_signal_id_t signal);
md_ui_signal_id_t md_ui_settings_card_signal(const md_ui_settings_t *settings,
                                             md_ui_card_group_t group,
                                             uint8_t slot);
bool md_ui_settings_bind_card_signal(md_ui_settings_t *settings,
                                     md_ui_card_group_t group, uint8_t slot,
                                     md_ui_signal_id_t signal);
md_ui_card_group_t md_ui_card_group_for_screen(md_ui_screen_t screen,
                                               const md_ui_settings_t *settings);
bool md_ui_settings_encode(const md_ui_settings_t *settings, uint8_t *output,
                           size_t output_size, size_t *written);
bool md_ui_settings_decode(const uint8_t *input, size_t input_size,
                           md_ui_settings_t *settings);
bool md_ui_settings_set_warning(md_ui_settings_t *settings,
                                const char *canonical_signal,
                                bool enabled,
                                md_ui_threshold_direction_t direction,
                                int32_t warning_milli,
                                int32_t critical_milli);

void md_ui_state_init(md_ui_state_t *state,
                      const md_ui_settings_t *settings);
bool md_ui_state_select_setting(md_ui_state_t *state, uint8_t row);
bool md_ui_state_settings_page(md_ui_state_t *state, int direction);
bool md_ui_state_dispatch(md_ui_state_t *state,
                          md_ui_settings_t *settings,
                          md_ui_action_t action);
bool md_ui_state_open_details(md_ui_state_t *state,
                              md_ui_settings_t *settings,
                              md_ui_signal_id_t signal);
bool md_ui_state_open_widget_picker(md_ui_state_t *state,
                                    const md_ui_settings_t *settings,
                                    uint8_t widget_index);
bool md_ui_state_widget_picker_set_signal(md_ui_state_t *state,
                                          md_ui_signal_id_t signal);
bool md_ui_action_repeatable(md_ui_action_t action);
bool md_ui_state_set_context(md_ui_state_t *state,
                             md_ui_settings_t *settings,
                             md_ui_context_t context, bool verified);
bool md_ui_input_command(const char *command,
                         md_ui_input_source_t *source,
                         md_ui_action_t *action);
bool md_ui_input_valid(md_ui_input_source_t source,
                       md_ui_action_t action);
bool md_ui_touch_action(md_ui_screen_t screen, bool sleeping,
                        md_ui_touch_gesture_t gesture,
                        md_ui_action_t *action);
