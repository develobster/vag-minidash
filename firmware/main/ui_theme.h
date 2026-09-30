#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "ui_core.h"

typedef struct {
    uint32_t background;
    uint32_t surface;
    uint32_t surface_alt;
    uint32_t border;
    uint32_t text;
    uint32_t muted;
    uint32_t accent;
    uint32_t normal;
    uint32_t warning;
    uint32_t critical;
    uint32_t unavailable;
} md_ui_palette_t;

md_ui_palette_t md_ui_theme_palette(md_ui_theme_id_t theme, bool day_mode);
uint32_t md_ui_theme_signal_color(const md_ui_palette_t *palette,
                                  md_ui_signal_state_t state,
                                  md_ui_severity_t severity);
void md_ui_format_signal(const md_ui_signal_definition_t *definition,
                         const md_ui_signal_snapshot_t *snapshot,
                         const md_ui_settings_t *settings,
                         char *value, size_t value_size,
                         char *unit, size_t unit_size);
const char *md_ui_signal_state_label(md_ui_signal_state_t state);

void md_ui_format_signal_precision(const md_ui_signal_definition_t *definition,
    const md_ui_signal_snapshot_t *snapshot, const md_ui_settings_t *settings,
    uint8_t precision, char *value, size_t value_size, char *unit, size_t unit_size);

typedef struct {
    uint8_t radius, border_width, glow_width, glow_opacity, scanline_opacity, letter_spacing;
    uint8_t font_family, frame_kind, gauge_style; /* 0=sans, 1=mono, 2=cluster digits */
} md_ui_theme_style_t;
const char *md_ui_theme_name(md_ui_theme_id_t theme);
md_ui_theme_style_t md_ui_theme_style(md_ui_theme_id_t theme);
