#include "ui_screens.h"
#include "ui_startup.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "lvgl.h"
#include "sdkconfig.h"
#include "ui_i18n.h"

#define UI_FIELDS_PER_VIEW 14
#define UI_DIAG_ROWS 7
#define UI_SETTINGS_PAGE_COUNT \
    ((MD_UI_SETTINGS_ROWS + MD_UI_SETTINGS_ROWS_PER_PAGE - 1U) / \
     MD_UI_SETTINGS_ROWS_PER_PAGE)

typedef enum {
    FIELD_SIGNAL = 0,
    FIELD_DIAGNOSTIC,
} field_kind_t;

typedef enum {
    DIAG_FIRMWARE = 0,
    DIAG_UPTIME,
    DIAG_HEAP_FREE,
    DIAG_HEAP_MIN,
    DIAG_REFRESH_RATE,
    DIAG_RENDER_TIME,
    DIAG_DISPLAY,
    DIAG_TOUCH,
    DIAG_BRIGHTNESS,
    DIAG_TRANSPORT,
    DIAG_ECUS,
    DIAG_SIGNALS,
    DIAG_STALE,
    DIAG_ERRORS,
} diagnostic_key_t;

typedef struct {
    lv_obj_t *card;
    lv_obj_t *title;
    lv_obj_t *value;
    lv_obj_t *unit;
    lv_obj_t *status;
    field_kind_t kind;
    md_ui_signal_id_t signal_id;
    md_ui_signal_id_t default_signal_id;
    md_ui_card_group_t card_group;
    uint8_t card_slot;
    uint8_t widget_slot;
    diagnostic_key_t diagnostic_key;
    bool primary;
    char canonical_signal[MD_UI_SIGNAL_NAME_LEN];
    char value_text[32];
    char unit_text[12];
    char status_text[32];
    uint32_t rendered_color;
    bool color_set;
    bool inline_unit;
    bool compact_title;
    char title_key[64];
    char title_text[64];
    const lv_font_t *title_font;
    const lv_font_t *value_font;
    uint8_t widget_type;
    lv_obj_t *visual;
    lv_chart_series_t *series;
} field_view_t;

typedef struct {
    lv_obj_t *root;
    lv_obj_t *title;
    lv_obj_t *mode;
    lv_obj_t *footer;
    field_view_t fields[UI_FIELDS_PER_VIEW];
    uint8_t field_count;
    char mode_text[32];
    char title_key[48];
    char title_text[48];
    char footer_key[80];
    char footer_text[80];
    lv_obj_t *previous_hint;
    lv_obj_t *next_hint;
} screen_view_t;

typedef struct {
    lv_obj_t *row;
    lv_obj_t *title;
    lv_obj_t *value;
    char title_key[48];
    char title_text[48];
    char value_text[32];
    bool selected;
    bool selection_valid;
} setting_row_view_t;

typedef struct {
    lv_obj_t *card;
    lv_obj_t *label;
    char label_text[32];
    bool selected;
} widget_picker_option_view_t;

static screen_view_t s_dashboards[MD_UI_DASHBOARD_COUNT];
static screen_view_t s_screens[MD_UI_SCREEN_COUNT];
static setting_row_view_t s_setting_rows[MD_UI_SETTINGS_ROWS];
static widget_picker_option_view_t s_widget_picker_options[MD_SIGNAL_COUNT];
static md_ui_palette_t s_palette;
static md_ui_theme_style_t s_theme_style;
static uint16_t s_width = 320;
static uint16_t s_height = 240;
static bool s_created;
static lv_obj_t *s_calibration_target;
static lv_obj_t *s_calibration_instruction;
static lv_obj_t *s_settings_action;
static lv_obj_t *s_wifi_switch;
static lv_obj_t *s_settings_action_label;
static lv_obj_t *s_settings_page;
static lv_obj_t *s_settings_page_label;
static lv_obj_t *s_settings_previous;
static lv_obj_t *s_settings_previous_label;
static char s_settings_action_text[24];
static char s_settings_page_text[24];
static char s_settings_previous_text[24];
static char s_widget_picker_title_text[48];
static md_ui_language_t s_language = MD_UI_LANGUAGE_ENGLISH;
static md_ui_language_t s_font_language = MD_UI_LANGUAGE_COUNT;
static md_ui_dashboard_page_t s_page_model;
static md_ui_history_t s_history[MD_SIGNAL_COUNT];
static lv_obj_t *s_focus_stats;
static char s_focus_stats_text[96];
static lv_obj_t *s_config_credentials;

extern const lv_font_t md_ui_font_mono_10;
extern const lv_font_t md_ui_font_mono_12;
extern const lv_font_t md_ui_font_mono_14;
extern const lv_font_t md_ui_font_mono_16;
extern const lv_font_t md_ui_font_mono_20;
extern const lv_font_t md_ui_font_mono_28;
extern const lv_font_t md_ui_font_mono_48;
extern const lv_font_t md_ui_font_cluster_16;
extern const lv_font_t md_ui_font_cluster_20;
extern const lv_font_t md_ui_font_cluster_28;
extern const lv_font_t md_ui_font_cluster_48;
extern const lv_font_t md_ui_font_ru_10;
extern const lv_font_t md_ui_font_ru_12;
extern const lv_font_t md_ui_font_ru_14;
extern const lv_font_t md_ui_font_ru_16;
#if CONFIG_MINIDASH_TOUCH_DEBUG_OVERLAY
static lv_obj_t *s_debug_label;
#endif

static lv_coord_t xpx(int value)
{
    return (lv_coord_t)((int32_t)value * s_width / 320);
}

static lv_coord_t ypx(int value)
{
    return (lv_coord_t)((int32_t)value * s_height / 240);
}

static const lv_font_t *ui_font_for_language(const lv_font_t *font)
{
    if(s_theme_style.font_family==1) {
        if(font==&lv_font_montserrat_10) return &md_ui_font_mono_10;
        if(font==&lv_font_montserrat_12) return &md_ui_font_mono_12;
        if(font==&lv_font_montserrat_14) return &md_ui_font_mono_14;
        if(font==&lv_font_montserrat_16) return &md_ui_font_mono_16;
        if(font==&lv_font_montserrat_20) return &md_ui_font_mono_20;
        if(font==&lv_font_montserrat_28) return &md_ui_font_mono_28;
        if(font==&lv_font_montserrat_48) return &md_ui_font_mono_48;
    }
    if (s_language != MD_UI_LANGUAGE_RUSSIAN) return font;
    if (font == &lv_font_montserrat_10) return &md_ui_font_ru_10;
    if (font == &lv_font_montserrat_12) return &md_ui_font_ru_12;
    if (font == &lv_font_montserrat_14) return &md_ui_font_ru_14;
    if (font == &lv_font_montserrat_16) return &md_ui_font_ru_16;
    return font;
}

static void set_ui_font(lv_obj_t *object, const lv_font_t *font,
                        uint32_t selector)
{
    if (object && font)
        lv_obj_set_style_text_font(object, ui_font_for_language(font), selector);
}

static const lv_font_t *metric_font(const lv_font_t *font)
{
    if(s_theme_style.font_family==2) {
        if(font==&lv_font_montserrat_16) return &md_ui_font_cluster_16;
        if(font==&lv_font_montserrat_20) return &md_ui_font_cluster_20;
        if(font==&lv_font_montserrat_28) return &md_ui_font_cluster_28;
        if(font==&lv_font_montserrat_48) return &md_ui_font_cluster_48;
    }
    return font;
}

static void apply_view_fonts(screen_view_t *view)
{
    if (!view || !view->root) return;
    set_ui_font(view->title, &lv_font_montserrat_16, 0);
    set_ui_font(view->mode, &lv_font_montserrat_12, 0);
    set_ui_font(view->footer, &lv_font_montserrat_10, 0);
    for (unsigned i = 0; i < view->field_count; ++i) {
        field_view_t *field = &view->fields[i];
        if (field->kind == FIELD_DIAGNOSTIC) {
            set_ui_font(field->title, &lv_font_montserrat_10, 0);
            set_ui_font(field->value, &lv_font_montserrat_12, 0);
        } else {
            set_ui_font(field->title, field->title_font, 0);
            set_ui_font(field->status, &lv_font_montserrat_10, 0);
            set_ui_font(field->value, (field->widget_type==MD_UI_WIDGET_STATUS ? field->value_font : metric_font(field->value_font)), 0);
            set_ui_font(field->unit, &lv_font_montserrat_12, 0);
        }
    }
}

static void apply_language_fonts(void)
{
    for (unsigned i = 0; i < MD_UI_DASHBOARD_COUNT; ++i)
        apply_view_fonts(&s_dashboards[i]);
    for (unsigned i = 0; i < MD_UI_SCREEN_COUNT; ++i)
        apply_view_fonts(&s_screens[i]);
    for (unsigned i = 0; i < MD_UI_SETTINGS_ROWS; ++i) {
        set_ui_font(s_setting_rows[i].title, s_height > s_width ? &lv_font_montserrat_12 : &lv_font_montserrat_14, 0);
        set_ui_font(s_setting_rows[i].value, s_height > s_width ? &lv_font_montserrat_12 : &lv_font_montserrat_14, 0);
    }
    set_ui_font(s_settings_action_label, &lv_font_montserrat_14, 0);
    set_ui_font(s_settings_previous_label, &lv_font_montserrat_14, 0);
    set_ui_font(s_settings_page_label, &lv_font_montserrat_14, 0);
    for (unsigned i = 0; i < MD_SIGNAL_COUNT; ++i)
        set_ui_font(s_widget_picker_options[i].label, &lv_font_montserrat_12, 0);
    set_ui_font(s_calibration_instruction, &lv_font_montserrat_14, 0);
    set_ui_font(s_config_credentials, &lv_font_montserrat_14, 0);
    set_ui_font(s_focus_stats, &lv_font_montserrat_10, 0);
    s_font_language = s_language;
}

static lv_color_t color(uint32_t rgb)
{
    return lv_color_hex(rgb);
}


static lv_obj_t *s_startup_masks[4];
static lv_obj_t *s_startup_glow;

static void themed_frame(lv_obj_t *object)
{
    if (!object) return;
    lv_obj_set_style_radius(object, xpx(s_theme_style.radius), 0);
    lv_obj_set_style_border_width(object, s_theme_style.border_width, 0);
    lv_obj_set_style_shadow_width(object, xpx(s_theme_style.glow_width), 0);
    lv_obj_set_style_shadow_color(object, color(s_palette.accent), 0);
    lv_obj_set_style_shadow_opa(object, s_theme_style.glow_opacity, 0);
}
/* Draw before children: labels stay legible above the instrument artwork. */
static void metric_frame_draw(lv_event_t *event)
{
    field_view_t *field=lv_event_get_user_data(event);
    lv_obj_t *obj=lv_event_get_target(event);
    lv_layer_t *layer=lv_event_get_layer(event);
    lv_area_t a; lv_obj_get_coords(obj,&a);
    const int w=lv_area_get_width(&a), h=lv_area_get_height(&a);
    lv_draw_line_dsc_t line; lv_draw_line_dsc_init(&line);
    line.color=color(s_palette.accent); line.width=1; line.opa=100;
    if(s_theme_style.frame_kind) {
        const int inset=s_theme_style.frame_kind==3 ? 5 : 3;
        line.p1.x=a.x1+inset; line.p1.y=a.y1+inset;
        line.p2.x=a.x1+inset; line.p2.y=a.y2-inset;
        if(s_theme_style.frame_kind==2) { line.width=3; line.opa=180; }
        lv_draw_line(layer,&line);
        if(s_theme_style.frame_kind!=2) {
            line.p1=line.p2; line.p2.x=a.x2-inset; lv_draw_line(layer,&line);
            if(s_theme_style.frame_kind==3) {
                line.p1=line.p2; line.p2.y=a.y1+inset; lv_draw_line(layer,&line);
            }
            if(s_theme_style.frame_kind==4) {
                line.p1.x=a.x2-14; line.p1.y=a.y1+3;
                line.p2.x=a.x2-3; line.p2.y=a.y1+14; lv_draw_line(layer,&line);
            }
        }
    }
    if(!s_theme_style.gauge_style || !field || field->visual || h<65 || w<90) return;
    const int radius=(w<h-22 ? w : h-22)/2-5;
    if(radius<16) return;
    lv_draw_arc_dsc_t arc; lv_draw_arc_dsc_init(&arc);
    arc.center.x=(a.x1+a.x2)/2; arc.center.y=a.y1+22+radius;
    arc.radius=radius; arc.start_angle=135; arc.end_angle=405;
    arc.width=s_theme_style.gauge_style==2 ? 3 : 2;
    arc.color=color(s_palette.border); arc.opa=100;
    lv_draw_arc(layer,&arc);
    for(int i=0;i<=10;i++) {
        const float angle=(135+i*27)*0.01745329252f;
        line.color=color(i==10 ? s_palette.critical : s_palette.text);
        line.width=i%5==0 ? 2 : 1; line.opa=120;
        line.p1.x=arc.center.x+(int)(cosf(angle)*(radius-5));
        line.p1.y=arc.center.y+(int)(sinf(angle)*(radius-5));
        line.p2.x=arc.center.x+(int)(cosf(angle)*radius);
        line.p2.y=arc.center.y+(int)(sinf(angle)*radius);
        lv_draw_line(layer,&line);
    }
}

