#include "display_manager.h"
#include <stdio.h>
#include <string.h>

bool md_display_profile_select(md_driver_t driver, uint8_t rotation,
                               md_display_profile_t *profile)
{
    if (!profile || rotation > 3 || driver != MD_DRIVER_ST7789) return false;
    *profile = (md_display_profile_t){driver,
        rotation % 2 ? 240 : 320, rotation % 2 ? 320 : 240,
        rotation, 16, MD_TOUCH_UNKNOWN, true};
    return true;
}

bool md_display_open(md_display_manager_t *manager, md_display_backend_t *backend,
                     const md_display_profile_t *profile, uint16_t *frame)
{
    if (!manager || !backend || !profile || !frame || !backend->open || !backend->present) return false;
    memset(manager, 0, sizeof(*manager));
    manager->profile = *profile;
    manager->backend = backend;
    manager->frame = frame;
    manager->ready = backend->open(backend, profile);
    return manager->ready;
}

/* Five-column uppercase glyphs. Rendering is deliberately independent of the panel driver. */
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
static void field(md_display_manager_t *m, int x, int y, const char *title,
                  bool available, float value, const char *unit, int decimals)
{
    char text[32];
    label(m,x,y,title,2,0x7bef);
    if (available) snprintf(text,sizeof(text),decimals ? "%.1f %s" : "%.0f %s",value,unit);
    else snprintf(text,sizeof(text),"-- %s",unit);
    label(m,x,y+22,text,2,0xffff);
}
void md_display_render(md_display_manager_t *m, const md_ui_model_t *v)
{
    if (!m || !m->ready || v->sleeping) return;
    for (unsigned i=0; i<(unsigned)m->profile.width*m->profile.height; ++i) m->frame[i]=0x0841;
    const bool wide=m->profile.width>=320;
    if (v->page==MD_PAGE_DRIVE) {
        field(m,8,8,"RPM",v->available[MD_RPM],v->value[MD_RPM],"",0);
        field(m,wide?170:8,wide?8:55,"SPEED",v->available[MD_SPEED],v->value[MD_SPEED],"KM/H",0);
        field(m,8,wide?83:102,"BOOST",v->boost_available,v->boost_bar,"BAR",1);
        field(m,wide?170:8,wide?83:149,"VOLT",v->available[MD_BATTERY_VOLTAGE],v->value[MD_BATTERY_VOLTAGE],"V",1);
    } else {
        field(m,8,8,"OIL",v->available[MD_OIL_TEMP],v->value[MD_OIL_TEMP],"C",0);
        field(m,wide?170:8,wide?8:60,"COOLANT",v->available[MD_COOLANT_TEMP],v->value[MD_COOLANT_TEMP],"C",0);
        field(m,8,wide?95:112,"RPM",v->available[MD_RPM],v->value[MD_RPM],"",0);
    }
    label(m,8,m->profile.height-22,v->page==MD_PAGE_DRIVE?"PAGE 1/2":"PAGE 2/2",2,0x07ff);
    label(m,m->profile.width-62,m->profile.height-22,"DEMO",2,0xf800);
    m->backend->present(m->backend,m->frame);
}
void md_display_power(md_display_manager_t *m, bool on)
{
    if (m && m->ready && m->backend->power) m->backend->power(m->backend,on);
}
