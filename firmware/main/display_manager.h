#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "display_model.h"

typedef enum { MD_DRIVER_ST7789, MD_DRIVER_SSD1306, MD_DRIVER_SH1106,
               MD_DRIVER_GC9A01, MD_DRIVER_ILI9341 } md_driver_t;
typedef enum { MD_TOUCH_UNKNOWN, MD_TOUCH_ABSENT, MD_TOUCH_PRESENT } md_touch_t;
typedef struct {
    md_driver_t driver;
    uint16_t width, height;
    uint8_t rotation;
    uint8_t color_depth;
    md_touch_t touch;
    bool backlight_capable;
} md_display_profile_t;

typedef struct md_display_backend md_display_backend_t;
struct md_display_backend {
    void *context;
    bool (*open)(md_display_backend_t *, const md_display_profile_t *);
    bool (*present)(md_display_backend_t *, const uint16_t *rgb565);
    void (*power)(md_display_backend_t *, bool on);
};

typedef struct {
    md_display_profile_t profile;
    md_display_backend_t *backend;
    uint16_t *frame;
    bool ready;
} md_display_manager_t;

bool md_display_profile_select(md_driver_t driver, uint8_t rotation,
                               md_display_profile_t *profile);
bool md_display_open(md_display_manager_t *manager, md_display_backend_t *backend,
                     const md_display_profile_t *profile, uint16_t *frame);
void md_display_render(md_display_manager_t *manager, const md_ui_model_t *model);
void md_display_power(md_display_manager_t *manager, bool on);