static void settings_row_reset(setting_row_view_t *row)
{
    lv_obj_set_style_bg_color(row->row,color(s_palette.surface),0);
    lv_obj_set_style_border_color(row->row,color(s_palette.border),0);
    lv_obj_set_style_border_width(row->row,0,0);
    lv_obj_set_style_shadow_opa(row->row,LV_OPA_TRANSP,0);
    row->selected=false; row->selection_valid=false;
}
static void settings_row_select(setting_row_view_t *row, bool selected)
{
    if(row->selection_valid && row->selected==selected) return;
    row->selected=selected; row->selection_valid=true;
    lv_obj_set_style_bg_color(row->row,color(selected?s_palette.surface_alt:s_palette.surface),0);
    lv_obj_set_style_border_color(row->row,color(selected?s_palette.accent:s_palette.border),0);
    lv_obj_set_style_border_width(row->row,selected?1:0,0);
    lv_obj_set_style_shadow_opa(row->row,selected?s_theme_style.glow_opacity:LV_OPA_TRANSP,0);
}
static void phosphor_scanlines(lv_event_t *event)
{
    if (!s_theme_style.scanline_opacity) return;
    lv_obj_t *root = lv_event_get_target(event);
    lv_area_t area; lv_obj_get_coords(root, &area);
    lv_draw_line_dsc_t line; lv_draw_line_dsc_init(&line);
    line.color = lv_color_black(); line.opa = s_theme_style.scanline_opacity;
    line.width = 1;
    line.p1.x = area.x1; line.p2.x = area.x2;
    for (int y=area.y1+2; y<=area.y2; y+=4) {
        line.p1.y = line.p2.y = y;
        lv_draw_line(lv_event_get_layer(event), &line);
    }
}

static void set_static_text(lv_obj_t *label, char *buffer, size_t capacity,
                            const char *text)
{
    if (!label || !buffer || capacity == 0 || !text) return;
    /* Text content alone is insufficient after an object is recreated: its
       label may still contain LVGL's default "Text" instead of this buffer. */
    if (strncmp(buffer, text, capacity) == 0 && lv_label_get_text(label) == buffer) return;
    (void)snprintf(buffer, capacity, "%s", text);
    lv_obj_invalidate(label);
    lv_label_set_text_static(label, buffer);
}

static void set_ui_text(lv_obj_t *label, char *buffer, size_t capacity,
                        const char *english)
{
    set_static_text(label, buffer, capacity,
                    md_ui_i18n_text(s_language, english));
}

