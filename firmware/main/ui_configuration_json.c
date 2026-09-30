#include "ui_web.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>

static const char *const color_names[] = {"background", "surface", "surface_alt", "border", "text", "muted", "accent", "normal", "warning", "critical", "unavailable"};
static void number(cJSON *o, const char *name, double value) { cJSON_AddNumberToObject(o, name, value); }
static bool integer(const cJSON *o, const char *name, unsigned maximum, unsigned *out)
{
    const cJSON *n = cJSON_GetObjectItemCaseSensitive(o, name);
    if (!cJSON_IsNumber(n) || !isfinite(n->valuedouble) || n->valuedouble < 0 ||
        n->valuedouble > maximum || floor(n->valuedouble) != n->valuedouble) return false;
    *out = (unsigned)n->valuedouble; return true;
}
static bool boolean(const cJSON *o, const char *name, bool *out)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, name);
    if (!cJSON_IsBool(v)) return false;
    *out = cJSON_IsTrue(v); return true;
}
static bool text(const cJSON *o, const char *name, char *out, size_t size)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, name);
    if (!cJSON_IsString(v) || strlen(v->valuestring) >= size) return false;
    snprintf(out, size, "%s", v->valuestring); return true;
}
static bool real(const cJSON *o, const char *name, float *out)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, name);
    if (!cJSON_IsNumber(v) || !isfinite(v->valuedouble) || fabs(v->valuedouble) > 1000000) return false;
    *out = (float)v->valuedouble; return true;
}
cJSON *md_configuration_json(const md_device_configuration_t *c, uint32_t revision)
{
    if (!md_configuration_validate(c, NULL)) return NULL;
    cJSON *root = cJSON_CreateObject(); if (!root) return NULL;
    number(root, "schema", c->schema_version); number(root, "revision", revision);
    const md_common_configuration_t *s = &c->common;
    cJSON *common = cJSON_AddObjectToObject(root, "common");
    if (!common) { cJSON_Delete(root); return NULL; }
    number(common, "dashboard", s->dashboard); number(common, "theme", s->theme);
    number(common, "day_night", s->day_night); number(common, "units", s->units);
    number(common, "temperature_unit", s->temperature_unit); number(common, "language", s->language);
    number(common, "last_screen", s->last_screen); number(common, "active_page", s->active_page);
    cJSON_AddBoolToObject(common, "warnings_enabled", s->warnings_enabled);
    cJSON_AddBoolToObject(common, "custom_theme", s->custom_theme);
    uint32_t colors[11]; memcpy(colors, &s->palette, sizeof(colors));
    cJSON *palette = cJSON_AddObjectToObject(common, "palette");
    for (unsigned i = 0; i < 11; ++i) number(palette, color_names[i], colors[i]);
    cJSON *warnings = cJSON_AddArrayToObject(common, "warnings");
    for (unsigned i = 0; i < MD_UI_MAX_WARNING_THRESHOLDS; ++i) {
        const md_ui_warning_threshold_t *t = &s->warnings[i]; cJSON *o = cJSON_CreateObject();
        number(o, "signal", t->signal_id); cJSON_AddBoolToObject(o, "enabled", t->enabled);
        number(o, "direction", t->direction); number(o, "warning_milli", t->warning_milli);
        number(o, "critical_milli", t->critical_milli); cJSON_AddItemToArray(warnings, o);
    }
    cJSON *fixed = cJSON_AddArrayToObject(common, "fixed_card_overrides");
    for (unsigned i = 0; i < MD_UI_FIXED_CARD_OVERRIDES_COUNT; ++i)
        cJSON_AddItemToArray(fixed, cJSON_CreateNumber(s->fixed_card_overrides[i]));
    cJSON *pages = cJSON_AddArrayToObject(common, "pages");
    for (unsigned p = 0; p < s->page_count; ++p) {
        const md_ui_dashboard_page_t *page = &s->pages[p]; cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "title", page->title); number(o, "layout", page->layout);
        cJSON *widgets = cJSON_AddArrayToObject(o, "widgets");
        for (unsigned i = 0; i < page->widget_count; ++i) {
            const md_ui_page_widget_t *w = &page->widgets[i]; cJSON *v = cJSON_CreateObject();
            cJSON_AddStringToObject(v, "signal", md_ui_signal_definitions[w->signal].canonical_name);
            number(v, "type", w->type); number(v, "precision", w->precision);
            number(v, "column", w->column); number(v, "row", w->row);
            number(v, "columns", w->columns); number(v, "rows", w->rows);
            number(v, "minimum", w->minimum); number(v, "maximum", w->maximum);
            cJSON_AddStringToObject(v, "label", w->label); cJSON_AddItemToArray(widgets, v);
        }
        cJSON_AddItemToArray(pages, o);
    }
    if (c->platform.present) {
        cJSON *platform = cJSON_AddObjectToObject(root, "platform");
        number(platform, "present", c->platform.present); number(platform, "brightness", c->platform.brightness);
        number(platform, "brightness_mode", c->platform.brightness_mode); number(platform, "rotation", c->platform.rotation);
    }
    return root;
}
/* Exact shapes prevent duplicate names, ignored fields and diagnostic commands. */
static bool shape(const cJSON *o, const char *names)
{
    if (!cJSON_IsObject(o)) return false;
    for (const cJSON *item = o->child; item; item = item->next) {
        if (!item->string || strlen(item->string) > 48) return false;
        char key[52]; snprintf(key, sizeof(key), "|%s|", item->string);
        if (!strstr(names, key)) return false;
        for (const cJSON *next = item->next; next; next = next->next)
            if (next->string && strcmp(item->string, next->string) == 0) return false;
    }
    return true;
}
bool md_configuration_parse_json(const cJSON *root, md_device_configuration_t *c, uint32_t *revision)
{
    if (!c || !revision || !shape(root, "|schema||revision||common||platform|")) return false;
    md_device_configuration_t candidate = {0}; unsigned n;
    if (!integer(root, "schema", MD_CONFIG_SCHEMA_VERSION, &n) || (n != 1 && n != MD_CONFIG_SCHEMA_VERSION)) return false;
    const bool legacy_rotation = n == 1;
    candidate.schema_version = MD_CONFIG_SCHEMA_VERSION;
    if (!integer(root, "revision", UINT32_MAX, &n)) return false;
    const uint32_t rev = n;
    md_common_configuration_t *s = &candidate.common;
    const cJSON *common = cJSON_GetObjectItemCaseSensitive(root, "common");
    if (!shape(common, "|dashboard||theme||day_night||units||temperature_unit||language||last_screen||active_page||warnings_enabled||custom_theme||palette||warnings||fixed_card_overrides||pages|")) return false;
#define BYTE(O,K,F) do { if (!integer((O),(K),255,&n)) return false; (F) = n; } while (0)
    BYTE(common,"dashboard",s->dashboard); BYTE(common,"theme",s->theme);
    BYTE(common,"day_night",s->day_night); BYTE(common,"units",s->units);
    BYTE(common,"temperature_unit",s->temperature_unit); BYTE(common,"language",s->language);
    BYTE(common,"last_screen",s->last_screen); BYTE(common,"active_page",s->active_page);
    if (!boolean(common,"warnings_enabled",&s->warnings_enabled) || !boolean(common,"custom_theme",&s->custom_theme)) return false;
    const cJSON *palette = cJSON_GetObjectItemCaseSensitive(common,"palette");
    if (!shape(palette,"|background||surface||surface_alt||border||text||muted||accent||normal||warning||critical||unavailable|")) return false;
    uint32_t colors[11];
    for (unsigned i = 0; i < 11; ++i) { if (!integer(palette,color_names[i],0xffffff,&n)) return false; colors[i] = n; }
    memcpy(&s->palette, colors, sizeof(colors));
    const cJSON *warnings = cJSON_GetObjectItemCaseSensitive(common,"warnings");
    if (!cJSON_IsArray(warnings) || cJSON_GetArraySize(warnings) != MD_UI_MAX_WARNING_THRESHOLDS) return false;
    for (unsigned i = 0; i < MD_UI_MAX_WARNING_THRESHOLDS; ++i) {
        const cJSON *o = cJSON_GetArrayItem(warnings,i); md_ui_warning_threshold_t *t = &s->warnings[i];
        if (!shape(o,"|signal||enabled||direction||warning_milli||critical_milli|")) return false;
        BYTE(o,"signal",t->signal_id); BYTE(o,"direction",t->direction);
        if (!boolean(o,"enabled",&t->enabled)) return false;
        const cJSON *a = cJSON_GetObjectItemCaseSensitive(o,"warning_milli"), *b = cJSON_GetObjectItemCaseSensitive(o,"critical_milli");
        if (!cJSON_IsNumber(a) || !cJSON_IsNumber(b) || !isfinite(a->valuedouble) || !isfinite(b->valuedouble) ||
            a->valuedouble < INT32_MIN || a->valuedouble > INT32_MAX || b->valuedouble < INT32_MIN || b->valuedouble > INT32_MAX ||
            floor(a->valuedouble) != a->valuedouble || floor(b->valuedouble) != b->valuedouble) return false;
        t->warning_milli = (int32_t)a->valuedouble; t->critical_milli = (int32_t)b->valuedouble;
    }
    const cJSON *fixed = cJSON_GetObjectItemCaseSensitive(common,"fixed_card_overrides");
    if (!cJSON_IsArray(fixed) || cJSON_GetArraySize(fixed) != MD_UI_FIXED_CARD_OVERRIDES_COUNT) return false;
    for (unsigned i = 0; i < MD_UI_FIXED_CARD_OVERRIDES_COUNT; ++i) {
        const cJSON *v = cJSON_GetArrayItem(fixed,i);
        if (!cJSON_IsNumber(v) || !isfinite(v->valuedouble) || v->valuedouble < 0 || v->valuedouble > 255 || floor(v->valuedouble) != v->valuedouble) return false;
        s->fixed_card_overrides[i] = (uint8_t)v->valuedouble;
    }
    const cJSON *pages = cJSON_GetObjectItemCaseSensitive(common,"pages");
    if (!cJSON_IsArray(pages) || cJSON_GetArraySize(pages) < 1 || cJSON_GetArraySize(pages) > MD_CONFIG_MAX_PAGES) return false;
    s->page_count = cJSON_GetArraySize(pages);
    for (unsigned p = 0; p < s->page_count; ++p) {
        const cJSON *o = cJSON_GetArrayItem(pages,p); md_ui_dashboard_page_t *page = &s->pages[p];
        if (!shape(o,"|title||layout||widgets|") || !text(o,"title",page->title,sizeof(page->title))) return false;
        BYTE(o,"layout",page->layout);
        const cJSON *widgets = cJSON_GetObjectItemCaseSensitive(o,"widgets");
        if (!cJSON_IsArray(widgets) || cJSON_GetArraySize(widgets) < 1 || cJSON_GetArraySize(widgets) > MD_UI_MAX_WIDGETS) return false;
        page->widget_count = cJSON_GetArraySize(widgets);
        for (unsigned i = 0; i < page->widget_count; ++i) {
            const cJSON *v = cJSON_GetArrayItem(widgets,i); md_ui_page_widget_t *w = &page->widgets[i]; char signal[MD_UI_SIGNAL_NAME_LEN];
            if (!shape(v,"|signal||type||precision||column||row||columns||rows||minimum||maximum||label|") || !text(v,"signal",signal,sizeof(signal))) return false;
            const int id = md_ui_signal_id_from_name(signal); if (id < 0) return false; w->signal = id;
            BYTE(v,"type",w->type); BYTE(v,"precision",w->precision); BYTE(v,"column",w->column);
            BYTE(v,"row",w->row); BYTE(v,"columns",w->columns); BYTE(v,"rows",w->rows);
            if (!real(v,"minimum",&w->minimum) || !real(v,"maximum",&w->maximum) || !text(v,"label",w->label,sizeof(w->label))) return false;
        }
    }
    const cJSON *platform = cJSON_GetObjectItemCaseSensitive(root,"platform");
    if (platform) {
        if (!shape(platform,"|present||brightness||brightness_mode||rotation|") || !integer(platform,"present",UINT32_MAX,&n)) return false;
        candidate.platform.present = n;
        BYTE(platform,"brightness",candidate.platform.brightness);
        BYTE(platform,"brightness_mode",candidate.platform.brightness_mode); BYTE(platform,"rotation",candidate.platform.rotation);
        if (candidate.platform.rotation > 3) return false;
        if (legacy_rotation) candidate.platform.rotation = 0;
    }
#undef BYTE
    if (!md_configuration_validate(&candidate,NULL)) return false;
    *c = candidate; *revision = rev; return true;
}

