#include "ui_theme.h"

#include <stdio.h>

md_ui_palette_t md_ui_theme_palette(md_ui_theme_id_t theme, bool day_mode)
{
    /* Original palettes; retro presets keep a dark phosphor background in DAY. */
    switch(theme) {
    case MD_UI_THEME_TERMINAL:
        return (md_ui_palette_t){.background=0x020b04,.surface=0x061509,.surface_alt=0x0a2310,.border=0x248844,.text=0xaaffbb,.muted=0x65ac77,.accent=0x45ff78,.normal=0x8bffac,.warning=0xffd76a,.critical=0xff6767,.unavailable=0x476d50};
    case MD_UI_THEME_MATRIX:
        return (md_ui_palette_t){.background=0x000500,.surface=0x020d03,.surface_alt=0x051905,.border=0x126c29,.text=0xa2ffb6,.muted=0x49a761,.accent=0x00ff66,.normal=0x47ef83,.warning=0xe4e86b,.critical=0xff6767,.unavailable=0x386747};
    case MD_UI_THEME_FALLOUT:
        return (md_ui_palette_t){.background=0x080c02,.surface=0x101b06,.surface_alt=0x1b2b09,.border=0x658936,.text=0xc3f68a,.muted=0x8aac60,.accent=0xa5f04d,.normal=0xb3e873,.warning=0xffc75c,.critical=0xff7854,.unavailable=0x637348};
    case MD_UI_THEME_AMBER:
        return (md_ui_palette_t){.background=0x100600,.surface=0x201003,.surface_alt=0x301a05,.border=0x99591d,.text=0xffd99a,.muted=0xc69558,.accent=0xffb340,.normal=0xffcb72,.warning=0xffed8c,.critical=0xff6852,.unavailable=0x8d7053};
    case MD_UI_THEME_CYBERPUNK:
        return (md_ui_palette_t){.background=0x0c061a,.surface=0x180f2b,.surface_alt=0x24143d,.border=0x753f8d,.text=0xf4e7ff,.muted=0xb79fcf,.accent=0xff50cc,.normal=0x4ce5ed,.warning=0xffdf63,.critical=0xff677c,.unavailable=0x796789};
    case MD_UI_THEME_ARCTIC:
        if (day_mode) return (md_ui_palette_t){
            .background=0xeaf5ff,.surface=0xffffff,.surface_alt=0xdcedf9,.border=0x9dbad0,
            .text=0x12334d,.muted=0x4b6d85,.accent=0x086fa0,.normal=0x247761,
            .warning=0x955a00,.critical=0xb5263a,.unavailable=0x70899a};
        return (md_ui_palette_t){.background=0x081623,.surface=0x13283b,.surface_alt=0x1e374d,.border=0x3b6080,.text=0xe6f6ff,.muted=0x9dbdd3,.accent=0x70d3ff,.normal=0x93e5d7,.warning=0xffd782,.critical=0xff848e,.unavailable=0x6c859a};
    default: break;
    }
    if (day_mode) {
        switch (theme) {
        case MD_UI_THEME_SPORT:
            return (md_ui_palette_t){
                .background = 0xf1f3f4, .surface = 0xffffff,
                .surface_alt = 0xe5e9eb, .border = 0xb9c2c7,
                .text = 0x121a1e, .muted = 0x56636a, .accent = 0x38464d,
                .normal = 0x267749, .warning = 0x9c5900,
                .critical = 0xb42323, .unavailable = 0x78838a};
        case MD_UI_THEME_MINIMAL:
            return (md_ui_palette_t){
                .background = 0xfafafa, .surface = 0xffffff,
                .surface_alt = 0xf0f0f0, .border = 0xd8d8d8,
                .text = 0x101010, .muted = 0x5c6265, .accent = 0x28604b,
                .normal = 0x267749, .warning = 0x9c5900,
                .critical = 0xb42323, .unavailable = 0x78838a};
        case MD_UI_THEME_OEM:
        default:
            return (md_ui_palette_t){
                .background = 0xf2f5f6, .surface = 0xffffff,
                .surface_alt = 0xe7edf0, .border = 0xc5d0d5,
                .text = 0x15232b, .muted = 0x536771, .accent = 0x0b6074,
                .normal = 0x267749, .warning = 0x9c5900,
                .critical = 0xb42323, .unavailable = 0x78838a};
        }
    }

    switch (theme) {
    case MD_UI_THEME_SPORT:
        return (md_ui_palette_t){
            .background = 0x07090b, .surface = 0x111518,
            .surface_alt = 0x191f23, .border = 0x303a3f,
            .text = 0xf2f4f5, .muted = 0xa2adb2, .accent = 0xc9d0d3,
            .normal = 0x68d391, .warning = 0xf0c351,
            .critical = 0xff6868, .unavailable = 0x788892};
    case MD_UI_THEME_MINIMAL:
        return (md_ui_palette_t){
            .background = 0x000000, .surface = 0x080808,
            .surface_alt = 0x111111, .border = 0x252525,
            .text = 0xffffff, .muted = 0xa0a0a0, .accent = 0xb8dcc8,
            .normal = 0x68d391, .warning = 0xf0c351,
            .critical = 0xff6868, .unavailable = 0x788892};
    case MD_UI_THEME_OEM:
    default:
        return (md_ui_palette_t){
            .background = 0x07121a, .surface = 0x10212b,
            .surface_alt = 0x172b36, .border = 0x29404b,
            .text = 0xf3f6f7, .muted = 0xa5b5be, .accent = 0x62c5d1,
            .normal = 0x68d391, .warning = 0xf0c351,
            .critical = 0xff6868, .unavailable = 0x788892};
    }
}

