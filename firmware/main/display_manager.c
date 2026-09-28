#include "display_manager.h"
#include <stdio.h>
#include <string.h>

bool md_display_profile_select(md_driver_t driver, uint8_t rotation,
                               md_display_profile_t *profile)
{
    if (!profile || rotation > 3) return false;

    uint16_t width;
    uint16_t height;
    switch (driver) {
    case MD_DRIVER_ST7789:
        width = rotation % 2 ? 240 : 320;
        height = rotation % 2 ? 320 : 240;
        break;
    case MD_DRIVER_ILI9341:
        width = rotation % 2 ? 320 : 240;
        height = rotation % 2 ? 240 : 320;
        break;
    default:
        return false;
    }

    *profile = (md_display_profile_t){driver, width, height, rotation, 16};
    return true;
}

bool md_display_orientation_select(const md_display_profile_t *profile,
                                   md_display_orientation_t *orientation)
{
    if (!profile || !orientation || profile->rotation > 3) return false;

    md_display_orientation_t selected = {0};
    switch (profile->driver) {
    case MD_DRIVER_ST7789:
        selected.swap_xy = profile->rotation % 2 == 0;
        selected.mirror_x = profile->rotation >= 2;
        selected.mirror_y = profile->rotation == 1 || profile->rotation == 2;
        /* This module's landscape profile uses the rotation-2 override. */
        if (profile->rotation == 2) selected.mirror_x = false;
        break;
    case MD_DRIVER_ILI9341:
        selected.swap_xy = profile->rotation % 2 != 0;
        selected.mirror_x = profile->rotation >= 2;
        selected.mirror_y = profile->rotation == 1 || profile->rotation == 2;
        break;
    default:
        return false;
    }

    *orientation = selected;
    return true;
}

bool md_display_open(md_display_manager_t *manager, md_display_backend_t *backend,
                     const md_display_profile_t *profile, uint16_t *frame)
{
    if (!manager || !backend || !profile || !backend->open || !backend->present) return false;
    memset(manager, 0, sizeof(*manager));
    manager->profile = *profile;
    manager->backend = backend;
    manager->frame = frame;
    manager->ready = backend->open(backend, profile);
    return manager->ready;
}

/* The direct RGB565 path remains for the panel-only bench smoke test. */
static const uint8_t font[][5] = {
    {0,0,0,0,0}, {0x7e,0x11,0x11,0x7e,0}, {0x7f,0x49,0x49,0x36,0},
    {0x3e,0x41,0x41,0x22,0}, {0x7f,0x41,0x41,0x3e,0},
    {0x7f,0x49,0x49,0x41,0}, {0x7f,0x09,0x09,0x01,0},
    {0x3e,0x41,0x51,0x72,0}, {0x7f,0x08,0x08,0x7f,0},
    {0x41,0x7f,0x41,0,0}, {0x20,0x40,0x41,0x3f,0},
    {0x7f,0x08,0x14,0x63,0}, {0x7f,0x40,0x40,0x40,0},
    {0x7f,0x02,0x0c,0x02,0x7f}, {0x7f,0x04,0x08,0x7f,0},
    {0x3e,0x41,0x41,0x3e,0}, {0x7f,0x09,0x09,0x06,0},
    {0x3e,0x41,0x51,0x3e,0x40}, {0x7f,0x09,0x19,0x66,0},
    {0x26,0x49,0x49,0x32,0}, {0x01,0x7f,0x01,0,0},
    {0x3f,0x40,0x40,0x3f,0}, {0x1f,0x20,0x40,0x20,0x1f},
    {0x7f,0x20,0x18,0x20,0x7f}, {0x63,0x14,0x08,0x14,0x63},
    {0x03,0x04,0x78,0x04,0x03}, {0x61,0x51,0x49,0x45,0x43}
};
static const uint8_t digits[][5] = {
    {0x3e,0x51,0x49,0x45,0x3e}, {0,0x42,0x7f,0x40,0},
    {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4b,0x31},
    {0x18,0x14,0x12,0x7f,0x10}, {0x27,0x45,0x45,0x45,0x39},
    {0x3c,0x4a,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1e}
};
static const uint8_t *glyph(char c)
{
    static const uint8_t dash[5]={0x08,0x08,0x08,0x08,0};
    static const uint8_t dot[5]={0,0x60,0x60,0,0};
    static const uint8_t slash[5]={0x60,0x10,0x08,0x04,0x03};
    if (c >= 'A' && c <= 'Z') return font[c-'A'+1];
    if (c >= '0' && c <= '9') return digits[c-'0'];
    if (c == '-') return dash;
    if (c == '.') return dot;
    if (c == '/') return slash;
    return font[0];
}
static void pixel(md_display_manager_t *m, int x, int y, uint16_t color)
{
    if (x >= 0 && x < m->profile.width && y >= 0 && y < m->profile.height)
        m->frame[y*m->profile.width+x] = color;
}
static void label(md_display_manager_t *m, int x, int y, const char *s, int scale, uint16_t color)
{
    for (; *s; ++s, x += 6*scale) {
        const uint8_t *g = glyph(*s);
        for (int col=0; col<5; ++col) for (int row=0; row<7; ++row)
            if (g[col] & (1U<<row)) for (int dx=0; dx<scale; ++dx)
                for (int dy=0; dy<scale; ++dy) pixel(m,x+col*scale+dx,y+row*scale+dy,color);
    }
}
void md_display_power(md_display_manager_t *m, bool on)
{
    if (m && m->ready && m->backend->power) m->backend->power(m->backend,on);
}

esp_lcd_panel_handle_t md_display_panel(md_display_manager_t *m)
{
    return m && m->ready && m->backend->panel_handle
        ? m->backend->panel_handle(m->backend) : NULL;
}

esp_lcd_panel_io_handle_t md_display_io(md_display_manager_t *m)
{
    return m && m->ready && m->backend->io_handle
        ? m->backend->io_handle(m->backend) : NULL;
}

bool md_display_set_brightness(md_display_manager_t *m, uint8_t brightness_pct)
{
    return m && m->ready && brightness_pct <= 100 && m->backend->set_brightness
        ? m->backend->set_brightness(m->backend, brightness_pct) : false;
}

bool md_display_present_solid(md_display_manager_t *m, uint16_t rgb565)
{
    if (!m || !m->ready || !m->frame || !m->backend || !m->backend->present) return false;
    const size_t pixels = (size_t)m->profile.width * m->profile.height;
    for (size_t i = 0; i < pixels; ++i) m->frame[i] = rgb565;
    return m->backend->present(m->backend, m->frame);
}

bool md_display_render_smoke_ok(md_display_manager_t *m)
{
    if (!m || !m->ready || !m->frame || !m->backend || !m->backend->present) return false;
    const size_t pixels = (size_t)m->profile.width * m->profile.height;
    for (size_t i = 0; i < pixels; ++i) m->frame[i] = 0x0000;

    const int title_scale = 3;
    const int subtitle_scale = 2;
    const int title_width = (int)strlen("MINIDASH") * 6 * title_scale;
    const int subtitle_width = (int)strlen("DISPLAY OK") * 6 * subtitle_scale;
    label(m, (m->profile.width - title_width) / 2, m->profile.height / 2 - 27,
          "MINIDASH", title_scale, 0xffff);
    label(m, (m->profile.width - subtitle_width) / 2, m->profile.height / 2 + 10,
          "DISPLAY OK", subtitle_scale, 0x07ff);
    return m->backend->present(m->backend, m->frame);
}