static bool test_json_inner(md_device_configuration_t *first, md_device_configuration_t *second)
{
#define a (*first)
#define b (*second)
    md_ui_settings_t settings; md_ui_settings_defaults(&settings);
    if (!md_configuration_from_legacy(&a,&settings,NULL)) return false;
    cJSON *root=md_configuration_json(&a,17); uint32_t revision;
    bool ok=root && md_configuration_parse_json(root,&b,&revision) && revision==17;
    if (ok) {
        cJSON_AddNumberToObject(root,"revision",18);
        ok=!md_configuration_parse_json(root,&b,&revision);
    }
    cJSON_Delete(root);
    if (!ok) return false;
    root=md_configuration_json(&a,17); cJSON_AddStringToObject(root,"uds","22f190");
    ok=!md_configuration_parse_json(root,&b,&revision); cJSON_Delete(root);
    if (!ok) return false;
    root=md_configuration_json(&a,17);
    cJSON *w=cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(root,"common"),"pages"),0),"widgets"),0);
    cJSON_ReplaceItemInObjectCaseSensitive(w,"signal",cJSON_CreateString("unverified.signal"));
    ok=!md_configuration_parse_json(root,&b,&revision); cJSON_Delete(root); return ok;
}

#undef a
#undef b
bool md_ui_test_configuration_json(void)
{
    md_device_configuration_t *first=malloc(sizeof(*first)), *second=malloc(sizeof(*second));
    bool ok=first && second && test_json_inner(first,second);
    free(first); free(second); return ok;
}