static void style_root(screen_view_t *view, const char *title,
                       const char *footer)
{
    view->root = lv_obj_create(NULL);
    if (!view->root) return;
    lv_obj_remove_style_all(view->root);
    lv_obj_set_size(view->root, s_width, s_height);
    lv_obj_clear_flag(view->root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(view->root, color(s_palette.background), 0);
    lv_obj_set_style_bg_opa(view->root, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(view->root, phosphor_scanlines, LV_EVENT_DRAW_POST, NULL);

    view->title = lv_label_create(view->root);
    view->mode = lv_label_create(view->root);
    view->footer = lv_label_create(view->root);
    if (!view->title || !view->mode || !view->footer) return;
    (void)snprintf(view->title_key, sizeof(view->title_key), "%s", title);
    (void)snprintf(view->footer_key, sizeof(view->footer_key), "%s", footer);
    set_ui_text(view->title, view->title_text, sizeof(view->title_text),
                view->title_key);
    set_ui_text(view->mode, view->mode_text, sizeof(view->mode_text), "OFFLINE");
    set_ui_text(view->footer, view->footer_text, sizeof(view->footer_text),
                view->footer_key);

    lv_obj_set_pos(view->title, xpx(12), ypx(7));
    lv_obj_set_size(view->title, xpx(205), ypx(20));
    set_ui_font(view->title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(view->title, color(s_palette.text), 0);
    lv_obj_set_style_text_letter_space(view->title,s_theme_style.letter_spacing,0);
    lv_obj_set_style_text_letter_space(view->title, 1, 0);

    lv_obj_set_pos(view->mode, xpx(224), ypx(8));
    lv_obj_set_size(view->mode, xpx(84), ypx(18));
    set_ui_font(view->mode, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(view->mode, color(s_palette.accent), 0);
    lv_obj_set_style_text_align(view->mode, LV_TEXT_ALIGN_RIGHT, 0);

    lv_obj_set_pos(view->footer, xpx(10), ypx(217));
    lv_obj_set_size(view->footer, xpx(300), ypx(16));
    set_ui_font(view->footer, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(view->footer, color(s_palette.muted), 0);
    lv_label_set_long_mode(view->footer, LV_LABEL_LONG_MODE_DOTS);
}

static void create_nav_hints(screen_view_t *view)
{
    if (!view || !view->root) return;
    view->previous_hint = lv_label_create(view->root);
    view->next_hint = lv_label_create(view->root);
    if (!view->previous_hint || !view->next_hint) return;
    lv_label_set_text_static(view->previous_hint, LV_SYMBOL_LEFT);
    lv_label_set_text_static(view->next_hint, LV_SYMBOL_RIGHT);
    lv_obj_set_pos(view->previous_hint, xpx(3), ypx(109));
    lv_obj_set_pos(view->next_hint, xpx(301), ypx(109));
    lv_obj_set_style_text_color(view->previous_hint, color(s_palette.accent), 0);
    lv_obj_set_style_text_color(view->next_hint, color(s_palette.accent), 0);
    lv_obj_set_style_text_opa(view->previous_hint, LV_OPA_70, 0);
    lv_obj_set_style_text_opa(view->next_hint, LV_OPA_70, 0);
    lv_obj_move_foreground(view->previous_hint);
    lv_obj_move_foreground(view->next_hint);
}

static const char *compact_signal_title(md_ui_signal_id_t signal_id,
                                       const char *fallback)
{
    switch (signal_id) {
    case MD_SIGNAL_ENGINE_RPM: return "RPM";
    case MD_SIGNAL_VEHICLE_SPEED: return "SPD";
    case MD_SIGNAL_ENGINE_COOLANT_TEMPERATURE: return "COOL";
    case MD_SIGNAL_ENGINE_OIL_TEMPERATURE: return "OIL";
    case MD_SIGNAL_ENGINE_LOAD: return "LOAD";
    case MD_SIGNAL_ENGINE_CHARGE_PRESSURE: return "CHG";
    case MD_SIGNAL_ENGINE_AMBIENT_PRESSURE: return "AMB";
    case MD_SIGNAL_ENGINE_BOOST: return "BST";
    case MD_SIGNAL_TRANSMISSION_OIL_TEMPERATURE: return "T OIL";
    case MD_SIGNAL_TRANSMISSION_GEAR: return "GEAR";
    case MD_SIGNAL_AWD_CLUTCH_TEMPERATURE: return "AWD";
    case MD_SIGNAL_BATTERY_VOLTAGE: return "BATT";
    default: return fallback ? fallback : "SIGNAL";
    }
}

static field_view_t *create_field(screen_view_t *view, int x, int y,
                                  int width, int height, const char *label,
                                  const char *canonical_signal, bool primary)
{
    if (!view || !view->root || view->field_count >= UI_FIELDS_PER_VIEW) return NULL;
    field_view_t *field = &view->fields[view->field_count++];
    memset(field, 0, sizeof(*field));
    field->kind = FIELD_SIGNAL;
    field->widget_slot = UINT8_MAX;
    field->card_group = MD_UI_CARD_GROUP_NONE;
    field->card_slot = UINT8_MAX;
    field->signal_id = (md_ui_signal_id_t)md_ui_signal_id_from_name(canonical_signal);
    if ((int)field->signal_id < 0 || field->signal_id >= MD_SIGNAL_COUNT) {
        field->signal_id = MD_SIGNAL_COUNT;
    }
    field->default_signal_id = field->signal_id;
    field->primary = primary;
    const bool narrow_card = xpx(width) < 110;
    const bool compact_card = ypx(height) < 70;
    field->inline_unit = width < 170 || compact_card || narrow_card;
    field->compact_title = width < 190;
    field->title_font = narrow_card || compact_card || width < 180
        ? &lv_font_montserrat_10 : &lv_font_montserrat_12;
    field->value_font = narrow_card || compact_card
        ? &lv_font_montserrat_16
        : (primary ? &lv_font_montserrat_48
            : ((width < 150 || height < 90) ? &lv_font_montserrat_20
                                            : &lv_font_montserrat_28));
    if (canonical_signal) {
        (void)snprintf(field->canonical_signal, sizeof(field->canonical_signal), "%s",
                       canonical_signal);
    }
    field->card = lv_obj_create(view->root);
    if (!field->card) return NULL;
    lv_obj_remove_style_all(field->card);
    lv_obj_set_pos(field->card, xpx(x), ypx(y));
    lv_obj_set_size(field->card, xpx(width), ypx(height));
    lv_obj_clear_flag(field->card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(field->card, color(s_palette.surface), 0);
    lv_obj_set_style_bg_opa(field->card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(field->card, color(s_palette.border), 0);
    lv_obj_set_style_border_width(field->card, 1, 0);
    themed_frame(field->card);
    lv_obj_add_event_cb(field->card,metric_frame_draw,LV_EVENT_DRAW_MAIN,field);
    /* Child positions below already include their own margins. */
    lv_obj_set_style_pad_all(field->card, 0, 0);

    field->title = lv_label_create(field->card);
    field->value = lv_label_create(field->card);
    field->unit = lv_label_create(field->card);
    field->status = lv_label_create(field->card);
    if (!field->title || !field->value || !field->unit || !field->status) return NULL;
    const char *display_label = width < 190
        ? compact_signal_title(field->signal_id, label)
        : (label ? label : "SIGNAL");
    (void)snprintf(field->title_key, sizeof(field->title_key), "%s", display_label);
    set_ui_text(field->title, field->title_text, sizeof(field->title_text),
                field->title_key);
    lv_label_set_text_static(field->value, "--");
    lv_label_set_text_static(field->unit, "");
    set_ui_text(field->status, field->status_text,
                sizeof(field->status_text), "NO DATA");

    if (narrow_card) {
        lv_obj_set_pos(field->title, xpx(6), ypx(4));
        lv_obj_set_size(field->title, xpx(width - 12), ypx(14));
        set_ui_font(field->title, field->title_font, 0);
        lv_obj_set_pos(field->status, xpx(6), ypx(19));
        lv_obj_set_size(field->status, xpx(width - 12), ypx(13));
        set_ui_font(field->status, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_align(field->status, LV_TEXT_ALIGN_LEFT, 0);
        const int value_top = height < 60 ? 30 : 36;
        lv_obj_set_pos(field->value, xpx(6), ypx(value_top));
        lv_obj_set_size(field->value, xpx(width - 12), ypx(height - value_top - 2));
        set_ui_font(field->value, (field->widget_type==MD_UI_WIDGET_STATUS ? field->value_font : metric_font(field->value_font)), 0);
        lv_obj_add_flag(field->unit, LV_OBJ_FLAG_HIDDEN);
    } else if (compact_card) {
        lv_obj_set_pos(field->title, xpx(6), ypx(3));
        lv_obj_set_size(field->title, xpx(width - 84), ypx(14));
        set_ui_font(field->title, field->title_font, 0);
        lv_obj_set_pos(field->status, xpx(width - 76), ypx(3));
        lv_obj_set_size(field->status, xpx(70), ypx(14));
        set_ui_font(field->status, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_align(field->status, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_pos(field->value, xpx(6), ypx(17));
        lv_obj_set_size(field->value, xpx(width - 12), ypx(height - 19));
        set_ui_font(field->value, (field->widget_type==MD_UI_WIDGET_STATUS ? field->value_font : metric_font(field->value_font)), 0);
        lv_obj_add_flag(field->unit, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_set_pos(field->title, xpx(7), ypx(5));
        lv_obj_set_size(field->title, xpx(width - 82), ypx(16));
        set_ui_font(field->title, field->title_font, 0);
        lv_obj_set_pos(field->status, xpx(width - 76), ypx(5));
        lv_obj_set_size(field->status, xpx(68), ypx(14));
        set_ui_font(field->status, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_align(field->status, LV_TEXT_ALIGN_RIGHT, 0);
        const int value_y = primary ? 27 : 24;
        const int unit_y = height - 18;
        lv_obj_set_pos(field->value, xpx(7), ypx(value_y));
        lv_obj_set_size(field->value, xpx(width - 14), ypx(unit_y - value_y - 2));
        set_ui_font(field->value, (field->widget_type==MD_UI_WIDGET_STATUS ? field->value_font : metric_font(field->value_font)), 0);
        lv_obj_set_pos(field->unit, xpx(7), ypx(unit_y));
        lv_obj_set_size(field->unit, xpx(width - 14), ypx(14));
        set_ui_font(field->unit, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(field->unit, color(s_palette.muted), 0);
        lv_obj_set_style_text_align(field->unit, LV_TEXT_ALIGN_RIGHT, 0);
    }
    lv_obj_set_style_text_color(field->title, color(s_palette.muted), 0);
    /* Keep card headings on one line; titles use short aliases on narrow cards. */
    lv_label_set_long_mode(field->title, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_style_text_color(field->status, color(s_palette.unavailable), 0);
    lv_obj_set_style_text_color(field->value, color(s_palette.unavailable), 0);
    return field;
}

static field_view_t *create_diagnostic_field(screen_view_t *view, int x, int y,
                                             int width, int height,
                                             const char *title,
                                             diagnostic_key_t key)
{
    if (!view || !view->root || view->field_count >= UI_FIELDS_PER_VIEW) return NULL;
    field_view_t *field = &view->fields[view->field_count++];
    memset(field, 0, sizeof(*field));
    field->kind = FIELD_DIAGNOSTIC;
    field->diagnostic_key = key;
    field->card = lv_obj_create(view->root);
    if (!field->card) return NULL;
    lv_obj_remove_style_all(field->card);
    lv_obj_set_pos(field->card, xpx(x), ypx(y));
    lv_obj_set_size(field->card, xpx(width), ypx(height));
    lv_obj_set_style_bg_color(field->card, color(s_palette.surface), 0);
    lv_obj_set_style_bg_opa(field->card, LV_OPA_COVER, 0);
    themed_frame(field->card);
    lv_obj_set_style_pad_hor(field->card, xpx(6), 0);
    lv_obj_set_style_pad_ver(field->card, ypx(2), 0);

    field->title = lv_label_create(field->card);
    field->value = lv_label_create(field->card);
    if (!field->title || !field->value) return NULL;
    (void)snprintf(field->title_key, sizeof(field->title_key), "%s", title);
    set_ui_text(field->title, field->title_text, sizeof(field->title_text),
                field->title_key);
    set_ui_text(field->value, field->value_text, sizeof(field->value_text), "N/A");
    lv_obj_set_pos(field->title, xpx(5), ypx(4));
    lv_obj_set_size(field->title, xpx(70), ypx(16));
    set_ui_font(field->title, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(field->title, color(s_palette.muted), 0);
    lv_obj_set_pos(field->value, xpx(76), ypx(3));
    lv_obj_set_size(field->value, xpx(width - 82), ypx(height - 6));
    set_ui_font(field->value, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(field->value, color(s_palette.text), 0);
    lv_obj_set_style_text_align(field->value, LV_TEXT_ALIGN_RIGHT, 0);
    return field;
}

static field_view_t *create_fixed_field(screen_view_t *view,
                                        md_ui_card_group_t group, uint8_t slot,
                                        int x, int y, int width, int height,
                                        const char *label,
                                        const char *canonical_signal,
                                        bool primary)
{
    if (s_height > s_width) {
        md_ui_rect_t rect={x,y,width,height}; md_ui_fixed_portrait_rect(group,slot,&rect);
        x=rect.x; y=rect.y; width=rect.width; height=rect.height;
    }
    field_view_t *field = create_field(view, x, y, width, height, label,
                                       canonical_signal, primary);
    if (field) {
        field->card_group = group;
        field->card_slot = slot;
    }
    return field;
}

static void create_dashboard(screen_view_t *view, md_ui_dashboard_t dashboard,
                             const md_ui_settings_t *settings)
{
    static const char *const footers[] = {
        "EDGES: PREV / NEXT  |  HOLD SETTINGS",
        "EDGES: PREV / NEXT  |  HOLD SETTINGS",
        "EDGES: PREV / NEXT  |  HOLD SETTINGS",
        "EDGES: PREV / NEXT  |  HOLD SETTINGS",
    };
    static const char *const titles[] = {"DRIVE", "SPORT", "MINIMAL", "CUSTOM"};
    style_root(view, titles[dashboard], footers[dashboard]);
    if (dashboard == MD_UI_DASHBOARD_OEM) {
        (void)create_fixed_field(view, MD_UI_CARD_GROUP_DASHBOARD_OEM, 0,
            10, 39, 174, 112, "ENGINE SPEED", "engine.rpm", true);
        (void)create_fixed_field(view, MD_UI_CARD_GROUP_DASHBOARD_OEM, 1,
            190, 39, 120, 78, "SPEED", "vehicle.speed", false);
        (void)create_fixed_field(view, MD_UI_CARD_GROUP_DASHBOARD_OEM, 2,
            190, 121, 120, 86, "COOLANT", "engine.coolant_temperature", false);
        (void)create_fixed_field(view, MD_UI_CARD_GROUP_DASHBOARD_OEM, 3,
            10, 157, 174, 57, "BOOST", "engine.boost", false);
    } else if (dashboard == MD_UI_DASHBOARD_SPORT) {
        (void)create_fixed_field(view, MD_UI_CARD_GROUP_DASHBOARD_SPORT, 0,
            10, 39, 174, 112, "ENGINE SPEED", "engine.rpm", true);
        (void)create_fixed_field(view, MD_UI_CARD_GROUP_DASHBOARD_SPORT, 1,
            190, 39, 120, 78, "SPEED", "vehicle.speed", false);
        (void)create_fixed_field(view, MD_UI_CARD_GROUP_DASHBOARD_SPORT, 2,
            190, 121, 120, 86, "ENGINE LOAD", "engine.load", false);
        (void)create_fixed_field(view, MD_UI_CARD_GROUP_DASHBOARD_SPORT, 3,
            10, 157, 174, 57, "BATTERY", "vehicle.battery_voltage", false);
    } else if (dashboard == MD_UI_DASHBOARD_MINIMAL) {
        (void)create_fixed_field(view, MD_UI_CARD_GROUP_DASHBOARD_MINIMAL, 0,
            10, 39, 174, 112, "SPEED", "vehicle.speed", true);
        (void)create_fixed_field(view, MD_UI_CARD_GROUP_DASHBOARD_MINIMAL, 1,
            190, 39, 120, 78, "ENGINE SPEED", "engine.rpm", false);
        (void)create_fixed_field(view, MD_UI_CARD_GROUP_DASHBOARD_MINIMAL, 2,
            190, 121, 120, 86, "BATTERY", "vehicle.battery_voltage", false);
        (void)create_fixed_field(view, MD_UI_CARD_GROUP_DASHBOARD_MINIMAL, 3,
            10, 157, 174, 57, "COOLANT", "engine.coolant_temperature", false);
    } else {
        for (unsigned i = 0; i < settings->widget_count; ++i) {
            const md_ui_widget_binding_t *widget = &settings->widgets[i];
            const md_ui_rect_t rect=md_ui_widget_rect(widget,s_height>s_width);
            const int x=rect.x-2, y=rect.y, w=rect.width, h=rect.height;
            const int id = md_ui_signal_id_from_name(widget->canonical_signal);
            const char *label = id >= 0
                ? md_ui_signal_definitions[id].label : widget->canonical_signal;
            field_view_t *field = create_field(view, x + 2, y, w, h, label,
                widget->canonical_signal, widget->columns >= 6 && widget->rows >= 5);
            if (field) {
                field->widget_type = widget->type;
                if (widget->type==MD_UI_WIDGET_STATUS) { field->value_font=&lv_font_montserrat_16; set_ui_font(field->value,(field->widget_type==MD_UI_WIDGET_STATUS ? field->value_font : metric_font(field->value_font)),0); }
                if (widget->type == MD_UI_WIDGET_BAR) {
                    field->visual = lv_bar_create(field->card);
                    lv_bar_set_range(field->visual, 0, 1000);
                    lv_obj_set_pos(field->visual, xpx(7), ypx(h - 9));
                    lv_obj_set_size(field->visual, xpx(w - 14), ypx(5));
                    lv_obj_set_style_bg_color(field->visual, color(s_palette.accent), LV_PART_INDICATOR);
                } else if (widget->type == MD_UI_WIDGET_GRAPH) {
                    field->visual = lv_chart_create(field->card);
                    lv_obj_set_pos(field->visual, xpx(7), ypx(h - 19));
                    lv_obj_set_size(field->visual, xpx(w - 14), ypx(15));
                    lv_obj_set_style_pad_all(field->visual, 0, 0);
                    lv_obj_set_style_bg_opa(field->visual, LV_OPA_TRANSP, 0);
                    lv_obj_set_style_border_width(field->visual, 0, 0);
                    lv_chart_set_point_count(field->visual, MD_UI_HISTORY_SAMPLES);
                    lv_chart_set_div_line_count(field->visual, 0, 0);
                    field->series = lv_chart_add_series(field->visual, color(s_palette.accent), LV_CHART_AXIS_PRIMARY_Y);
                }
                field->widget_slot = (uint8_t)i;
                field->card_group = MD_UI_CARD_GROUP_CUSTOM;
                field->card_slot = (uint8_t)i;
            }
        }
        if (settings->widget_count == 0) {
            (void)create_diagnostic_field(view, 18, 62, 284, 48,
                "NO WIDGETS CONFIGURED", DIAG_SIGNALS);
        }
    }
}

static void create_performance_screen(screen_view_t *view)
{
    style_root(view, "PERFORMANCE", "READ ONLY  |  STALE VALUES HIDDEN");
    (void)create_fixed_field(view, MD_UI_CARD_GROUP_PERFORMANCE, 0,
        10, 39, 156, 105, "ENGINE SPEED", "engine.rpm", true);
    (void)create_fixed_field(view, MD_UI_CARD_GROUP_PERFORMANCE, 1,
        172, 39, 138, 78, "VEHICLE SPEED", "vehicle.speed", false);
    (void)create_fixed_field(view, MD_UI_CARD_GROUP_PERFORMANCE, 2,
        172, 123, 138, 84, "ENGINE LOAD", "engine.load", false);
    (void)create_fixed_field(view, MD_UI_CARD_GROUP_PERFORMANCE, 3,
        10, 150, 156, 57, "BOOST", "engine.boost", false);
}

static void create_temperature_screen(screen_view_t *view)
{
    style_root(view, "TEMPERATURES", "MISSING / STALE VALUES HIDDEN");
    (void)create_fixed_field(view, MD_UI_CARD_GROUP_TEMPERATURES, 0,
        10, 39, 148, 78, "ENGINE OIL", "engine.oil_temperature", false);
    (void)create_fixed_field(view, MD_UI_CARD_GROUP_TEMPERATURES, 1,
        162, 39, 148, 78, "COOLANT", "engine.coolant_temperature", false);
    (void)create_fixed_field(view, MD_UI_CARD_GROUP_TEMPERATURES, 2,
        10, 123, 148, 78, "TRANSMISSION OIL", "transmission.oil_temperature", false);
    (void)create_fixed_field(view, MD_UI_CARD_GROUP_TEMPERATURES, 3,
        162, 123, 148, 78, "AWD CLUTCH", "awd.clutch_temperature", false);
}

static void create_drivetrain_screen(screen_view_t *view)
{
    style_root(view, "DRIVETRAIN", "READINGS REQUIRE A DATA PROVIDER");
    (void)create_fixed_field(view, MD_UI_CARD_GROUP_DRIVETRAIN, 0,
        10, 39, 138, 128, "GEAR", "transmission.gear", true);
    (void)create_fixed_field(view, MD_UI_CARD_GROUP_DRIVETRAIN, 1,
        154, 39, 156, 78, "TRANSMISSION OIL", "transmission.oil_temperature", false);
    (void)create_fixed_field(view, MD_UI_CARD_GROUP_DRIVETRAIN, 2,
        154, 123, 156, 78, "AWD CLUTCH", "awd.clutch_temperature", false);
}

static void create_diagnostics_screen(screen_view_t *view)
{
    static const struct {
        const char *title;
        diagnostic_key_t key;
    } rows[] = {
        {"FIRMWARE", DIAG_FIRMWARE}, {"UPTIME", DIAG_UPTIME},
        {"FREE HEAP", DIAG_HEAP_FREE}, {"MIN HEAP", DIAG_HEAP_MIN},
        {"RENDERS / S", DIAG_REFRESH_RATE}, {"AVG RENDER", DIAG_RENDER_TIME},
        {"DISPLAY", DIAG_DISPLAY}, {"TOUCH", DIAG_TOUCH},
        {"BRIGHTNESS", DIAG_BRIGHTNESS}, {"TRANSPORT", DIAG_TRANSPORT},
        {"ECUS", DIAG_ECUS}, {"SAMPLES", DIAG_SIGNALS},
        {"STALE", DIAG_STALE}, {"ERRORS", DIAG_ERRORS},
    };
    style_root(view, "SERVICE STATUS", "N/A = SOURCE NOT PROVIDED");
    for (unsigned i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i) {
        const int column = (int)(i / UI_DIAG_ROWS);
        const int row = (int)(i % UI_DIAG_ROWS);
        (void)create_diagnostic_field(view, 10 + column * 153, 36 + row * 25,
            147, 23, rows[i].title, rows[i].key);
    }
}

static void create_settings_screen(void)
{
    static const char *const titles[MD_UI_SETTINGS_ROWS] = {
        "Dashboard", "Theme", "Day / night", "Speed units",
        "Temperature", "Brightness", "Master warnings", "Touch calibration",
        "Coolant warning", "Engine oil warn", "Trans oil warn", "Low voltage warn",
        "Dashboard layout", "Language", "Widget 1 signal", "Widget 2 signal",
        "Widget 3 signal", "Widget 4 signal", "Widget 5 signal", "Widget 6 signal",
        "Widget 7 signal", "Widget 8 signal", "Wi-Fi configuration", "Screen rotation"};
    screen_view_t *view = &s_screens[MD_UI_SCREEN_SETTINGS];
    style_root(view, "SETTINGS", "TAP ROW TO SELECT");
    for (unsigned i = 0; i < MD_UI_SETTINGS_ROWS; ++i) {
        setting_row_view_t *row = &s_setting_rows[i];
        row->row = lv_obj_create(view->root);
        if (!row->row) continue;
        lv_obj_remove_style_all(row->row);
        lv_obj_set_pos(row->row, xpx(10), ypx(35 + (int)(i % 4) * 40));
        lv_obj_set_size(row->row, xpx(300), ypx(37));
        if (i >= MD_UI_SETTINGS_ROWS_PER_PAGE) lv_obj_add_flag(row->row, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_opa(row->row, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(row->row, xpx(3), 0);
        row->title = lv_label_create(row->row);
        row->value = lv_label_create(row->row);
        if (!row->title || !row->value) continue;
        (void)snprintf(row->title_key, sizeof(row->title_key), "%s", titles[i]);
        row->value_text[0] = '\0';
        set_ui_text(row->title, row->title_text, sizeof(row->title_text),
                    row->title_key);
        lv_label_set_text_static(row->value, row->value_text);
        lv_obj_set_pos(row->title, xpx(8), ypx(10));
        lv_obj_set_size(row->title, xpx(145), ypx(20));
        set_ui_font(row->title, &lv_font_montserrat_14, 0);
        lv_obj_set_pos(row->value, xpx(155), ypx(10));
        lv_obj_set_size(row->value, xpx(135), ypx(20));
        set_ui_font(row->value, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_align(row->value, LV_TEXT_ALIGN_RIGHT, 0);
        if (s_height > s_width) {
            lv_obj_set_pos(row->title,xpx(8),ypx(2)); lv_obj_set_size(row->title,xpx(284),ypx(14));
            lv_obj_set_pos(row->value,xpx(8),ypx(18)); lv_obj_set_size(row->value,xpx(284),ypx(15));
            set_ui_font(row->title,&lv_font_montserrat_12,0); set_ui_font(row->value,&lv_font_montserrat_12,0);
        }
    }
    s_wifi_switch=lv_switch_create(s_setting_rows[22].row);
    lv_obj_set_pos(s_wifi_switch,xpx(250),ypx(7)); lv_obj_set_size(s_wifi_switch,xpx(42),ypx(24));
    lv_obj_clear_flag(s_wifi_switch,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_width(s_setting_rows[22].value,xpx(s_height>s_width?230:80));
    s_settings_action = lv_obj_create(view->root);
    if (!s_settings_action) return;
    lv_obj_remove_style_all(s_settings_action);
    lv_obj_set_pos(s_settings_action, xpx(208), ypx(198));
    lv_obj_set_size(s_settings_action, xpx(102), ypx(40));
    lv_obj_set_style_bg_color(s_settings_action, color(s_palette.surface_alt), 0);
    lv_obj_set_style_bg_opa(s_settings_action, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_settings_action, color(s_palette.accent), 0);
    lv_obj_set_style_border_width(s_settings_action, 1, 0);
    lv_obj_set_style_radius(s_settings_action, xpx(5), 0);
    s_settings_action_label = lv_label_create(s_settings_action);
    if (!s_settings_action_label) return;
    set_ui_text(s_settings_action_label, s_settings_action_text,
                sizeof(s_settings_action_text), "CHANGE");
    set_ui_font(s_settings_action_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_settings_action_label, color(s_palette.text), 0);
    lv_obj_center(s_settings_action_label);
    s_settings_previous = lv_obj_create(view->root);
    if (!s_settings_previous) return;
    lv_obj_remove_style_all(s_settings_previous);
    lv_obj_set_pos(s_settings_previous, xpx(10), ypx(198));
    lv_obj_set_size(s_settings_previous, xpx(92), ypx(40));
    lv_obj_set_style_bg_color(s_settings_previous, color(s_palette.surface_alt), 0);
    lv_obj_set_style_bg_opa(s_settings_previous, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_settings_previous, color(s_palette.accent), 0);
    lv_obj_set_style_border_width(s_settings_previous, 1, 0);
    lv_obj_set_style_radius(s_settings_previous, xpx(5), 0);
    s_settings_previous_label = lv_label_create(s_settings_previous);
    if (!s_settings_previous_label) return;
    set_ui_text(s_settings_previous_label, s_settings_previous_text,
                sizeof(s_settings_previous_text), "PREV");
    set_ui_font(s_settings_previous_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_settings_previous_label, color(s_palette.text), 0);
    lv_obj_center(s_settings_previous_label);

    s_settings_page = lv_obj_create(view->root);
    if (!s_settings_page) return;
    lv_obj_remove_style_all(s_settings_page);
    lv_obj_set_pos(s_settings_page, xpx(109), ypx(198));
    lv_obj_set_size(s_settings_page, xpx(92), ypx(40));
    lv_obj_set_style_bg_color(s_settings_page, color(s_palette.surface_alt), 0);
    lv_obj_set_style_bg_opa(s_settings_page, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_settings_page, color(s_palette.accent), 0);
    lv_obj_set_style_border_width(s_settings_page, 1, 0);
    lv_obj_set_style_radius(s_settings_page, xpx(5), 0);
    s_settings_page_label = lv_label_create(s_settings_page);
    if (!s_settings_page_label) return;
    set_ui_text(s_settings_page_label, s_settings_page_text,
                sizeof(s_settings_page_text), "MORE");
    set_ui_font(s_settings_page_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_settings_page_label, color(s_palette.text), 0);
    lv_obj_center(s_settings_page_label);
}

static void create_focus_screen(void)
{
    screen_view_t *view = &s_screens[MD_UI_SCREEN_FOCUS];
    style_root(view, "SIGNAL", "TAP OR HOLD TO RETURN");
    field_view_t *field=create_field(view, 54, 48, 212, 156, "ENGINE SPEED",
                       "engine.rpm", true);
    if (field) {
        field->widget_type=MD_UI_WIDGET_GRAPH;
        field->visual=lv_chart_create(field->card);
        lv_obj_set_pos(field->visual,xpx(7),ypx(113));
        lv_obj_set_size(field->visual,xpx(198),ypx(23));
        lv_obj_set_style_pad_all(field->visual,0,0);
        lv_obj_set_style_bg_opa(field->visual,LV_OPA_TRANSP,0);
        lv_obj_set_style_border_width(field->visual,0,0);
        lv_chart_set_point_count(field->visual,MD_UI_HISTORY_SAMPLES);
        lv_chart_set_div_line_count(field->visual,0,0);
        field->series=lv_chart_add_series(field->visual,color(s_palette.accent),LV_CHART_AXIS_PRIMARY_Y);
    }
    s_focus_stats = lv_label_create(view->root);
    lv_obj_set_pos(s_focus_stats, xpx(12), ypx(205));
    lv_obj_set_size(s_focus_stats, xpx(296), ypx(16));
    set_ui_font(s_focus_stats, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(s_focus_stats, color(s_palette.muted), 0);
    lv_obj_set_y(view->footer, ypx(225));
}

static void create_widget_picker_screen(void)
{
    screen_view_t *view = &s_screens[MD_UI_SCREEN_WIDGET_PICKER];
    style_root(view, "REPLACE WIDGET", "TAP SIGNAL TO REPLACE  |  HOLD TO CANCEL");
    for (unsigned i = 0; i < MD_SIGNAL_COUNT; ++i) {
        widget_picker_option_view_t *option = &s_widget_picker_options[i];
        const unsigned column = i / 6U;
        const unsigned row = i % 6U;
        option->card = lv_obj_create(view->root);
        if (!option->card) continue;
        lv_obj_remove_style_all(option->card);
        lv_obj_set_pos(option->card, xpx(10 + (int)column * 153),
                       ypx(39 + (int)row * 27));
        lv_obj_set_size(option->card, xpx(147), ypx(24));
        lv_obj_clear_flag(option->card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(option->card, color(s_palette.surface), 0);
        lv_obj_set_style_bg_opa(option->card, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(option->card, color(s_palette.border), 0);
        lv_obj_set_style_border_width(option->card, 1, 0);
        lv_obj_set_style_radius(option->card, xpx(3), 0);
        option->label = lv_label_create(option->card);
        if (!option->label) continue;
        set_ui_text(option->label, option->label_text,
                    sizeof(option->label_text),
                    md_ui_i18n_compact_signal_label(s_language,
                        (md_ui_signal_id_t)i, "SIGNAL"));
        lv_obj_set_pos(option->label, xpx(6), ypx(3));
        lv_obj_set_size(option->label, xpx(135), ypx(17));
        set_ui_font(option->label, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(option->label, color(s_palette.text), 0);
        lv_label_set_long_mode(option->label, LV_LABEL_LONG_MODE_CLIP);
    }
}

static void create_calibration_screen(void)
{
    screen_view_t *view = &s_screens[MD_UI_SCREEN_CALIBRATION];
    style_root(view, "TOUCH CALIBRATION", "HOLD ANYWHERE TO CANCEL");
    s_calibration_instruction = lv_label_create(view->root);
    s_calibration_target = lv_label_create(view->root);
    lv_label_set_text_static(s_calibration_target, "+");
    set_ui_font(s_calibration_instruction, &lv_font_montserrat_14, 0);
    set_ui_font(s_calibration_target, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(s_calibration_target, color(s_palette.accent), 0);
    lv_obj_set_style_text_color(s_calibration_instruction, color(s_palette.text), 0);
    lv_obj_set_pos(s_calibration_instruction, xpx(75), ypx(110));
    lv_obj_set_size(s_calibration_instruction, xpx(170), ypx(25));
    md_ui_screens_calibration_step(0);
}

static screen_view_t *view_for(md_ui_screen_t screen,
                               const md_ui_settings_t *settings)
{
    if (screen == MD_UI_SCREEN_DASHBOARD) {
        const md_ui_dashboard_t dashboard = settings &&
            settings->dashboard < MD_UI_DASHBOARD_COUNT
            ? settings->dashboard : MD_UI_DASHBOARD_OEM;
        return &s_dashboards[dashboard];
    }
    if (screen == MD_UI_SCREEN_CUSTOM) return &s_dashboards[MD_UI_DASHBOARD_CUSTOM];
    if (screen >= 0 && screen < MD_UI_SCREEN_COUNT) return &s_screens[screen];
    return NULL;
}

bool md_ui_screens_create(uint16_t width, uint16_t height,
                          const md_ui_settings_t *settings,
                          const md_ui_palette_t *palette)
{
    if (!settings || !palette || width == 0 || height == 0) return false;
    memset(s_dashboards, 0, sizeof(s_dashboards));
    memset(s_screens, 0, sizeof(s_screens));
    memset(s_setting_rows, 0, sizeof(s_setting_rows));
    memset(s_widget_picker_options, 0, sizeof(s_widget_picker_options));
    s_width = width;
    s_height = height;
    s_palette = *palette;
    s_theme_style = md_ui_theme_style(settings->theme);
    s_language = settings->language;

    for (int i = 0; i < MD_UI_DASHBOARD_COUNT; ++i) {
        create_dashboard(&s_dashboards[i], (md_ui_dashboard_t)i, settings);
        if (!s_dashboards[i].root) return false;
    }
    create_performance_screen(&s_screens[MD_UI_SCREEN_PERFORMANCE]);
    create_temperature_screen(&s_screens[MD_UI_SCREEN_TEMPERATURES]);
    create_drivetrain_screen(&s_screens[MD_UI_SCREEN_DRIVETRAIN]);
    create_diagnostics_screen(&s_screens[MD_UI_SCREEN_DIAGNOSTICS]);
    create_settings_screen();
    create_focus_screen();
    create_widget_picker_screen();
    create_calibration_screen();
    screen_view_t *config_view = &s_screens[MD_UI_SCREEN_CONFIG_MODE];
    style_root(config_view, "Wi-Fi configuration", "BACK TO RETURN | AP TIMEOUT 10 MIN");
    s_config_credentials = lv_label_create(config_view->root);
    lv_obj_set_pos(s_config_credentials, xpx(16), ypx(52));
    lv_obj_set_size(s_config_credentials, xpx(288), ypx(146));
    set_ui_font(s_config_credentials, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_config_credentials, color(s_palette.text), 0);
#if CONFIG_MINIDASH_TOUCH_DEBUG_OVERLAY
    lv_obj_t *top = lv_layer_top();
    for (int side = 0; side < 2; ++side) {
        lv_obj_t *zone = lv_obj_create(top);
        lv_obj_remove_style_all(zone);
        lv_obj_set_pos(zone, side ? s_width - xpx(56) : 0, 0);
        lv_obj_set_size(zone, xpx(56), s_height);
        lv_obj_set_style_bg_color(zone, color(s_palette.accent), 0);
        lv_obj_set_style_bg_opa(zone, LV_OPA_20, 0);
        lv_obj_clear_flag(zone, LV_OBJ_FLAG_CLICKABLE);
    }
    s_debug_label = lv_label_create(top);
    lv_label_set_text_static(s_debug_label, "TOUCH --,--");
    lv_obj_set_pos(s_debug_label, xpx(105), ypx(216));
    lv_obj_set_style_text_color(s_debug_label, color(s_palette.accent), 0);
#endif
    for (int i = 0; i < MD_UI_DASHBOARD_COUNT; ++i)
        create_nav_hints(&s_dashboards[i]);
    for (int i = MD_UI_SCREEN_PERFORMANCE; i <= MD_UI_SCREEN_DIAGNOSTICS; ++i)
        create_nav_hints(&s_screens[i]);
    for (int i = 1; i < MD_UI_SCREEN_COUNT; ++i) {
        if (i == MD_UI_SCREEN_CUSTOM) continue;
        if (!s_screens[i].root) return false;
    }
    s_created = true;
    md_ui_screens_retheme(palette, settings->theme);
    return true;
}

bool md_ui_screens_resize(uint16_t width, uint16_t height,
                          const md_ui_settings_t *settings, const md_ui_palette_t *palette,
                          md_ui_screen_t screen)
{
    lv_obj_t *temporary = lv_obj_create(NULL);
    if (!temporary) return false;
    lv_screen_load(temporary);
    for (unsigned i=0; i<MD_UI_DASHBOARD_COUNT; ++i)
        if (s_dashboards[i].root) lv_obj_delete(s_dashboards[i].root);
    for (unsigned i=0; i<MD_UI_SCREEN_COUNT; ++i)
        if (s_screens[i].root) lv_obj_delete(s_screens[i].root);
#if CONFIG_MINIDASH_TOUCH_DEBUG_OVERLAY
    lv_obj_clean(lv_layer_top()); s_debug_label=NULL;
#endif
    s_created=false;
    s_calibration_target=s_calibration_instruction=s_focus_stats=s_config_credentials=NULL;
    s_settings_action=s_settings_action_label=s_settings_page=s_settings_page_label=NULL;
    s_settings_previous=s_settings_previous_label=NULL;
    if (!md_ui_screens_create(width,height,settings,palette)) return false;
    md_ui_screens_show(screen,settings);
    lv_obj_delete(temporary);
    return true;
}

void md_ui_screens_rebuild_custom(const md_ui_settings_t *settings)
{
    if (!s_created || !settings) return;
    screen_view_t *view=&s_dashboards[MD_UI_DASHBOARD_CUSTOM];
    screen_view_t *backup=malloc(sizeof(*backup));
    if (!backup) return;
    *backup=*view;
    lv_obj_t *old=view->root;
    const bool active=old==lv_screen_active();
    /* Static LVGL labels borrow these buffers. Construct directly in the
       permanent view: copying from a temporary view leaves dangling text. */
    memset(view,0,sizeof(*view));
    create_dashboard(view,MD_UI_DASHBOARD_CUSTOM,settings);
    if (!view->root) { *view=*backup; free(backup); return; }
    create_nav_hints(view);
    if (active) lv_screen_load(view->root);
    if (old) lv_obj_delete(old);
    free(backup);
}

void md_ui_screens_focus_signal(md_ui_signal_id_t signal)
{
    if (!s_created || signal < 0 || signal >= MD_SIGNAL_COUNT) return;
    screen_view_t *view = &s_screens[MD_UI_SCREEN_FOCUS];
    if (!view->field_count) return;
    field_view_t *field = &view->fields[0];
    const md_ui_signal_definition_t *definition = md_ui_signal_definition(signal);
    field->signal_id = signal;
    field->default_signal_id = signal;
    (void)snprintf(field->canonical_signal, sizeof(field->canonical_signal),
                   "%s", definition->canonical_name);
    const char *label = md_ui_i18n_signal_label(s_language, signal);
    set_static_text(field->title, field->title_text, sizeof(field->title_text), label);
}

void md_ui_screens_set_calibrated(bool calibrated)
{
    if (!s_created) return;
    screen_view_t *settings = &s_screens[MD_UI_SCREEN_SETTINGS];
    (void)snprintf(settings->footer_key, sizeof(settings->footer_key), "%s",
                   "TAP NEXT | DOUBLE CHANGE | HOLD BACK");
    set_ui_text(settings->footer, settings->footer_text,
                sizeof(settings->footer_text), settings->footer_key);
    if (calibrated)
        lv_obj_add_flag(s_screens[MD_UI_SCREEN_SETTINGS].footer, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_remove_flag(s_screens[MD_UI_SCREEN_SETTINGS].footer, LV_OBJ_FLAG_HIDDEN);
    if (s_settings_action) {
        if (calibrated) lv_obj_remove_flag(s_settings_action, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_settings_action, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_settings_page) {
        if (calibrated) lv_obj_remove_flag(s_settings_page, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_settings_page, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_settings_previous) {
        if (calibrated) lv_obj_remove_flag(s_settings_previous, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_settings_previous, LV_OBJ_FLAG_HIDDEN);
    }
    for (int i = 0; i < MD_UI_DASHBOARD_COUNT; ++i) {
        screen_view_t *view = &s_dashboards[i];
        const char *footer = calibrated
            ? "EDGES: PREV / NEXT  |  HOLD SETTINGS"
            : "TAP NEXT | DOUBLE PREV | HOLD SETTINGS";
        (void)snprintf(view->footer_key, sizeof(view->footer_key), "%s", footer);
        set_ui_text(view->footer, view->footer_text,
                    sizeof(view->footer_text), view->footer_key);
        if (view->previous_hint) {
            if (calibrated) lv_obj_remove_flag(view->previous_hint, LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(view->previous_hint, LV_OBJ_FLAG_HIDDEN);
        }
        if (view->next_hint) {
            if (calibrated) lv_obj_remove_flag(view->next_hint, LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(view->next_hint, LV_OBJ_FLAG_HIDDEN);
        }
    }
    for (int i = MD_UI_SCREEN_PERFORMANCE; i <= MD_UI_SCREEN_DIAGNOSTICS; ++i) {
        screen_view_t *view = &s_screens[i];
        if (view->previous_hint) {
            if (calibrated) lv_obj_remove_flag(view->previous_hint, LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(view->previous_hint, LV_OBJ_FLAG_HIDDEN);
        }
        if (view->next_hint) {
            if (calibrated) lv_obj_remove_flag(view->next_hint, LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(view->next_hint, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

int md_ui_screens_setting_at(uint8_t selected_setting, int16_t x, int16_t y)
{
    if (x < xpx(10) || x >= xpx(310)) return -1;
    for (int slot = 0; slot < 4; ++slot) {
        const int top = ypx(35 + slot * 40);
        if (y >= top && y < top + ypx(37)) {
            const int row = (int)(selected_setting / MD_UI_SETTINGS_ROWS_PER_PAGE) *
                            MD_UI_SETTINGS_ROWS_PER_PAGE + slot;
            return row < MD_UI_SETTINGS_ROWS ? row : -1;
        }
    }
    return -1;
}

bool md_ui_screens_setting_action_at(int16_t x, int16_t y)
{
    return s_settings_action && !lv_obj_has_flag(s_settings_action, LV_OBJ_FLAG_HIDDEN) &&
           x >= xpx(208) && x < xpx(310) &&
           y >= ypx(198) && y < ypx(238);
}

void md_ui_screens_setting_action_pressed(bool pressed)
{
    if (!s_settings_action) return;
    lv_obj_set_style_bg_color(s_settings_action,
        color(pressed ? s_palette.accent : s_palette.surface_alt), 0);
}

bool md_ui_screens_setting_page_at(int16_t x, int16_t y)
{
    return s_settings_page && !lv_obj_has_flag(s_settings_page, LV_OBJ_FLAG_HIDDEN) &&
           x >= xpx(109) && x < xpx(201) &&
           y >= ypx(198) && y < ypx(238);
}

bool md_ui_screens_setting_previous_at(int16_t x, int16_t y)
{
    return s_settings_previous &&
           !lv_obj_has_flag(s_settings_previous, LV_OBJ_FLAG_HIDDEN) &&
           x >= xpx(10) && x < xpx(102) &&
           y >= ypx(198) && y < ypx(238);
}

void md_ui_screens_setting_page_pressed(bool pressed)
{
    if (!s_settings_page) return;
    lv_obj_set_style_bg_color(s_settings_page,
        color(pressed ? s_palette.accent : s_palette.surface_alt), 0);
}

void md_ui_screens_setting_previous_pressed(bool pressed)
{
    if (!s_settings_previous) return;
    lv_obj_set_style_bg_color(s_settings_previous,
        color(pressed ? s_palette.accent : s_palette.surface_alt), 0);
}

void md_ui_screens_calibration_step(uint8_t step)
{
    if (!s_calibration_target || !s_calibration_instruction) return;
    static const uint16_t targets[5][2] = {
        {35, 40}, {285, 40}, {285, 200}, {35, 200}, {160, 120}};
    if (step >= 5) return;
    lv_obj_set_pos(s_calibration_target, xpx(targets[step][0] - 10),
                    ypx(targets[step][1] - 16));
    char message[48];
    if (s_language == MD_UI_LANGUAGE_RUSSIAN)
        (void)snprintf(message, sizeof(message), "КОСНИТЕСЬ МЕТКИ %u / 5", step + 1U);
    else
        (void)snprintf(message, sizeof(message), "Tap cross %u / 5", step + 1U);
    lv_label_set_text(s_calibration_instruction, message);
}

int md_ui_screens_signal_at(md_ui_screen_t screen,
                            const md_ui_settings_t *settings,
                            int16_t x, int16_t y)
{
    if (!s_created || screen >= MD_UI_SCREEN_SETTINGS) return -1;
    screen_view_t *view = view_for(screen, settings);
    if (!view) return -1;
    for (unsigned i = 0; i < view->field_count; ++i) {
        field_view_t *field = &view->fields[i];
        if (field->kind != FIELD_SIGNAL || field->signal_id >= MD_SIGNAL_COUNT ||
            !field->card) continue;
        const int left = lv_obj_get_x(field->card);
        const int top = lv_obj_get_y(field->card);
        if (x >= left && y >= top && x < left + lv_obj_get_width(field->card) &&
            y < top + lv_obj_get_height(field->card)) return (int)field->signal_id;
    }
    return -1;
}

int md_ui_screens_widget_slot_at(md_ui_screen_t screen,
                                 const md_ui_settings_t *settings,
                                 int16_t x, int16_t y)
{
    if (!s_created || !settings) return -1;
    screen_view_t *view = view_for(screen, settings);
    if (!view) return -1;
    const md_ui_card_group_t group = md_ui_card_group_for_screen(screen, settings);
    for (unsigned i = 0; i < view->field_count; ++i) {
        field_view_t *field = &view->fields[i];
        if (field->kind != FIELD_SIGNAL || !field->card) continue;
        const bool custom_slot = group == MD_UI_CARD_GROUP_CUSTOM &&
            field->card_group == MD_UI_CARD_GROUP_CUSTOM &&
            field->widget_slot < settings->widget_count;
        const bool fixed_slot = group >= 0 && group < MD_UI_CARD_GROUP_COUNT &&
            field->card_group == group && field->card_slot < MD_UI_CARD_SLOTS_PER_GROUP;
        if (!custom_slot && !fixed_slot) continue;
        const int left = lv_obj_get_x(field->card);
        const int top = lv_obj_get_y(field->card);
        if (x >= left && y >= top && x < left + lv_obj_get_width(field->card) &&
            y < top + lv_obj_get_height(field->card))
            return (int)(custom_slot ? field->widget_slot : field->card_slot);
    }
    return -1;
}

int md_ui_screens_widget_picker_signal_at(int16_t x, int16_t y)
{
    if (!s_created) return -1;
    for (unsigned i = 0; i < MD_SIGNAL_COUNT; ++i) {
        const lv_obj_t *card = s_widget_picker_options[i].card;
        if (!card) continue;
        const int left = lv_obj_get_x(card);
        const int top = lv_obj_get_y(card);
        if (x >= left && y >= top && x < left + lv_obj_get_width(card) &&
            y < top + lv_obj_get_height(card)) return (int)i;
    }
    return -1;
}

void md_ui_screens_nav_pressed(md_ui_screen_t screen,
                                const md_ui_settings_t *settings,
                                int direction)
{
    screen_view_t *view = view_for(screen, settings);
    if (!view) return;
    if (view->previous_hint)
        lv_obj_set_style_text_opa(view->previous_hint,
                                   direction < 0 ? LV_OPA_COVER : LV_OPA_70, 0);
    if (view->next_hint)
        lv_obj_set_style_text_opa(view->next_hint,
                                   direction > 0 ? LV_OPA_COVER : LV_OPA_70, 0);
}

void md_ui_screens_touch_feedback(md_ui_screen_t screen,
                                   const md_ui_settings_t *settings,
                                   int16_t x, int16_t y, bool pressed)
{
    screen_view_t *view = view_for(screen, settings);
    if (!view) return;
    for (unsigned i = 0; i < view->field_count; ++i) {
        field_view_t *field = &view->fields[i];
        if (field->kind != FIELD_SIGNAL || !field->card) continue;
        const int left = lv_obj_get_x(field->card);
        const int top = lv_obj_get_y(field->card);
        const bool inside = pressed && x >= left && y >= top &&
            x < left + lv_obj_get_width(field->card) &&
            y < top + lv_obj_get_height(field->card);
        lv_obj_set_style_border_color(field->card,
            color(inside ? s_palette.accent : s_palette.border), 0);
        lv_obj_set_style_border_width(field->card, inside ? 2 : s_theme_style.border_width, 0);
    }
}

void md_ui_screens_debug_touch(int16_t x, int16_t y)
{
#if CONFIG_MINIDASH_TOUCH_DEBUG_OVERLAY
    if (s_debug_label) {
        char value[32];
        (void)snprintf(value, sizeof(value), "TOUCH %d,%d", (int)x, (int)y);
        lv_label_set_text(s_debug_label, value);
    }
#else
    (void)x;
    (void)y;
#endif
}

void md_ui_screens_show(md_ui_screen_t screen,
                        const md_ui_settings_t *settings)
{
    if (!s_created) return;
    screen_view_t *view = view_for(screen, settings);
    if (screen==MD_UI_SCREEN_SETTINGS) {
        for(unsigned i=0;i<MD_UI_SETTINGS_ROWS;++i)
            if(s_setting_rows[i].row) settings_row_reset(&s_setting_rows[i]);
    }
    if (view && view->root) lv_screen_load(view->root);
}

static void retheme_view(screen_view_t *view)
{
    if (!view || !view->root) return;
    lv_obj_set_style_bg_color(view->root, color(s_palette.background), 0);
    lv_obj_set_style_text_color(view->title, color(s_palette.text), 0);
    lv_obj_set_style_text_color(view->mode, color(s_palette.accent), 0);
    lv_obj_set_style_text_color(view->footer, color(s_palette.muted), 0);
    lv_obj_set_style_text_letter_space(view->title, s_theme_style.letter_spacing, 0);
    lv_obj_invalidate(view->root);
    if (view->previous_hint) lv_obj_set_style_text_color(view->previous_hint, color(s_palette.accent), 0);
    if (view->next_hint) lv_obj_set_style_text_color(view->next_hint, color(s_palette.accent), 0);
    for (unsigned i = 0; i < view->field_count; ++i) {
        field_view_t *field = &view->fields[i];
        themed_frame(field->card);
        lv_obj_set_style_bg_color(field->card, color(s_palette.surface), 0);
        lv_obj_set_style_border_color(field->card, color(s_palette.border), 0);
        lv_obj_set_style_text_color(field->title, color(s_palette.muted), 0);
        if (field->unit) lv_obj_set_style_text_color(field->unit, color(s_palette.muted), 0);
        if (field->value) lv_obj_set_style_text_color(field->value, color(s_palette.text), 0);
        if (field->status) lv_obj_set_style_text_color(field->status, color(s_palette.unavailable), 0);
        if (field->visual) {
            if (field->widget_type==MD_UI_WIDGET_BAR) lv_obj_set_style_bg_color(field->visual,color(s_palette.accent),LV_PART_INDICATOR);
            else if (field->series) lv_chart_set_series_color(field->visual,field->series,color(s_palette.accent));
        }
        field->color_set = false;
    }
}

void md_ui_screens_retheme(const md_ui_palette_t *palette, md_ui_theme_id_t theme)
{
    if (!palette || !s_created) return;
    s_palette = *palette;
    s_theme_style = md_ui_theme_style(theme);
    apply_language_fonts();
    for (unsigned i = 0; i < MD_UI_DASHBOARD_COUNT; ++i) retheme_view(&s_dashboards[i]);
    for (unsigned i = 1; i < MD_UI_SCREEN_COUNT; ++i) {
        if (i == MD_UI_SCREEN_CUSTOM) continue;
        retheme_view(&s_screens[i]);
    }
    for (unsigned i = 0; i < MD_UI_SETTINGS_ROWS; ++i) {
        setting_row_view_t *row = &s_setting_rows[i];
        if (!row->row) continue;
        themed_frame(row->row);
        lv_obj_set_style_bg_color(row->row, color(s_palette.surface), 0);
        lv_obj_set_style_text_color(row->title, color(s_palette.text), 0);
        lv_obj_set_style_text_color(row->value, color(s_palette.accent), 0);
        settings_row_reset(row);
    }
    for (unsigned i = 0; i < MD_SIGNAL_COUNT; ++i) {
        widget_picker_option_view_t *option = &s_widget_picker_options[i];
        if (!option->card || !option->label) continue;
        themed_frame(option->card);
        lv_obj_set_style_bg_color(option->card, color(s_palette.surface), 0);
        lv_obj_set_style_border_color(option->card, color(s_palette.border), 0);
        lv_obj_set_style_text_color(option->label, color(s_palette.text), 0);
        option->selected = false;
    }
    if(s_wifi_switch) {
        lv_obj_set_style_bg_color(s_wifi_switch,color(s_palette.border),LV_PART_MAIN);
        lv_obj_set_style_bg_color(s_wifi_switch,color(s_palette.accent),LV_PART_INDICATOR|LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(s_wifi_switch,color(s_palette.text),LV_PART_KNOB);
    }
    themed_frame(s_settings_action); themed_frame(s_settings_page); themed_frame(s_settings_previous);
    md_ui_screens_setting_action_pressed(false);
    md_ui_screens_setting_page_pressed(false);
    md_ui_screens_setting_previous_pressed(false);
    if (s_settings_action) {
        lv_obj_set_style_border_color(s_settings_action, color(s_palette.accent), 0);
        lv_obj_set_style_text_color(s_settings_action_label, color(s_palette.text), 0);
    }
    if (s_settings_page) {
        lv_obj_set_style_border_color(s_settings_page, color(s_palette.accent), 0);
        lv_obj_set_style_text_color(s_settings_page_label, color(s_palette.text), 0);
    }
    if (s_settings_previous) {
        lv_obj_set_style_border_color(s_settings_previous, color(s_palette.accent), 0);
        lv_obj_set_style_text_color(s_settings_previous_label, color(s_palette.text), 0);
    }
}

static void update_metric_field(field_view_t *field,
                                const md_ui_settings_t *settings,
                                const md_ui_signal_store_t *signals,
                                uint32_t now_ms)
{
    md_ui_signal_snapshot_t snapshot;
    const md_ui_signal_definition_t *definition = NULL;
    char formatted_value[32] = "--";
    char formatted_unit[12] = "";
    const int id = md_ui_signal_id_from_name(field->canonical_signal);
    if (id < 0) {
        snapshot = (md_ui_signal_snapshot_t){.state = MD_UI_SIGNAL_UNSUPPORTED};
    } else {
        definition = &md_ui_signal_definitions[id];
        snapshot = md_ui_signal_snapshot(signals, (md_ui_signal_id_t)id, now_ms);
    }
    md_ui_format_signal(definition, &snapshot, settings,
                        formatted_value, sizeof(formatted_value),
                        formatted_unit, sizeof(formatted_unit));

    const md_ui_page_widget_t *spec = field->card_group == MD_UI_CARD_GROUP_CUSTOM &&
        field->widget_slot < s_page_model.widget_count ? &s_page_model.widgets[field->widget_slot] : NULL;
    md_ui_page_widget_t focus_spec={0};
    if (!spec && field->visual && id>=0) {
        focus_spec.minimum=s_history[id].minimum;
        focus_spec.maximum=s_history[id].maximum;
        if (focus_spec.maximum<=focus_spec.minimum) focus_spec.maximum=focus_spec.minimum+1;
        focus_spec.precision=definition ? definition->decimals : 0; spec=&focus_spec;
    }
    if (spec && snapshot.value_available && definition)
        md_ui_format_signal_precision(definition,&snapshot,settings,spec->precision,
            formatted_value,sizeof(formatted_value),formatted_unit,sizeof(formatted_unit));
    if (field->widget_type == MD_UI_WIDGET_STATUS) {
        snprintf(formatted_value, sizeof(formatted_value), "%s", md_ui_i18n_text(s_language,
            snapshot.demo && snapshot.value_available ? "DEMO" : md_ui_signal_state_label(snapshot.state)));
        formatted_unit[0] = 0;
    }
    if (field->visual && spec && id >= 0) {
        md_ui_widget_view_t projected;
        md_ui_project_widget(&projected, (md_ui_signal_id_t)id, settings, signals, now_ms,
            spec->minimum, spec->maximum, &s_history[id]);
        if (!projected.signal.value_available) lv_obj_add_flag(field->visual, LV_OBJ_FLAG_HIDDEN);
        else {
            lv_obj_remove_flag(field->visual, LV_OBJ_FLAG_HIDDEN);
            if (field->widget_type == MD_UI_WIDGET_BAR) lv_bar_set_value(field->visual, projected.level_per_mille, LV_ANIM_OFF);
            else if (field->series) {
                lv_chart_set_axis_range(field->visual, LV_CHART_AXIS_PRIMARY_Y, 0, 1000);
                for (unsigned i = 0; i < MD_UI_HISTORY_SAMPLES; ++i) {
                    int32_t point = LV_CHART_POINT_NONE;
                    if (i < s_history[id].count) {
                        unsigned index = (s_history[id].head + MD_UI_HISTORY_SAMPLES - s_history[id].count + i) % MD_UI_HISTORY_SAMPLES;
                        float ratio = (s_history[id].samples[index] - spec->minimum) / (spec->maximum - spec->minimum);
                        point = (int32_t)(fmaxf(0, fminf(1, ratio)) * 1000);
                    }
                    lv_chart_set_series_value_by_id(field->visual, field->series, i, point);
                }
                lv_chart_refresh(field->visual);
            }
        }
    }
    if (!snapshot.value_available && field->widget_type != MD_UI_WIDGET_STATUS) {
        (void)snprintf(formatted_value, sizeof(formatted_value), "--");
        formatted_unit[0] = '\0';
    } else if (field->inline_unit && formatted_unit[0]) {
        char combined[sizeof(formatted_value)];
        (void)snprintf(combined, sizeof(combined), "%.20s %.10s",
                       formatted_value, formatted_unit);
        (void)snprintf(formatted_value, sizeof(formatted_value), "%s", combined);
        formatted_unit[0] = '\0';
    }
    const char *state_label = snapshot.demo && snapshot.value_available
        ? "DEMO" : md_ui_signal_state_label(snapshot.state);
    set_static_text(field->value, field->value_text, sizeof(field->value_text), formatted_value);
    set_static_text(field->unit, field->unit_text, sizeof(field->unit_text), formatted_unit);
    set_ui_text(field->status, field->status_text,
                sizeof(field->status_text), state_label);
    const md_ui_severity_t severity = snapshot.value_available
        ? md_ui_signal_severity(settings, field->signal_id, snapshot.value, snapshot.state)
        : MD_UI_SEVERITY_NORMAL;
    const uint32_t signal_color = md_ui_theme_signal_color(&s_palette, snapshot.state, severity);
    if (!field->color_set || field->rendered_color != signal_color) {
        lv_obj_set_style_text_color(field->value, color(signal_color), 0);
        lv_obj_set_style_text_color(field->status, color(signal_color), 0);
        field->rendered_color = signal_color;
        field->color_set = true;
    }
}

static void refresh_metric_binding(field_view_t *field,
                                   const md_ui_settings_t *settings)
{
    if (!field || !settings || field->kind != FIELD_SIGNAL) return;
    md_ui_signal_id_t signal = field->default_signal_id;
    if (field->card_group == MD_UI_CARD_GROUP_CUSTOM) {
        if (field->widget_slot >= settings->widget_count) return;
        const int configured = md_ui_signal_id_from_name(
            settings->widgets[field->widget_slot].canonical_signal);
        if (configured < 0 || configured >= MD_SIGNAL_COUNT) return;
        signal = (md_ui_signal_id_t)configured;
    } else if (field->card_group >= 0 &&
               field->card_group < MD_UI_CARD_GROUP_COUNT) {
        signal = md_ui_settings_card_signal(settings, field->card_group,
                                            field->card_slot);
    }
    const md_ui_signal_definition_t *definition = md_ui_signal_definition(signal);
    if (!definition) return;
    field->signal_id = signal;
    (void)snprintf(field->canonical_signal, sizeof(field->canonical_signal),
                   "%s", definition->canonical_name);
    const char *label = field->compact_title
        ? md_ui_i18n_compact_signal_label(s_language, signal, field->title_key)
        : md_ui_i18n_signal_label(s_language, signal);
    if (field->card_group == MD_UI_CARD_GROUP_CUSTOM && field->widget_slot < s_page_model.widget_count &&
        s_page_model.widgets[field->widget_slot].signal == signal && s_page_model.widgets[field->widget_slot].label[0])
        label = s_page_model.widgets[field->widget_slot].label;
    if (field->card_group==MD_UI_CARD_GROUP_CUSTOM)
        lv_obj_set_style_text_font(field->title,s_theme_style.font_family==1 ? ui_font_for_language(field->title_font) : (field->title_font==&lv_font_montserrat_10 ? &md_ui_font_ru_10 : &md_ui_font_ru_12),0);
    set_static_text(field->title, field->title_text,
                    sizeof(field->title_text), label);
}

static void mode_text(const md_ui_signal_summary_t *summary, char *out, size_t size)
{
    if (summary->has_valid_live_data) {
        (void)snprintf(out, size, "LIVE");
    } else if (summary->has_valid_demo_data) {
        (void)snprintf(out, size, "DEMO");
    } else if (summary->stale_count > 0) {
        (void)snprintf(out, size, "STALE");
    } else {
        (void)snprintf(out, size, "OFFLINE");
    }
}

static void update_diagnostic_field(field_view_t *field,
                                    const md_ui_diagnostics_t *diag)
{
    char text[32] = "N/A";
    const md_ui_diagnostics_t empty_diagnostics = {0};
    if (!diag) diag = &empty_diagnostics;
    switch (field->diagnostic_key) {
    case DIAG_FIRMWARE:
        if (diag->firmware_version[0]) (void)snprintf(text, sizeof(text), "%s", diag->firmware_version);
        break;
    case DIAG_UPTIME:
        (void)snprintf(text, sizeof(text), "%lu h %02lu m",
            (unsigned long)(diag->uptime_seconds / 3600U),
            (unsigned long)((diag->uptime_seconds / 60U) % 60U));
        break;
    case DIAG_HEAP_FREE:
        (void)snprintf(text, sizeof(text), "%lu KB", (unsigned long)(diag->free_heap_bytes / 1024U));
        break;
    case DIAG_HEAP_MIN:
        (void)snprintf(text, sizeof(text), "%lu KB", (unsigned long)(diag->minimum_free_heap_bytes / 1024U));
        break;
    case DIAG_REFRESH_RATE:
        if (diag->render_metrics_available) (void)snprintf(text, sizeof(text), "%lu /s", (unsigned long)diag->refreshes_per_second);
        break;
    case DIAG_RENDER_TIME:
        if (diag->render_metrics_available) (void)snprintf(text, sizeof(text), "%lu ms", (unsigned long)diag->average_render_ms);
        break;
    case DIAG_DISPLAY:
        (void)snprintf(text, sizeof(text), "%s", diag->display_ready ? "READY" : "UNAVAILABLE");
        break;
    case DIAG_TOUCH:
        (void)snprintf(text, sizeof(text), "%s", diag->touch_ready ? "READY" : "UNAVAILABLE");
        break;
    case DIAG_BRIGHTNESS:
        (void)snprintf(text, sizeof(text), "%s", diag->brightness_control ? "PWM" : "FIXED");
        break;
    case DIAG_TRANSPORT:
        if (diag->transport_available && diag->transport_state[0]) {
            (void)snprintf(text, sizeof(text), "%s", diag->transport_state);
        }
        break;
    case DIAG_ECUS:
        if (diag->ecu_count_available) (void)snprintf(text, sizeof(text), "%u", diag->ecu_count);
        break;
    case DIAG_SIGNALS:
        (void)snprintf(text, sizeof(text), "%u / %u", diag->signal_summary.sample_count,
                       MD_SIGNAL_COUNT);
        break;
    case DIAG_STALE:
        (void)snprintf(text, sizeof(text), "%u", diag->signal_summary.stale_count);
        break;
    case DIAG_ERRORS:
        (void)snprintf(text, sizeof(text), "%u", diag->signal_summary.error_count);
        break;
    }
    set_ui_text(field->value, field->value_text,
                sizeof(field->value_text), text);
}

static void settings_value(uint8_t index, const md_ui_settings_t *settings,
                           bool brightness_control, bool wifi_active, char *output, size_t capacity)
{
    static const char *const dashboard[] = {"OEM", "SPORT", "MINIMAL", "CUSTOM"};
    static const char *const day_night[] = {"AUTO", "DAY", "NIGHT"};
    static const char *const speed[] = {"km/h", "mph"};
    static const char *const temperature[] = {"\xc2\xb0" "C", "\xc2\xb0" "F"};
    const char *value = "N/A";
    switch (index) {
    case 0: value = dashboard[settings->dashboard]; break;
    case 1: value = md_ui_theme_name(settings->theme); break;
    case 2: value = day_night[settings->day_night]; break;
    case 3: value = speed[settings->units]; break;
    case 4: value = temperature[settings->temperature_unit]; break;
    case 5:
        if (brightness_control) {
            (void)snprintf(output, capacity, "%u%%", settings->brightness_pct);
            return;
        }
        value = "FIXED";
        break;
    case 6: {
        bool any_threshold = false;
        for (unsigned i = 0; i < MD_UI_MAX_WARNING_THRESHOLDS; ++i) {
            any_threshold |= settings->warnings[i].enabled;
        }
        value = !settings->warnings_enabled ? "DISABLED"
            : (any_threshold ? "ENABLED" : "NO THRESHOLDS");
        break;
    }
    case 7: value = "START"; break;
    case 8: case 9: case 10: case 11:
        value = settings->warnings[index - 8U].enabled ? "ENABLED" : "DISABLED";
        break;
    case 12: {
        static const char *const layout[] = {
            "CUSTOM", "1 WIDGET", "2 WIDGETS", "4 WIDGETS", "6 WIDGETS",
            "8 WIDGETS", "1 LARGE + 2", "1 LARGE + 4", "2 LARGE + 4"};
        value = layout[settings->layout];
        break;
    }
    case 23: snprintf(output, capacity, "%u°", settings->rotation * 90U); return;
    case 22: value = wifi_active ? "ON" : "OFF"; break;
    case 13:
        value = settings->language == MD_UI_LANGUAGE_RUSSIAN
            ? "RUSSIAN" : "ENGLISH";
        break;
    default:
        if (index >= 14U && index < 22U) {
            const unsigned widget = index - 14U;
            if (widget >= settings->widget_count) {
                value = "INACTIVE";
            } else {
                const int signal = md_ui_signal_id_from_name(
                    settings->widgets[widget].canonical_signal);
                value = signal >= 0 && signal < MD_SIGNAL_COUNT
                    ? md_ui_i18n_compact_signal_label(settings->language,
                        (md_ui_signal_id_t)signal, "SIGNAL") : "UNKNOWN";
            }
        }
        break;
    }
    (void)snprintf(output, capacity, "%s",
                   md_ui_i18n_text(settings->language, value));
}

void md_ui_screens_update(md_ui_screen_t screen,
                          const md_ui_settings_t *settings,
                          const md_ui_state_t *state,
                          const md_ui_signal_store_t *signals,
                          uint32_t now_ms,
                          const md_ui_diagnostics_t *diagnostics)
{
    if (!s_created || !settings || !state || !signals) return;
    for (unsigned i = 0; i < MD_SIGNAL_COUNT; ++i)
        md_ui_history_observe(&s_history[i], &signals->signals[i], now_ms);
    s_language = settings->language;
    if (s_font_language != s_language) apply_language_fonts();
    screen_view_t *view = view_for(screen, settings);
    if (!view || !view->root) return;
    set_ui_text(view->title, view->title_text, sizeof(view->title_text),
                view->title_key);
    set_ui_text(view->footer, view->footer_text, sizeof(view->footer_text),
                view->footer_key);
    const md_ui_signal_summary_t summary = md_ui_signal_summary(signals, now_ms);
    char mode[32];
    if (screen == MD_UI_SCREEN_SETTINGS) {
        const unsigned page =
            (unsigned)(state->selected_setting / MD_UI_SETTINGS_ROWS_PER_PAGE) + 1U;
        if (s_language == MD_UI_LANGUAGE_RUSSIAN)
            (void)snprintf(mode, sizeof(mode), "СТР. %u/%u", page,
                           (unsigned)UI_SETTINGS_PAGE_COUNT);
        else
            (void)snprintf(mode, sizeof(mode), "PAGE %u/%u", page,
                           (unsigned)UI_SETTINGS_PAGE_COUNT);
    } else if (screen == MD_UI_SCREEN_WIDGET_PICKER) {
        (void)snprintf(mode, sizeof(mode), "%s",
                       md_ui_i18n_text(s_language, "CHOOSE"));
    } else {
        mode_text(&summary, mode, sizeof(mode));
    }
    set_ui_text(view->mode, view->mode_text, sizeof(view->mode_text), mode);

    if (screen == MD_UI_SCREEN_WIDGET_PICKER) {
        char title[48];
        (void)snprintf(title, sizeof(title), "%s %u",
                       md_ui_i18n_text(s_language, "REPLACE WIDGET"),
                       (unsigned)state->widget_picker_slot + 1U);
        set_static_text(view->title, s_widget_picker_title_text,
                        sizeof(s_widget_picker_title_text), title);
        for (unsigned i = 0; i < MD_SIGNAL_COUNT; ++i) {
            widget_picker_option_view_t *option = &s_widget_picker_options[i];
            if (!option->card || !option->label) continue;
            set_ui_text(option->label, option->label_text,
                        sizeof(option->label_text),
                        md_ui_i18n_compact_signal_label(s_language,
                            (md_ui_signal_id_t)i, "SIGNAL"));
            const bool selected = state->widget_picker_signal == (md_ui_signal_id_t)i;
            if (option->selected == selected) continue;
            option->selected = selected;
            lv_obj_set_style_bg_color(option->card,
                color(selected ? s_palette.surface_alt : s_palette.surface), 0);
            lv_obj_set_style_border_color(option->card,
                color(selected ? s_palette.accent : s_palette.border), 0);
            lv_obj_set_style_border_width(option->card, selected ? 2 : 1, 0);
            lv_obj_set_style_text_color(option->label,
                color(selected ? s_palette.accent : s_palette.text), 0);
        }
    }

    for (unsigned i = 0; i < view->field_count; ++i) {
        field_view_t *field = &view->fields[i];
        if (field->kind == FIELD_DIAGNOSTIC) {
            set_ui_text(field->title, field->title_text,
                        sizeof(field->title_text), field->title_key);
            update_diagnostic_field(field, diagnostics);
        } else {
            refresh_metric_binding(field, settings);
            update_metric_field(field, settings, signals, now_ms);
        }
    }

    if (screen == MD_UI_SCREEN_FOCUS && state->focused_signal < MD_SIGNAL_COUNT && s_focus_stats) {
        const md_ui_history_t *h = &s_history[state->focused_signal];
        char low[32] = "--", high[32] = "--", unit[12];
        if (h->valid && h->seen) {
            md_ui_signal_snapshot_t sample = {.state = MD_UI_SIGNAL_VALID, .value_available = true, .value = h->minimum};
            md_ui_format_signal(md_ui_signal_definition(state->focused_signal), &sample, settings, low, sizeof(low), unit, sizeof(unit));
            sample.value = h->maximum;
            md_ui_format_signal(md_ui_signal_definition(state->focused_signal), &sample, settings, high, sizeof(high), unit, sizeof(unit));
        }
        snprintf(s_focus_stats_text, sizeof(s_focus_stats_text), "MIN %s | MAX %s | N %u%s", low, high,
            h->valid ? h->count : 0, h->demo && h->valid ? " | DEMO" : "");
        lv_label_set_text(s_focus_stats, s_focus_stats_text);
    }
    if ((screen == MD_UI_SCREEN_CUSTOM || (screen == MD_UI_SCREEN_DASHBOARD && settings->dashboard == MD_UI_DASHBOARD_CUSTOM)) && s_page_model.title[0])
    {
        lv_obj_set_style_text_font(view->title,s_theme_style.font_family==1 ? &md_ui_font_mono_16 : &md_ui_font_ru_16,0);
        set_static_text(view->title, view->title_text, sizeof(view->title_text), s_page_model.title);
    }
    if (screen == MD_UI_SCREEN_SETTINGS) {
        if(s_wifi_switch) {
            if(diagnostics && diagnostics->wifi_active) lv_obj_add_state(s_wifi_switch,LV_STATE_CHECKED);
            else lv_obj_remove_state(s_wifi_switch,LV_STATE_CHECKED);
        }
        if (s_settings_action_label) {
            const char *action_label = state->selected_setting == 7 ? "START" : "CHANGE";
            if (state->selected_setting == 5 &&
                !(diagnostics && diagnostics->brightness_control)) action_label = "FIXED";
            if (state->selected_setting >= 14U && state->selected_setting < 22U &&
                state->selected_setting - 14U >= settings->widget_count)
                action_label = "INACTIVE";
            set_ui_text(s_settings_action_label, s_settings_action_text,
                        sizeof(s_settings_action_text), action_label);
            lv_obj_center(s_settings_action_label);
        }
        if (s_settings_page_label) {
            const unsigned page = state->selected_setting / MD_UI_SETTINGS_ROWS_PER_PAGE;
            set_ui_text(s_settings_page_label, s_settings_page_text,
                        sizeof(s_settings_page_text), "MORE");
            lv_obj_center(s_settings_page_label);
            if (page + 1U < UI_SETTINGS_PAGE_COUNT)
                lv_obj_remove_flag(s_settings_page, LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_add_flag(s_settings_page, LV_OBJ_FLAG_HIDDEN);
        }
        if (s_settings_previous) {
            const unsigned page = state->selected_setting / MD_UI_SETTINGS_ROWS_PER_PAGE;
            if (page > 0U) lv_obj_remove_flag(s_settings_previous, LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(s_settings_previous, LV_OBJ_FLAG_HIDDEN);
        }
        for (unsigned i = 0; i < MD_UI_SETTINGS_ROWS; ++i) {
            setting_row_view_t *row = &s_setting_rows[i];
            if (!row->row) continue;
            set_ui_text(row->title, row->title_text,
                        sizeof(row->title_text), row->title_key);
            if (i / MD_UI_SETTINGS_ROWS_PER_PAGE ==
                state->selected_setting / MD_UI_SETTINGS_ROWS_PER_PAGE)
                lv_obj_remove_flag(row->row, LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_add_flag(row->row, LV_OBJ_FLAG_HIDDEN);
            char value[32];
            settings_value((uint8_t)i, settings,
                           diagnostics && diagnostics->brightness_control, diagnostics && diagnostics->wifi_active,
                           value, sizeof(value));
            set_static_text(row->value, row->value_text, sizeof(row->value_text), value);
            const bool selected = i == state->selected_setting;
            settings_row_select(row,selected);
        }
    }
}

void md_ui_screens_configuration(const md_device_configuration_t *config)
{
    if (config && config->common.active_page < config->common.page_count)
        s_page_model = config->common.pages[config->common.active_page];
}
void md_ui_screens_config_credentials(const char *ssid, const char *password)
{
    if (!s_config_credentials) return;
    char text[192];
    snprintf(text, sizeof(text), "SSID: %s\nPassword: %s\n\nhttp://192.168.4.1\nSettings > Wi-Fi to stop", ssid, password);
    lv_label_set_text(s_config_credentials, text);
}
static void renderer_show(void *context, md_ui_screen_t screen, const md_ui_settings_t *settings)
{ (void)context; md_ui_screens_show(screen, settings); }
static void renderer_update(void *context, md_ui_screen_t screen, const md_ui_settings_t *settings,
    const md_ui_state_t *state, const md_ui_signal_store_t *store, uint32_t now_ms, const md_ui_diagnostics_t *diagnostics)
{ (void)context; md_ui_screens_update(screen, settings, state, store, now_ms, diagnostics); }
const md_ui_renderer_t *md_ui_lvgl_renderer(void)
{
    static const md_ui_renderer_t renderer = {.show = renderer_show, .update = renderer_update};
    return &renderer;
}

static lv_obj_t *startup_object(void)
{
    lv_obj_t *object = lv_obj_create(lv_layer_top());
    if (!object) return NULL;
    lv_obj_remove_style_all(object);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(object, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
    return object;
}
void md_ui_screens_startup_end(void)
{
    for(unsigned i=0;i<4;++i) {
        if(s_startup_masks[i]) lv_obj_delete(s_startup_masks[i]);
        s_startup_masks[i]=NULL;
    }
    if(s_startup_glow) lv_obj_delete(s_startup_glow);
    s_startup_glow=NULL;
}
bool md_ui_screens_startup_begin(void)
{
    if(!s_created) return false;
    md_ui_screens_startup_end();
    for(unsigned i=0;i<4;++i) {
        s_startup_masks[i]=startup_object();
        if(!s_startup_masks[i]) { md_ui_screens_startup_end(); return false; }
    }
    s_startup_glow=startup_object();
    if(!s_startup_glow) { md_ui_screens_startup_end(); return false; }
    lv_obj_set_style_bg_color(s_startup_glow, lv_color_hex(0xe8fff0), 0);
    lv_obj_set_style_border_color(s_startup_glow, color(s_palette.accent), 0);
    lv_obj_set_style_border_width(s_startup_glow, 2, 0);
    lv_obj_set_style_shadow_color(s_startup_glow, color(s_palette.accent), 0);
    lv_obj_set_style_shadow_width(s_startup_glow, 8, 0);
    md_ui_screens_startup_frame(0);
    return true;
}
static void startup_mask(unsigned index,int x,int y,int width,int height)
{
    lv_obj_t *object=s_startup_masks[index];
    if(width<=0 || height<=0) { lv_obj_add_flag(object,LV_OBJ_FLAG_HIDDEN); return; }
    lv_obj_clear_flag(object,LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(object,x,y);lv_obj_set_size(object,width,height);
}
void md_ui_screens_startup_frame(uint32_t elapsed)
{
    if(!s_startup_glow) return;
    const md_ui_startup_frame_t f=md_ui_startup_frame(elapsed,s_width,s_height,100);
    const int left=(s_width-f.width)/2, top=(s_height-f.height)/2;
    startup_mask(0,0,0,s_width,top);
    startup_mask(1,0,top+f.height,s_width,s_height-top-f.height);
    startup_mask(2,0,top,left,f.height);
    startup_mask(3,left+f.width,top,s_width-left-f.width,f.height);
    lv_obj_set_pos(s_startup_glow,left,top);
    lv_obj_set_size(s_startup_glow,f.width,f.height);
    lv_obj_set_style_bg_opa(s_startup_glow,elapsed<300 ? LV_OPA_COVER :
                            (elapsed<800 ? (800-elapsed)*255/500 : 0),0);
    lv_obj_set_style_border_opa(s_startup_glow,f.glow_opacity,0);
    lv_obj_set_style_shadow_opa(s_startup_glow,f.glow_opacity,0);
}

/* Isolated actual-LVGL regression test; never loads or changes the owner's scene. */
bool md_ui_screens_selftest(void)
{
    lv_obj_t *root=lv_obj_create(NULL); if(!root) return false;
    bool ok=true; char cached[24]="PREV";
    const lv_font_t *mono[]={&md_ui_font_mono_10,&md_ui_font_mono_12,&md_ui_font_mono_14,&md_ui_font_mono_16};
    const uint32_t glyphs[]={0x410,0x42f,0x430,0x44f,0x401,0x451,'0','9',0xb0};
    for(unsigned i=0;i<4;i++) for(unsigned j=0;j<sizeof(glyphs)/sizeof(glyphs[0]);j++) {
        lv_font_glyph_dsc_t glyph;
        ok=ok && lv_font_get_glyph_dsc(mono[i],&glyph,glyphs[j],0);
    }
    const lv_font_t *cluster[]={&md_ui_font_cluster_16,&md_ui_font_cluster_20,&md_ui_font_cluster_28,&md_ui_font_cluster_48};
    for(unsigned i=0;i<4;i++) for(unsigned ch='0';ch<='9';ch++) {
        lv_font_glyph_dsc_t glyph; ok=ok && lv_font_get_glyph_dsc(cluster[i],&glyph,ch,0);
    }
    for(unsigned i=0;i<2;++i) {
        lv_obj_t *label=lv_label_create(root);
        set_static_text(label,cached,sizeof(cached),"PREV");
        ok=ok && strcmp(lv_label_get_text(label),"PREV")==0 && lv_label_get_text(label)==cached;
        lv_obj_delete(label); /* Next new object reuses the unchanged cached string. */
    }
    setting_row_view_t rows[4]={0};
    for(unsigned i=0;i<4;++i) { rows[i].row=lv_obj_create(root); lv_obj_remove_style_all(rows[i].row); }
    for(unsigned cycle=0;cycle<8;++cycle) {
        const unsigned current=cycle%4;
        for(unsigned i=0;i<4;++i) settings_row_select(&rows[i],i==current);
        if(cycle%2) { /* Theme reset must erase old borders and invalidate cache. */
            for(unsigned i=0;i<4;++i) { themed_frame(rows[i].row); settings_row_reset(&rows[i]); }
            for(unsigned i=0;i<4;++i) settings_row_select(&rows[i],i==current);
        }
        unsigned count=0;
        for(unsigned i=0;i<4;++i) {
            const bool highlighted=lv_obj_get_style_border_width(rows[i].row,LV_PART_MAIN)>0;
            if(highlighted) ++count;
            ok=ok && highlighted==(i==current);
            if(i!=current) ok=ok && lv_obj_get_style_shadow_opa(rows[i].row,LV_PART_MAIN)==LV_OPA_TRANSP;
        }
        ok=ok && count==1;
    }
    lv_obj_delete(root); return ok;
}
