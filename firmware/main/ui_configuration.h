#pragma once
#include "ui_model.h"
#include "derived_engine.h"

#define MD_CONFIG_SCHEMA_VERSION 4U
#define MD_CONFIG_MAX_PAGES 4U
#define MD_CONFIG_TEXT_SIZE 32U
#define MD_CONFIG_WIRE_MAX 4096U
#define MD_CONFIG_ANALYTICS_WIRE_SIZE 177U
typedef struct {
    uint8_t signal, type, precision;
    uint8_t column, row, columns, rows;
    float minimum, maximum; /* Canonical units; presentation ranges, not safety limits. */
    char label[MD_CONFIG_TEXT_SIZE];
} md_ui_page_widget_t;
typedef struct {
    char title[MD_CONFIG_TEXT_SIZE];
    uint8_t layout, widget_count;
    md_ui_page_widget_t widgets[MD_UI_MAX_WIDGETS];
} md_ui_dashboard_page_t;
typedef struct {
    uint8_t dashboard, theme, day_night, units, temperature_unit, language, last_screen;
    uint8_t page_count, active_page;
    bool warnings_enabled, custom_theme, gauge_auto;
    md_ui_warning_threshold_t warnings[MD_UI_MAX_WARNING_THRESHOLDS];
    uint8_t fixed_card_overrides[MD_UI_FIXED_CARD_OVERRIDES_COUNT];
    md_ui_palette_t palette;
    struct { float minimum, maximum; } gauge_ranges[MD_SIGNAL_COUNT];
    md_analytics_config_t analytics;
    md_ui_dashboard_page_t pages[MD_CONFIG_MAX_PAGES];
} md_common_configuration_t;
typedef struct {
    /* Optional platform section: zero capabilities means no platform settings. */
    uint32_t present;
    uint8_t brightness, brightness_mode, rotation;
} md_platform_configuration_t;
typedef struct {
    uint32_t schema_version;
    md_common_configuration_t common;
    md_platform_configuration_t platform;
} md_device_configuration_t;

bool md_configuration_from_legacy(md_device_configuration_t *config,
                                   const md_ui_settings_t *settings,
                                   const md_target_profile_t *target);
bool md_configuration_to_legacy(const md_device_configuration_t *config,
                                 md_ui_settings_t *settings);
bool md_configuration_update_legacy(md_device_configuration_t *config,
                                     const md_ui_settings_t *settings);
bool md_configuration_validate(const md_device_configuration_t *config,
                                const md_target_profile_t *target);
bool md_configuration_set_layout(md_ui_dashboard_page_t *page, md_ui_layout_t layout);
bool md_configuration_encode(const md_device_configuration_t *config,
                              uint8_t *wire, size_t capacity, size_t *written);
bool md_configuration_decode(const uint8_t *wire, size_t length,
                              md_device_configuration_t *config);
void md_configuration_reset_theme(md_device_configuration_t *config);
#define MD_ANALYTICS_PRESET_COUNT 6U
typedef struct { const char *name; uint8_t signals[8]; } md_analytics_preset_t;
extern const md_analytics_preset_t md_analytics_presets[MD_ANALYTICS_PRESET_COUNT];
bool md_configuration_analytics_preset(md_device_configuration_t *config,uint8_t page,uint8_t preset);
bool md_configuration_reset_page(md_device_configuration_t *config, uint8_t page);
bool md_configuration_reset_ui(md_device_configuration_t *config,
                               const md_target_profile_t *target);