void md_ui_theme_json_metadata(cJSON *preset, md_ui_theme_id_t theme)
{
    if(!preset) return;
    /* Presets carry palette/style, not eleven copies of user pages/settings. */
    cJSON *common=cJSON_GetObjectItemCaseSensitive(preset,"common");
    cJSON *palette=common ? cJSON_DetachItemFromObjectCaseSensitive(common,"palette") : NULL;
    cJSON_DeleteItemFromObjectCaseSensitive(preset,"common");
    common=cJSON_AddObjectToObject(preset,"common");
    if(palette && common) cJSON_AddItemToObject(common,"palette",palette);
    else cJSON_Delete(palette);
    cJSON_DeleteItemFromObjectCaseSensitive(preset,"platform");
    cJSON_DeleteItemFromObjectCaseSensitive(preset,"schema");
    cJSON_DeleteItemFromObjectCaseSensitive(preset,"revision");
    cJSON_AddStringToObject(preset,"name",md_ui_theme_name(theme));
    cJSON *style=cJSON_AddObjectToObject(preset,"style");
    if(!style) return;
    const md_ui_theme_style_t traits=md_ui_theme_style(theme);
    number(style,"radius",traits.radius); number(style,"border_width",traits.border_width);
    number(style,"glow_width",traits.glow_width); number(style,"glow_opacity",traits.glow_opacity);
    number(style,"scanline_opacity",traits.scanline_opacity);
    number(style,"font_family",traits.font_family); number(style,"frame_kind",traits.frame_kind);
    number(style,"gauge_style",traits.gauge_style);
}
