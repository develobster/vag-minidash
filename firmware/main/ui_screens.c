#include "ui_screens.h"

#include <stdio.h>
#include <string.h>

#include "lvgl.h"

#define UI_FIELDS_PER_VIEW 14
#define UI_SETTINGS_ROWS 7
#define UI_DIAG_ROWS 7

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
    diagnostic_key_t diagnostic_key;
    bool primary;
    char canonical_signal[MD_UI_SIGNAL_NAME_LEN];
    char value_text[32];
    char unit_text[12];
    char status_text[16];
    uint32_t rendered_color;
    bool color_set;
    bool inline_unit;
} field_view_t;

typedef struct {
    lv_obj_t *root;
    lv_obj_t *title;
    lv_obj_t *mode;
    lv_obj_t *footer;
    field_view_t fields[UI_FIELDS_PER_VIEW];
    uint8_t field_count;
    char mode_text[12];
} screen_view_t;

typedef struct {
    lv_obj_t *row;
    lv_obj_t *title;
    lv_obj_t *value;
    char title_text[24];
    char value_text[32];
    bool selected;
} setting_row_view_t;

static screen_view_t s_dashboards[MD_UI_DASHBOARD_COUNT];
static screen_view_t s_screens[MD_UI_SCREEN_COUNT];
static setting_row_view_t s_setting_rows[UI_SETTINGS_ROWS];
static md_ui_palette_t s_palette;
static uint16_t s_width = 320;
static uint16_t s_height = 240;
static bool s_created;

static lv_coord_t xpx(int value)
{
    return (lv_coord_t)((int32_t)value * s_width / 320);
}

static lv_coord_t ypx(int value)
{
    return (lv_coord_t)((int32_t)value * s_height / 240);
}

static lv_color_t color(uint32_t rgb)
{
    return lv_color_hex(rgb);
}

