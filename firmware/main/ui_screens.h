#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "ui_core.h"
#include "ui_theme.h"

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
    bool transport_available;
    char transport_state[16];
    bool ecu_count_available;
    uint16_t ecu_count;
    md_ui_signal_summary_t signal_summary;
} md_ui_diagnostics_t;

bool md_ui_screens_create(uint16_t width, uint16_t height,
                          const md_ui_settings_t *settings,
                          const md_ui_palette_t *palette);
void md_ui_screens_show(md_ui_screen_t screen,
                        const md_ui_settings_t *settings);
void md_ui_screens_retheme(const md_ui_palette_t *palette);
void md_ui_screens_update(md_ui_screen_t screen,
                          const md_ui_settings_t *settings,
                          const md_ui_state_t *state,
                          const md_ui_signal_store_t *signals,
                          uint32_t now_ms,
                          const md_ui_diagnostics_t *diagnostics);
