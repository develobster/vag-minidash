#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

typedef enum {
    MD_DRIVER_ST7789,
    MD_DRIVER_SSD1306,
    MD_DRIVER_SH1106,
    MD_DRIVER_GC9A01,
    MD_DRIVER_ILI9341,
} md_driver_t;

typedef struct {
    md_driver_t driver;
    uint16_t width;
    uint16_t height;
    uint8_t rotation;
    uint8_t color_depth;
} md_display_profile_t;

typedef struct {
    bool swap_xy;
    bool mirror_x;
    bool mirror_y;
} md_display_orientation_t;

typedef struct md_display_backend md_display_backend_t;
struct md_display_backend {
    void *context;
    bool (*open)(md_display_backend_t *, const md_display_profile_t *);
    bool (*present)(md_display_backend_t *, const uint16_t *rgb565);
    void (*power)(md_display_backend_t *, bool on);
    bool (*set_brightness)(md_display_backend_t *, uint8_t brightness_pct);
    esp_lcd_panel_handle_t (*panel_handle)(md_display_backend_t *);
    esp_lcd_panel_io_handle_t (*io_handle)(md_display_backend_t *);
};

typedef struct {
    md_display_profile_t profile;
    md_display_backend_t *backend;
    uint16_t *frame;
    bool ready;
} md_display_manager_t;

bool md_display_profile_select(md_driver_t driver, uint8_t rotation,
                               md_display_profile_t *profile);
bool md_display_orientation_select(const md_display_profile_t *profile,
                                   md_display_orientation_t *orientation);
bool md_display_open(md_display_manager_t *manager, md_display_backend_t *backend,
                     const md_display_profile_t *profile, uint16_t *smoke_frame);
esp_lcd_panel_handle_t md_display_panel(md_display_manager_t *manager);
esp_lcd_panel_io_handle_t md_display_io(md_display_manager_t *manager);
bool md_display_set_brightness(md_display_manager_t *manager, uint8_t brightness_pct);
void md_display_power(md_display_manager_t *manager, bool on);
bool md_display_present_solid(md_display_manager_t *manager, uint16_t rgb565);
bool md_display_render_smoke_ok(md_display_manager_t *manager);