static void set_static_text(lv_obj_t *label, char *buffer, size_t capacity,
                            const char *text)
{
    if (!label || !buffer || capacity == 0 || !text) return;
    if (strncmp(buffer, text, capacity) == 0) return;
    (void)snprintf(buffer, capacity, "%s", text);
    lv_obj_invalidate(label);
    lv_label_set_text_static(label, buffer);
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

    view->title = lv_label_create(view->root);
    view->mode = lv_label_create(view->root);
    view->footer = lv_label_create(view->root);
    if (!view->title || !view->mode || !view->footer) return;
    lv_label_set_text_static(view->title, title);
    (void)snprintf(view->mode_text, sizeof(view->mode_text), "OFFLINE");
    lv_label_set_text_static(view->mode, view->mode_text);
    lv_label_set_text_static(view->footer, footer);

    lv_obj_set_pos(view->title, xpx(12), ypx(7));
    lv_obj_set_size(view->title, xpx(205), ypx(20));
    lv_obj_set_style_text_font(view->title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(view->title, color(s_palette.text), 0);
    lv_obj_set_style_text_letter_space(view->title, 1, 0);

    lv_obj_set_pos(view->mode, xpx(224), ypx(8));
    lv_obj_set_size(view->mode, xpx(84), ypx(18));
    lv_obj_set_style_text_font(view->mode, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(view->mode, color(s_palette.accent), 0);
    lv_obj_set_style_text_align(view->mode, LV_TEXT_ALIGN_RIGHT, 0);

    lv_obj_set_pos(view->footer, xpx(10), ypx(220));
    lv_obj_set_size(view->footer, xpx(300), ypx(16));
    lv_obj_set_style_text_font(view->footer, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(view->footer, color(s_palette.muted), 0);
}

static field_view_t *create_field(screen_view_t *view, int x, int y,
                                  int width, int height, const char *label,
                                  const char *canonical_signal, bool primary)
{
    if (!view || !view->root || view->field_count >= UI_FIELDS_PER_VIEW) return NULL;
    field_view_t *field = &view->fields[view->field_count++];
    memset(field, 0, sizeof(*field));
    field->kind = FIELD_SIGNAL;
    field->signal_id = (md_ui_signal_id_t)md_ui_signal_id_from_name(canonical_signal);
    if ((int)field->signal_id < 0 || field->signal_id >= MD_SIGNAL_COUNT) {
        field->signal_id = MD_SIGNAL_COUNT;
    }
    field->primary = primary;
    field->inline_unit = width < 150;
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
    lv_obj_set_style_radius(field->card, xpx(6), 0);
    lv_obj_set_style_pad_all(field->card, xpx(7), 0);

    field->title = lv_label_create(field->card);
    field->value = lv_label_create(field->card);
    field->unit = lv_label_create(field->card);
    field->status = lv_label_create(field->card);
    if (!field->title || !field->value || !field->unit || !field->status) return NULL;
    lv_label_set_text_static(field->title, label ? label : "SIGNAL");
    lv_label_set_text_static(field->value, "--");
    lv_label_set_text_static(field->unit, "");
    lv_label_set_text_static(field->status, "NO DATA");

    const bool wide_card = width >= 150;
    const bool compact_card = height < 70;
    lv_obj_set_pos(field->title, xpx(7), ypx(5));
    lv_obj_set_size(field->title, xpx(wide_card ? width - 92 : width - 14), ypx(16));
    lv_obj_set_style_text_font(field->title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(field->title, color(s_palette.muted), 0);

    lv_obj_set_pos(field->status, xpx(wide_card ? width - 82 : 7),
                   ypx(wide_card ? 5 : height - 19));
    lv_obj_set_size(field->status,
                    xpx(wide_card ? 74 : width - 14), ypx(14));
    lv_obj_set_style_text_font(field->status, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(field->status, color(s_palette.unavailable), 0);
    lv_obj_set_style_text_align(field->status,
        wide_card ? LV_TEXT_ALIGN_RIGHT : LV_TEXT_ALIGN_LEFT, 0);

    const int value_y = primary ? 27 : (compact_card ? 18 : 24);
    const int unit_y = height - 18;
    lv_obj_set_pos(field->value, xpx(7), ypx(value_y));
    lv_obj_set_size(field->value, xpx(width - 14), ypx(unit_y - value_y - 2));
    lv_obj_set_style_text_font(field->value,
        primary ? &lv_font_montserrat_48 :
            (compact_card ? &lv_font_montserrat_16 :
             ((width < 150 || height < 90) ? &lv_font_montserrat_20 : &lv_font_montserrat_28)), 0);
    lv_obj_set_style_text_color(field->value, color(s_palette.unavailable), 0);

    lv_obj_set_pos(field->unit, xpx(7), ypx(unit_y));
    lv_obj_set_size(field->unit, xpx(width - 14), ypx(14));
    lv_obj_set_style_text_font(field->unit, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(field->unit, color(s_palette.muted), 0);
    lv_obj_set_style_text_align(field->unit, LV_TEXT_ALIGN_RIGHT, 0);
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
    lv_obj_set_style_radius(field->card, xpx(4), 0);
    lv_obj_set_style_pad_hor(field->card, xpx(6), 0);
    lv_obj_set_style_pad_ver(field->card, ypx(2), 0);

    field->title = lv_label_create(field->card);
    field->value = lv_label_create(field->card);
    if (!field->title || !field->value) return NULL;
    lv_label_set_text_static(field->title, title);
    lv_label_set_text_static(field->value, "N/A");
    lv_obj_set_pos(field->title, xpx(5), ypx(4));
    lv_obj_set_size(field->title, xpx(70), ypx(16));
    lv_obj_set_style_text_font(field->title, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(field->title, color(s_palette.muted), 0);
    lv_obj_set_pos(field->value, xpx(76), ypx(3));
    lv_obj_set_size(field->value, xpx(width - 82), ypx(height - 6));
    lv_obj_set_style_text_font(field->value, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(field->value, color(s_palette.text), 0);
    lv_obj_set_style_text_align(field->value, LV_TEXT_ALIGN_RIGHT, 0);
    return field;
}

static void create_dashboard(screen_view_t *view, md_ui_dashboard_t dashboard,
                             const md_ui_settings_t *settings)
{
    static const char *const footers[] = {
        "TAP NEXT  |  DOUBLE PREV  |  HOLD SETTINGS",
        "TAP NEXT  |  DOUBLE PREV  |  HOLD SETTINGS",
        "TAP NEXT  |  DOUBLE PREV  |  HOLD SETTINGS",
        "CUSTOM  |  TAP NEXT  |  HOLD SETTINGS",
    };
    static const char *const titles[] = {"DRIVE", "SPORT", "MINIMAL", "CUSTOM"};
    style_root(view, titles[dashboard], footers[dashboard]);
    if (dashboard == MD_UI_DASHBOARD_OEM) {
        (void)create_field(view, 10, 39, 174, 112, "ENGINE SPEED", "engine.rpm", true);
        (void)create_field(view, 190, 39, 120, 78, "SPEED", "vehicle.speed", false);
        (void)create_field(view, 190, 121, 120, 86, "COOLANT", "engine.coolant_temperature", false);
        (void)create_field(view, 10, 157, 174, 57, "BOOST", "engine.boost", false);
    } else if (dashboard == MD_UI_DASHBOARD_SPORT) {
        (void)create_field(view, 10, 39, 174, 112, "ENGINE SPEED", "engine.rpm", true);
        (void)create_field(view, 190, 39, 120, 78, "SPEED", "vehicle.speed", false);
        (void)create_field(view, 190, 121, 120, 86, "ENGINE LOAD", "engine.load", false);
        (void)create_field(view, 10, 157, 174, 57, "BATTERY", "vehicle.battery_voltage", false);
    } else if (dashboard == MD_UI_DASHBOARD_MINIMAL) {
        (void)create_field(view, 10, 39, 174, 112, "SPEED", "vehicle.speed", true);
        (void)create_field(view, 190, 39, 120, 78, "ENGINE SPEED", "engine.rpm", false);
        (void)create_field(view, 190, 121, 120, 86, "BATTERY", "vehicle.battery_voltage", false);
        (void)create_field(view, 10, 157, 174, 57, "COOLANT", "engine.coolant_temperature", false);
    } else {
        for (unsigned i = 0; i < settings->widget_count; ++i) {
            const md_ui_widget_binding_t *widget = &settings->widgets[i];
            const int x = widget->column * 320 / MD_UI_GRID_COLUMNS;
            const int y = 39 + widget->row * 168 / MD_UI_GRID_ROWS;
            const int w = widget->columns * 320 / MD_UI_GRID_COLUMNS - 3;
            const int h = widget->rows * 168 / MD_UI_GRID_ROWS - 3;
            const int id = md_ui_signal_id_from_name(widget->canonical_signal);
            const char *label = id >= 0
                ? md_ui_signal_definitions[id].label : widget->canonical_signal;
            (void)create_field(view, x + 2, y, w, h, label,
                widget->canonical_signal, widget->columns >= 6 && widget->rows >= 5);
        }
        if (settings->widget_count == 0) {
            (void)create_diagnostic_field(view, 18, 62, 284, 48,
                "NO WIDGETS CONFIGURED", DIAG_SIGNALS);
        }
    }
}

static void create_performance_screen(screen_view_t *view)
{
    style_root(view, "PERFORMANCE", "READ ONLY  |  VALUE AGE IS CHECKED BEFORE DISPLAY");
    (void)create_field(view, 10, 39, 156, 105, "ENGINE SPEED", "engine.rpm", true);
    (void)create_field(view, 172, 39, 138, 78, "VEHICLE SPEED", "vehicle.speed", false);
    (void)create_field(view, 172, 123, 138, 84, "ENGINE LOAD", "engine.load", false);
    (void)create_field(view, 10, 150, 156, 57, "BOOST", "engine.boost", false);
}

static void create_temperature_screen(screen_view_t *view)
{
    style_root(view, "TEMPERATURES", "MISSING VALUES STAY UNAVAILABLE; STALE VALUES ARE HIDDEN");
    (void)create_field(view, 10, 39, 148, 78, "ENGINE OIL", "engine.oil_temperature", false);
    (void)create_field(view, 162, 39, 148, 78, "COOLANT", "engine.coolant_temperature", false);
    (void)create_field(view, 10, 123, 148, 78, "TRANSMISSION OIL", "transmission.oil_temperature", false);
    (void)create_field(view, 162, 123, 148, 78, "AWD CLUTCH", "awd.clutch_temperature", false);
}

static void create_drivetrain_screen(screen_view_t *view)
{
    style_root(view, "DRIVETRAIN", "VEHICLE AND ECU DEFINITIONS ARE OWNED BY KNOWLEDGE PROVIDERS");
    (void)create_field(view, 10, 39, 138, 128, "GEAR", "transmission.gear", true);
    (void)create_field(view, 154, 39, 156, 78, "TRANSMISSION OIL", "transmission.oil_temperature", false);
    (void)create_field(view, 154, 123, 156, 78, "AWD CLUTCH", "awd.clutch_temperature", false);
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
    style_root(view, "SERVICE STATUS", "N/A MEANS FIRMWARE DOES NOT PROVIDE THAT SOURCE");
    for (unsigned i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i) {
        const int column = (int)(i / UI_DIAG_ROWS);
        const int row = (int)(i % UI_DIAG_ROWS);
        (void)create_diagnostic_field(view, 10 + column * 153, 36 + row * 25,
            147, 23, rows[i].title, rows[i].key);
    }
}

static void create_settings_screen(void)
{
    static const char *const titles[UI_SETTINGS_ROWS] = {
        "Dashboard", "Theme", "Day / night", "Speed units",
        "Temperature", "Brightness", "Warnings"};
    screen_view_t *view = &s_screens[MD_UI_SCREEN_SETTINGS];
    style_root(view, "SETTINGS", "TOUCH: TAP ROW  |  DOUBLE CHANGE  |  HOLD BACK");
    for (unsigned i = 0; i < UI_SETTINGS_ROWS; ++i) {
        setting_row_view_t *row = &s_setting_rows[i];
        row->row = lv_obj_create(view->root);
        if (!row->row) continue;
        lv_obj_remove_style_all(row->row);
        lv_obj_set_pos(row->row, xpx(10), ypx(35 + (int)i * 25));
        lv_obj_set_size(row->row, xpx(300), ypx(23));
        lv_obj_set_style_bg_opa(row->row, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(row->row, xpx(3), 0);
        row->title = lv_label_create(row->row);
        row->value = lv_label_create(row->row);
        if (!row->title || !row->value) continue;
        (void)snprintf(row->title_text, sizeof(row->title_text), "%s", titles[i]);
        row->title_text[sizeof(row->title_text) - 1] = '\0';
        row->value_text[0] = '\0';
        lv_label_set_text_static(row->title, row->title_text);
        lv_label_set_text_static(row->value, row->value_text);
        lv_obj_set_pos(row->title, xpx(8), ypx(3));
        lv_obj_set_size(row->title, xpx(140), ypx(18));
        lv_obj_set_style_text_font(row->title, &lv_font_montserrat_14, 0);
        lv_obj_set_pos(row->value, xpx(154), ypx(3));
        lv_obj_set_size(row->value, xpx(138), ypx(18));
        lv_obj_set_style_text_font(row->value, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_align(row->value, LV_TEXT_ALIGN_RIGHT, 0);
    }
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
    s_width = width;
    s_height = height;
    s_palette = *palette;

    for (int i = 0; i < MD_UI_DASHBOARD_COUNT; ++i) {
        create_dashboard(&s_dashboards[i], (md_ui_dashboard_t)i, settings);
        if (!s_dashboards[i].root) return false;
    }
    create_performance_screen(&s_screens[MD_UI_SCREEN_PERFORMANCE]);
    create_temperature_screen(&s_screens[MD_UI_SCREEN_TEMPERATURES]);
    create_drivetrain_screen(&s_screens[MD_UI_SCREEN_DRIVETRAIN]);
    create_diagnostics_screen(&s_screens[MD_UI_SCREEN_DIAGNOSTICS]);
    create_settings_screen();
    for (int i = 1; i < MD_UI_SCREEN_COUNT; ++i) {
        if (i == MD_UI_SCREEN_CUSTOM) continue;
        if (!s_screens[i].root) return false;
    }
    s_created = true;
    return true;
}

void md_ui_screens_show(md_ui_screen_t screen,
                        const md_ui_settings_t *settings)
{
    if (!s_created) return;
    screen_view_t *view = view_for(screen, settings);
    if (view && view->root) lv_screen_load(view->root);
}

static void retheme_view(screen_view_t *view)
{
    if (!view || !view->root) return;
    lv_obj_set_style_bg_color(view->root, color(s_palette.background), 0);
    lv_obj_set_style_text_color(view->title, color(s_palette.text), 0);
    lv_obj_set_style_text_color(view->mode, color(s_palette.accent), 0);
    lv_obj_set_style_text_color(view->footer, color(s_palette.muted), 0);
    for (unsigned i = 0; i < view->field_count; ++i) {
        field_view_t *field = &view->fields[i];
        lv_obj_set_style_bg_color(field->card, color(s_palette.surface), 0);
        lv_obj_set_style_border_color(field->card, color(s_palette.border), 0);
        lv_obj_set_style_text_color(field->title, color(s_palette.muted), 0);
        lv_obj_set_style_text_color(field->unit, color(s_palette.muted), 0);
        lv_obj_set_style_text_color(field->value, color(s_palette.text), 0);
        lv_obj_set_style_text_color(field->status, color(s_palette.unavailable), 0);
        field->color_set = false;
    }
}

void md_ui_screens_retheme(const md_ui_palette_t *palette)
{
    if (!palette || !s_created) return;
    s_palette = *palette;
    for (unsigned i = 0; i < MD_UI_DASHBOARD_COUNT; ++i) retheme_view(&s_dashboards[i]);
    for (unsigned i = 1; i < MD_UI_SCREEN_COUNT; ++i) {
        if (i == MD_UI_SCREEN_CUSTOM) continue;
        retheme_view(&s_screens[i]);
    }
    for (unsigned i = 0; i < UI_SETTINGS_ROWS; ++i) {
        setting_row_view_t *row = &s_setting_rows[i];
        if (!row->row) continue;
        lv_obj_set_style_bg_color(row->row, color(s_palette.surface), 0);
        lv_obj_set_style_text_color(row->title, color(s_palette.text), 0);
        lv_obj_set_style_text_color(row->value, color(s_palette.accent), 0);
        row->selected = false;
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

    if (!snapshot.value_available) {
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
    set_static_text(field->status, field->status_text, sizeof(field->status_text), state_label);
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
    set_static_text(field->value, field->value_text, sizeof(field->value_text), text);
}

static void settings_value(uint8_t index, const md_ui_settings_t *settings,
                           bool brightness_control, char *output, size_t capacity)
{
    static const char *const dashboard[] = {"OEM", "SPORT", "MINIMAL", "CUSTOM"};
    static const char *const theme[] = {"OEM", "SPORT", "MINIMAL"};
    static const char *const day_night[] = {"AUTO", "DAY", "NIGHT"};
    static const char *const speed[] = {"km/h", "mph"};
    static const char *const temperature[] = {"\xc2\xb0" "C", "\xc2\xb0" "F"};
    switch (index) {
    case 0: (void)snprintf(output, capacity, "%s", dashboard[settings->dashboard]); break;
    case 1: (void)snprintf(output, capacity, "%s", theme[settings->theme]); break;
    case 2: (void)snprintf(output, capacity, "%s", day_night[settings->day_night]); break;
    case 3: (void)snprintf(output, capacity, "%s", speed[settings->units]); break;
    case 4: (void)snprintf(output, capacity, "%s", temperature[settings->temperature_unit]); break;
    case 5:
        (void)snprintf(output, capacity, brightness_control ? "%u%%" : "%u%% FIXED",
                       settings->brightness_pct);
        break;
    case 6: {
        bool any_threshold = false;
        for (unsigned i = 0; i < MD_UI_MAX_WARNING_THRESHOLDS; ++i) {
            any_threshold |= settings->warnings[i].enabled;
        }
        const char *status = !settings->warnings_enabled ? "DISABLED"
            : (any_threshold ? "ENABLED" : "NO THRESHOLDS");
        (void)snprintf(output, capacity, "%s", status);
        break;
    }
    default: (void)snprintf(output, capacity, "N/A"); break;
    }
}

void md_ui_screens_update(md_ui_screen_t screen,
                          const md_ui_settings_t *settings,
                          const md_ui_state_t *state,
                          const md_ui_signal_store_t *signals,
                          uint32_t now_ms,
                          const md_ui_diagnostics_t *diagnostics)
{
    if (!s_created || !settings || !state || !signals) return;
    screen_view_t *view = view_for(screen, settings);
    if (!view || !view->root) return;
    const md_ui_signal_summary_t summary = md_ui_signal_summary(signals, now_ms);
    char mode[12];
    mode_text(&summary, mode, sizeof(mode));
    set_static_text(view->mode, view->mode_text, sizeof(view->mode_text), mode);

    for (unsigned i = 0; i < view->field_count; ++i) {
        field_view_t *field = &view->fields[i];
        if (field->kind == FIELD_DIAGNOSTIC) {
            update_diagnostic_field(field, diagnostics);
        } else {
            update_metric_field(field, settings, signals, now_ms);
        }
    }

    if (screen == MD_UI_SCREEN_SETTINGS) {
        for (unsigned i = 0; i < UI_SETTINGS_ROWS; ++i) {
            setting_row_view_t *row = &s_setting_rows[i];
            if (!row->row) continue;
            char value[32];
            settings_value((uint8_t)i, settings,
                           diagnostics && diagnostics->brightness_control,
                           value, sizeof(value));
            set_static_text(row->value, row->value_text, sizeof(row->value_text), value);
            const bool selected = i == state->selected_setting;
            if (row->selected != selected) {
                row->selected = selected;
                if (selected) {
                    lv_obj_set_style_bg_color(row->row, color(s_palette.surface_alt), 0);
                    lv_obj_set_style_border_color(row->row, color(s_palette.accent), 0);
                    lv_obj_set_style_border_width(row->row, 1, 0);
                } else {
                    lv_obj_set_style_bg_color(row->row, color(s_palette.surface), 0);
                    lv_obj_set_style_border_width(row->row, 0, 0);
                }
            }
        }
    }
}