uint32_t md_ui_theme_signal_color(const md_ui_palette_t *palette,
                                  md_ui_signal_state_t state,
                                  md_ui_severity_t severity)
{
    if (!palette) return 0xffffff;
    switch (state) {
    case MD_UI_SIGNAL_VALID:
        if (severity == MD_UI_SEVERITY_CRITICAL) return palette->critical;
        if (severity == MD_UI_SEVERITY_WARNING) return palette->warning;
        return palette->normal;
    case MD_UI_SIGNAL_STALE: return palette->warning;
    case MD_UI_SIGNAL_ERROR: return palette->critical;
    case MD_UI_SIGNAL_UNAVAILABLE:
    case MD_UI_SIGNAL_UNSUPPORTED:
    default: return palette->unavailable;
    }
}

void md_ui_format_signal_precision(const md_ui_signal_definition_t *definition,
                         const md_ui_signal_snapshot_t *snapshot,
                         const md_ui_settings_t *settings,
                         uint8_t precision, char *value, size_t value_size,
                         char *unit, size_t unit_size)
{
    if (!value || value_size == 0 || !unit || unit_size == 0) return;
    value[0] = '\0';
    unit[0] = '\0';
    if (!definition || !snapshot || !settings || !snapshot->value_available) {
        (void)snprintf(value, value_size, "--");
        return;
    }

    float display_value = snapshot->value;
    const char *display_unit = definition->canonical_unit;
    switch (definition->unit_family) {
    case MD_UI_UNIT_SPEED:
        if (settings->units == MD_UI_UNITS_IMPERIAL) {
            display_value *= 0.6213712f;
            display_unit = "mph";
        }
        break;
    case MD_UI_UNIT_TEMPERATURE:
        if (settings->temperature_unit == MD_UI_TEMP_FAHRENHEIT) {
            display_value = display_value * 1.8f + 32.0f;
            display_unit = "\xc2\xb0" "F";
        } else {
            display_unit = "\xc2\xb0" "C";
        }
        break;
    case MD_UI_UNIT_PRESSURE:
        if (settings->units == MD_UI_UNITS_IMPERIAL) {
            display_value *= 0.0145038f;
            display_unit = "psi";
        }
        break;
    case MD_UI_UNIT_BOOST:
        if (settings->units == MD_UI_UNITS_IMPERIAL) {
            display_value *= 0.0145038f;
            display_unit = "psi";
        } else {
            display_value *= 0.001f;
            display_unit = "bar";
        }
        break;
    case MD_UI_UNIT_NONE:
    case MD_UI_UNIT_PERCENT:
    case MD_UI_UNIT_VOLTAGE:
    case MD_UI_UNIT_GEAR:
    default:
        break;
    }

    (void)snprintf(value, value_size, "%.*f", precision<=3 ? precision : 3, display_value);
    (void)snprintf(unit, unit_size, "%s", display_unit ? display_unit : "");
}

void md_ui_format_signal(const md_ui_signal_definition_t *definition,
    const md_ui_signal_snapshot_t *snapshot, const md_ui_settings_t *settings,
    char *value, size_t value_size, char *unit, size_t unit_size)
{
    md_ui_format_signal_precision(definition,snapshot,settings,definition ? definition->decimals : 0,
        value,value_size,unit,unit_size);
}

const char *md_ui_signal_state_label(md_ui_signal_state_t state)
{
    switch (state) {
    case MD_UI_SIGNAL_VALID: return "LIVE";
    case MD_UI_SIGNAL_STALE: return "STALE";
    case MD_UI_SIGNAL_UNAVAILABLE: return "NO DATA";
    case MD_UI_SIGNAL_UNSUPPORTED: return "UNSUPPORTED";
    case MD_UI_SIGNAL_ERROR: return "ERROR";
    default: return "ERROR";
    }
}

const char *md_ui_theme_name(md_ui_theme_id_t theme)
{
    static const char *const names[MD_UI_THEME_COUNT] = {
        "OEM", "Sport", "Minimal", "Terminal", "Matrix", "Fallout", "Amber CRT", "Cyberpunk", "Arctic"
    };
    return theme >= 0 && theme < MD_UI_THEME_COUNT ? names[theme] : names[0];
}
md_ui_theme_style_t md_ui_theme_style(md_ui_theme_id_t theme)
{
    switch(theme) {
    case MD_UI_THEME_TERMINAL: return (md_ui_theme_style_t){0,1,0,0,0,1};
    case MD_UI_THEME_MATRIX: return (md_ui_theme_style_t){0,1,4,35,24,1};
    case MD_UI_THEME_FALLOUT: return (md_ui_theme_style_t){2,2,2,24,14,1};
    case MD_UI_THEME_AMBER: return (md_ui_theme_style_t){2,1,4,35,20,1};
    case MD_UI_THEME_CYBERPUNK: return (md_ui_theme_style_t){0,2,5,48,0,0};
    case MD_UI_THEME_ARCTIC: return (md_ui_theme_style_t){10,1,0,0,0,0};
    default: return (md_ui_theme_style_t){6,1,0,0,0,0};
    }
}
