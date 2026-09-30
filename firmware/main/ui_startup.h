#pragma once
#include <stdbool.h>
#include <stdint.h>
#define MD_UI_STARTUP_DURATION_MS 2000U
typedef struct {
    uint16_t width, height;
    uint8_t brightness, glow_opacity;
    bool complete;
} md_ui_startup_frame_t;
md_ui_startup_frame_t md_ui_startup_frame(uint32_t elapsed_ms, uint16_t width,
                                         uint16_t height, uint8_t target_brightness);
