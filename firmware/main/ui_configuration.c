#include "ui_configuration.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool text_valid(const char *text, size_t size, bool empty_allowed)
{
    size_t i = 0;
    if (!text || (!empty_allowed && !text[0])) return false;
    while (i < size && text[i]) {
        const unsigned char c = (unsigned char)text[i++];
        if (c < 0x20U || c == 0x7fU) return false;
        if (c < 0x80U) continue;
        /* UI fonts support Latin and Cyrillic; reject invalid UTF-8. */
        unsigned n; uint32_t cp;
        if (c >= 0xc2U && c <= 0xdfU) { n = 1; cp = c & 0x1fU; }
        else if (c >= 0xe0U && c <= 0xefU) { n = 2; cp = c & 0x0fU; }
        else return false;
        if (i + n >= size) return false;
        for (unsigned j = 0; j < n; ++j) {
            const unsigned char next = (unsigned char)text[i++];
            if ((next & 0xc0U) != 0x80U) return false;
            cp = (cp << 6) | (next & 0x3fU);
        }
        if ((n == 1 && cp < 0x80U) || (n == 2 && cp < 0x800U) ||
            (cp >= 0xd800U && cp <= 0xdfffU)) return false;
        if (cp > 0x7fU && cp != 0xb0U && cp != 0x2022U &&
            !(cp >= 0x410U && cp <= 0x44fU) && cp != 0x401U && cp != 0x451U) return false;
    }
    return i < size;
}
static void copy_common_from_settings(md_common_configuration_t *c, const md_ui_settings_t *s)
{
    c->dashboard = s->dashboard; c->theme = s->theme; c->day_night = s->day_night;
    c->units = s->units; c->temperature_unit = s->temperature_unit;
    c->language = s->language; c->last_screen = s->last_screen;
    c->warnings_enabled = s->warnings_enabled;
    memcpy(c->warnings, s->warnings, sizeof(c->warnings));
    memcpy(c->fixed_card_overrides, s->fixed_card_overrides, sizeof(c->fixed_card_overrides));
}
static void page_from_settings(md_ui_dashboard_page_t *page, const md_ui_settings_t *s)
{
    page->layout = s->layout; page->widget_count = s->widget_count;
    for (unsigned i = 0; i < s->widget_count; ++i) {
        const md_ui_widget_binding_t *w = &s->widgets[i];
        md_ui_page_widget_t *out = &page->widgets[i];
        const int signal = md_ui_signal_id_from_name(w->canonical_signal);
        const bool same = signal >= 0 && out->signal == signal && out->maximum > out->minimum;
        if (!same) {
            out->minimum = 0; out->maximum = 100;
            out->precision = signal >= 0 ? md_ui_signal_definitions[signal].decimals : 0;
            out->label[0] = '\0';
        }
        out->signal = signal < 0 ? UINT8_MAX : (uint8_t)signal;
        out->type = w->type; out->column = w->column; out->row = w->row;
        out->columns = w->columns; out->rows = w->rows;
    }
}
bool md_configuration_from_legacy(md_device_configuration_t *c, const md_ui_settings_t *s,
                                   const md_target_profile_t *target)
{
    if (!c || !md_ui_settings_validate(s)) return false;
    memset(c, 0, sizeof(*c)); c->schema_version = MD_CONFIG_SCHEMA_VERSION;
    copy_common_from_settings(&c->common, s);
    c->common.page_count = 1;
    snprintf(c->common.pages[0].title, MD_CONFIG_TEXT_SIZE, "CUSTOM");
    for (unsigned i = 0; i < MD_UI_MAX_WIDGETS; ++i) c->common.pages[0].widgets[i].signal = UINT8_MAX;
    page_from_settings(&c->common.pages[0], s);
    c->common.palette = md_ui_theme_palette(s->theme, s->day_night == MD_UI_DAY_NIGHT_DAY);
    if (md_target_has(target, MD_CAP_COLOR)) {
        c->platform.present = MD_CAP_COLOR;
        c->platform.brightness = md_target_has(target, MD_CAP_DIMMING) && s->brightness_pct < 10
            ? 10 : s->brightness_pct;
        c->platform.rotation = s->rotation;
        c->platform.brightness_mode = md_target_has(target, MD_CAP_DIMMING)
            ? MD_BRIGHTNESS_MANUAL : MD_BRIGHTNESS_FIXED;
        if (md_target_has(target, MD_CAP_DIMMING)) c->platform.present |= MD_CAP_DIMMING;
    }
    return md_configuration_validate(c, target);
}
static bool settings_for_page(const md_device_configuration_t *c, uint8_t page_index, md_ui_settings_t *s)
{
    if (!c || !s || page_index >= c->common.page_count ||
        c->common.page_count > MD_CONFIG_MAX_PAGES) return false;
    md_ui_settings_defaults(s);
    const md_common_configuration_t *common = &c->common;
    s->dashboard = common->dashboard; s->theme = common->theme; s->day_night = common->day_night;
    s->units = common->units; s->temperature_unit = common->temperature_unit;
    s->language = common->language; s->last_screen = common->last_screen;
    s->warnings_enabled = common->warnings_enabled;
    memcpy(s->warnings, common->warnings, sizeof(s->warnings));
    memcpy(s->fixed_card_overrides, common->fixed_card_overrides, sizeof(s->fixed_card_overrides));
    s->brightness_pct = c->platform.present ? c->platform.brightness : 100;
    s->rotation = c->platform.present ? c->platform.rotation : 0;
    const md_ui_dashboard_page_t *page = &common->pages[page_index];
    s->layout = page->layout; s->widget_count = page->widget_count;
    if (page->widget_count > MD_UI_MAX_WIDGETS) return false;
    for (unsigned i = 0; i < page->widget_count; ++i) {
        const md_ui_page_widget_t *w = &page->widgets[i];
        if (w->signal >= MD_SIGNAL_COUNT) return false;
        s->widgets[i] = (md_ui_widget_binding_t){.id = i + 1U, .type = w->type,
            .column = w->column, .row = w->row, .columns = w->columns, .rows = w->rows};
        snprintf(s->widgets[i].canonical_signal, MD_UI_SIGNAL_NAME_LEN, "%s",
                 md_ui_signal_definitions[w->signal].canonical_name);
    }
    return md_ui_settings_validate(s);
}
bool md_configuration_to_legacy(const md_device_configuration_t *c, md_ui_settings_t *s)
{
    return c && settings_for_page(c,c->common.active_page,s);
}
bool md_configuration_update_legacy(md_device_configuration_t *c, const md_ui_settings_t *s)
{
    if (!c || !md_ui_settings_validate(s) || c->common.active_page >= c->common.page_count) return false;
    if (c->common.theme!=s->theme || c->common.day_night!=s->day_night) c->common.custom_theme=false;
    copy_common_from_settings(&c->common, s);
    page_from_settings(&c->common.pages[c->common.active_page], s);
    if (c->platform.present) { c->platform.brightness = s->brightness_pct; c->platform.rotation = s->rotation; }
    return true;
}
bool md_configuration_set_layout(md_ui_dashboard_page_t *page, md_ui_layout_t layout)
{
    if (!page) return false;
    md_ui_settings_t s; md_ui_settings_defaults(&s);
    if (!md_ui_settings_set_layout(&s, layout)) return false;
    for (unsigned i = 0; i < MD_UI_MAX_WIDGETS; ++i) {
        if (i >= page->widget_count) {
            memset(&page->widgets[i], 0, sizeof(page->widgets[i]));
            page->widgets[i].signal = i % MD_SIGNAL_COUNT;
            page->widgets[i].precision = md_ui_signal_definitions[page->widgets[i].signal].decimals;
            page->widgets[i].maximum = 100;
        }
        page->widgets[i].column = s.widgets[i].column; page->widgets[i].row = s.widgets[i].row;
        page->widgets[i].columns = s.widgets[i].columns; page->widgets[i].rows = s.widgets[i].rows;
    }
    page->layout = layout; page->widget_count = s.widget_count;
    return true;
}
bool md_configuration_validate(const md_device_configuration_t *c, const md_target_profile_t *target)
{
    if (!c || c->schema_version != MD_CONFIG_SCHEMA_VERSION || !c->common.page_count ||
        c->common.page_count > MD_CONFIG_MAX_PAGES || c->common.active_page >= c->common.page_count ||
        (c->platform.present & ~(MD_CAP_COLOR | MD_CAP_DIMMING)) || c->platform.rotation > 3 ||
        c->platform.brightness > 100 || c->platform.brightness_mode > MD_BRIGHTNESS_AUTO) return false;
    if (target && md_target_has(target, MD_CAP_DIMMING) && c->platform.present &&
        c->platform.brightness < 10) return false;
    if (target && ((c->platform.present & target->hardware) != c->platform.present ||
        (c->platform.brightness_mode != MD_BRIGHTNESS_FIXED && !md_target_has(target, MD_CAP_DIMMING)) ||
        (c->platform.brightness_mode == MD_BRIGHTNESS_AUTO && !md_target_has(target, MD_CAP_AMBIENT_SOURCE)))) return false;
    uint32_t colors[sizeof(c->common.palette) / sizeof(uint32_t)];
    memcpy(colors, &c->common.palette, sizeof(colors));
    for (unsigned i = 0; i < sizeof(c->common.palette) / sizeof(uint32_t); ++i)
        if (colors[i] > 0xffffffU) return false;
    for (unsigned p = 0; p < c->common.page_count; ++p) {
        const md_ui_dashboard_page_t *page = &c->common.pages[p];
        if (!text_valid(page->title, sizeof(page->title), false) ||
            !page->widget_count || page->widget_count > MD_UI_MAX_WIDGETS) return false;
        for (unsigned w = 0; w < page->widget_count; ++w) {
            const md_ui_page_widget_t *widget = &page->widgets[w];
            if (widget->signal >= MD_SIGNAL_COUNT || widget->type >= MD_UI_WIDGET_COUNT ||
                widget->precision > 3 || !text_valid(widget->label, sizeof(widget->label), true) ||
                !isfinite(widget->minimum) || !isfinite(widget->maximum) ||
                widget->maximum <= widget->minimum || fabsf(widget->minimum) > 1000000.0f ||
                fabsf(widget->maximum) > 1000000.0f) return false;
        }
        md_ui_settings_t settings;
        if (!settings_for_page(c,p,&settings)) return false;
    }
    return true;
}

