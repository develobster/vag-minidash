#pragma once
#include "ui_core.h"
#include "ui_theme.h"
#include "ui_platform.h"

#define MD_UI_HISTORY_SAMPLES 16U
typedef struct {
    float samples[MD_UI_HISTORY_SAMPLES];
    float minimum, maximum;
    uint32_t last_update_ms;
    uint8_t count, head;
    bool seen, valid, demo;
} md_ui_history_t;
typedef struct {
    md_ui_signal_snapshot_t signal;
    md_ui_severity_t severity;
    bool selected, inverse, has_range, has_history;
    uint16_t level_per_mille;
    float minimum, maximum;
    char value[32], unit[12];
} md_ui_widget_view_t;

void md_ui_history_observe(md_ui_history_t *history,
                          const md_ui_signal_record_t *record, uint32_t now_ms);
void md_ui_project_widget(md_ui_widget_view_t *view, md_ui_signal_id_t signal,
                         const md_ui_settings_t *settings,
                         const md_ui_signal_store_t *store, uint32_t now_ms,
                         float range_minimum, float range_maximum,
                         const md_ui_history_t *history);
/* Layout adaptation operates on logical bindings, never on LVGL objects. */
bool md_ui_presentation_slice(uint8_t widget_count, uint8_t capacity,
                              uint8_t subpage, uint8_t *first, uint8_t *count);
typedef struct {
    char firmware_version[32];
    uint32_t uptime_seconds;
    size_t free_heap_bytes;
    size_t minimum_free_heap_bytes;
    uint32_t refreshes_per_second;
    uint32_t average_render_ms;
    bool render_metrics_available;
    bool display_ready;
    bool touch_ready;
    bool brightness_control;
    bool wifi_active;
    bool transport_available;
    char transport_state[16];
    bool ecu_count_available;
    uint16_t ecu_count;
    uint16_t selected_ecu;
    uint32_t can_rx, can_tx, can_tx_failed, can_timeouts, can_dropped;
    uint32_t can_arbitration_lost, can_bus_errors;
    char dtc_text[24];
    md_ui_signal_summary_t signal_summary;
} md_ui_diagnostics_t;

typedef struct {
    void *context;
    void (*show)(void *context, md_ui_screen_t screen, const md_ui_settings_t *settings);
    void (*update)(void *context, md_ui_screen_t screen,
                   const md_ui_settings_t *settings, const md_ui_state_t *state,
                   const md_ui_signal_store_t *store, uint32_t now_ms,
                   const md_ui_diagnostics_t *diagnostics);
} md_ui_renderer_t;

typedef struct {
    md_ui_action_t short_action, long_action;
    bool raw_pressed, stable_pressed, long_sent, suppressed_until_release;
    uint32_t raw_changed_ms, pressed_ms, repeat_ms;
} md_ui_button_t;
void md_ui_button_init(md_ui_button_t *button, md_ui_action_t short_action,
                       md_ui_action_t long_action);
bool md_ui_button_sample(md_ui_button_t *button, bool pressed, uint32_t now_ms,
                         bool lockout, md_ui_action_t *action);
bool md_ui_vehicle_button_action(bool evidence_verified, md_ui_action_t mapped,
                                 md_ui_action_t *action);

typedef enum { MD_BRIGHTNESS_FIXED, MD_BRIGHTNESS_MANUAL, MD_BRIGHTNESS_AUTO } md_brightness_mode_t;
typedef struct {
    md_brightness_mode_t mode;
    uint8_t minimum, maximum, target, current;
    bool dimming, ambient, screen_on;
    uint32_t last_step_ms;
} md_brightness_t;
void md_brightness_init(md_brightness_t *brightness, const md_target_profile_t *target,
                        uint8_t level);
bool md_brightness_set(md_brightness_t *brightness, uint8_t level);
bool md_brightness_mode(md_brightness_t *brightness, md_brightness_mode_t mode);
bool md_brightness_tick(md_brightness_t *brightness, uint32_t now_ms, uint8_t *output);

/* Logical 320x240 scene coordinates, scaled only after portrait reflow. */
typedef struct { int x,y,width,height; } md_ui_rect_t;
void md_ui_fixed_portrait_rect(md_ui_card_group_t group, uint8_t slot, md_ui_rect_t *rect);
md_ui_rect_t md_ui_widget_rect(const md_ui_widget_binding_t *widget, bool portrait);

/* Presentation bounds in canonical units, never warning/safety thresholds. */
bool md_ui_focus_gauge_range(md_ui_signal_id_t signal, float *low, float *high);