/* A bounded canonical little-endian codec; no ABI structs are copied to flash. */
typedef struct { uint8_t *bytes; size_t size, offset; bool ok; } writer_t;
typedef struct { const uint8_t *bytes; size_t size, offset; bool ok; } reader_t;
static void put8(writer_t *w, uint8_t n) { if (w->offset < w->size) w->bytes[w->offset++] = n; else w->ok = false; }
static void put32(writer_t *w, uint32_t n) { for (unsigned i = 0; i < 4; ++i) put8(w, (uint8_t)(n >> (i * 8))); }
static uint8_t get8(reader_t *r) { if (r->offset < r->size) return r->bytes[r->offset++]; r->ok = false; return 0; }
static uint32_t get32(reader_t *r) { uint32_t n = 0; for (unsigned i = 0; i < 4; ++i) n |= (uint32_t)get8(r) << (i * 8); return n; }
static void puttext(writer_t *w, const char *s) { const size_t n = strlen(s); put8(w, n); for (size_t i = 0; i < n; ++i) put8(w, s[i]); }
static void read_text_field(reader_t *r, char *s) { const uint8_t n = get8(r); if (n >= MD_CONFIG_TEXT_SIZE) { r->ok = false; return; } for (unsigned i = 0; i < n; ++i) s[i] = get8(r); s[n] = 0; }
static uint32_t checksum(const uint8_t *s, size_t n) { uint32_t c = UINT32_MAX; while (n--) { c ^= *s++; for (unsigned i = 0; i < 8; ++i) c = (c >> 1) ^ (0xedb88320U & (uint32_t)-(int32_t)(c & 1U)); } return ~c; }
static uint32_t floatbits(float value) { uint32_t result; memcpy(&result, &value, 4); return result; }
static float bitsfloat(uint32_t value) { float result; memcpy(&result, &value, 4); return result; }
bool md_configuration_encode(const md_device_configuration_t *c, uint8_t *wire, size_t capacity, size_t *written)
{
    if (written) *written = 0;
    if (!wire || !md_configuration_validate(c, NULL)) return false;
    writer_t w = {wire, capacity, 0, true};
    put32(&w, 0x3143444dU); put8(&w, MD_CONFIG_SCHEMA_VERSION);
    const md_common_configuration_t *s = &c->common;
    put8(&w, s->dashboard); put8(&w, s->theme); put8(&w, s->day_night); put8(&w, s->units);
    put8(&w, s->temperature_unit); put8(&w, s->language); put8(&w, s->last_screen);
    put8(&w, s->page_count); put8(&w, s->active_page); put8(&w, s->warnings_enabled); put8(&w, s->custom_theme);
    uint32_t colors[sizeof(s->palette) / sizeof(uint32_t)];
    memcpy(colors, &s->palette, sizeof(colors));
    for (unsigned i = 0; i < sizeof(s->palette) / 4; ++i) put32(&w, colors[i]);
    for (unsigned i = 0; i < MD_UI_MAX_WARNING_THRESHOLDS; ++i) {
        const md_ui_warning_threshold_t *t = &s->warnings[i];
        put8(&w, t->signal_id); put8(&w, t->enabled); put8(&w, t->direction);
        put32(&w, (uint32_t)t->warning_milli); put32(&w, (uint32_t)t->critical_milli);
    }
    for (unsigned i = 0; i < MD_UI_FIXED_CARD_OVERRIDES_COUNT; ++i) put8(&w, s->fixed_card_overrides[i]);
    for (unsigned p = 0; p < s->page_count; ++p) {
        const md_ui_dashboard_page_t *page = &s->pages[p];
        puttext(&w, page->title); put8(&w, page->layout); put8(&w, page->widget_count);
        for (unsigned i = 0; i < page->widget_count; ++i) {
            const md_ui_page_widget_t *v = &page->widgets[i];
            put8(&w, v->signal); put8(&w, v->type); put8(&w, v->precision);
            put8(&w, v->column); put8(&w, v->row); put8(&w, v->columns); put8(&w, v->rows);
            put32(&w, floatbits(v->minimum)); put32(&w, floatbits(v->maximum)); puttext(&w, v->label);
        }
    }
    put32(&w, c->platform.present);
    if (c->platform.present) { put8(&w, c->platform.brightness); put8(&w, c->platform.brightness_mode); put8(&w, c->platform.rotation); }
    if (!w.ok || w.offset + 4 > capacity) return false;
    put32(&w, checksum(wire, w.offset));
    if (written) *written = w.offset;
    return w.ok;
}
bool md_configuration_decode(const uint8_t *wire, size_t length, md_device_configuration_t *config)
{
    if (!wire || !config || length < 120 || length > MD_CONFIG_WIRE_MAX) return false;
    reader_t tail = {wire + length - 4, 4, 0, true};
    if (get32(&tail) != checksum(wire, length - 4)) return false;
    reader_t r = {wire, length - 4, 0, true};
    if (get32(&r) != 0x3143444dU) return false;
    const uint8_t version = get8(&r);
    if (version != 1 && version != MD_CONFIG_SCHEMA_VERSION) return false;
    md_device_configuration_t result = {0}; result.schema_version = MD_CONFIG_SCHEMA_VERSION;
    md_common_configuration_t *s = &result.common;
    s->dashboard = get8(&r); s->theme = get8(&r); s->day_night = get8(&r); s->units = get8(&r);
    s->temperature_unit = get8(&r); s->language = get8(&r); s->last_screen = get8(&r);
    s->page_count = get8(&r); s->active_page = get8(&r);
    const uint8_t warnings = get8(&r), theme = get8(&r);
    if (warnings > 1 || theme > 1 || !s->page_count || s->page_count > MD_CONFIG_MAX_PAGES) return false;
    s->warnings_enabled = warnings; s->custom_theme = theme;
    uint32_t colors[sizeof(s->palette) / sizeof(uint32_t)];
    for (unsigned i = 0; i < sizeof(s->palette) / 4; ++i) colors[i] = get32(&r);
    memcpy(&s->palette, colors, sizeof(colors));
    for (unsigned i = 0; i < MD_UI_MAX_WARNING_THRESHOLDS; ++i) {
        md_ui_warning_threshold_t *t = &s->warnings[i];
        t->signal_id = get8(&r); const uint8_t enabled = get8(&r); t->direction = get8(&r);
        if (enabled > 1) return false;
        t->enabled = enabled; t->warning_milli = (int32_t)get32(&r); t->critical_milli = (int32_t)get32(&r);
    }
    for (unsigned i = 0; i < MD_UI_FIXED_CARD_OVERRIDES_COUNT; ++i) s->fixed_card_overrides[i] = get8(&r);
    for (unsigned p = 0; p < s->page_count; ++p) {
        md_ui_dashboard_page_t *page = &s->pages[p];
        read_text_field(&r, page->title); page->layout = get8(&r); page->widget_count = get8(&r);
        if (page->widget_count > MD_UI_MAX_WIDGETS) return false;
        for (unsigned i = 0; i < page->widget_count; ++i) {
            md_ui_page_widget_t *v = &page->widgets[i];
            v->signal = get8(&r); v->type = get8(&r); v->precision = get8(&r);
            v->column = get8(&r); v->row = get8(&r); v->columns = get8(&r); v->rows = get8(&r);
            v->minimum = bitsfloat(get32(&r)); v->maximum = bitsfloat(get32(&r)); read_text_field(&r, v->label);
        }
    }
    result.platform.present = get32(&r);
    if (result.platform.present) { result.platform.brightness = get8(&r); result.platform.brightness_mode = get8(&r); result.platform.rotation = get8(&r); }
    if (result.platform.rotation > 3) return false;
    if (version == 1) result.platform.rotation = 0; /* Old rotation was build-only. */
    if (!r.ok || r.offset != r.size || !md_configuration_validate(&result, NULL)) return false;
    *config = result; return true;
}
void md_configuration_reset_theme(md_device_configuration_t *c)
{
    if (!c) return;
    c->common.custom_theme = false; c->common.theme = MD_UI_THEME_OEM;
    c->common.palette = md_ui_theme_palette(MD_UI_THEME_OEM, false);
}
bool md_configuration_reset_page(md_device_configuration_t *c, uint8_t page)
{
    if (!c || page >= c->common.page_count) return false;
    md_ui_settings_t s; md_ui_settings_defaults(&s);
    memset(&c->common.pages[page], 0, sizeof(c->common.pages[page]));
    snprintf(c->common.pages[page].title, MD_CONFIG_TEXT_SIZE, "CUSTOM %u", page + 1U);
    page_from_settings(&c->common.pages[page], &s); return true;
}
bool md_configuration_reset_ui(md_device_configuration_t *c, const md_target_profile_t *target)
{
    if (!c) return false;
    const md_platform_configuration_t platform = c->platform;
    md_ui_settings_t s; md_ui_settings_defaults(&s);
    if (!md_configuration_from_legacy(c, &s, target)) return false;
    c->platform = platform; return true;
}
