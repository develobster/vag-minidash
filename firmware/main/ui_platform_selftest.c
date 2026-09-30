#include "ui_startup.h"
#include "backlight_controller.h"
#include "ui_platform_selftest.h"
#include "ui_configuration.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

static md_device_configuration_t a,b;
static uint8_t wire[MD_CONFIG_WIRE_MAX];
static bool defaults(void)
{
    md_ui_settings_t s; md_ui_settings_defaults(&s);
    return md_configuration_from_legacy(&a,&s,NULL);
}
bool md_ui_test_configuration(void)
{
    if (!defaults()) return false;
    a.common.page_count=4;
    for (unsigned p=1;p<4;++p) { a.common.pages[p]=a.common.pages[0]; if (!md_configuration_set_layout(&a.common.pages[p],MD_UI_LAYOUT_EIGHT)) return false; }
    a.common.pages[1].widgets[0].type=MD_UI_WIDGET_BAR;
    a.common.pages[2].widgets[0].type=MD_UI_WIDGET_STATUS;
    a.common.pages[3].widgets[0].type=MD_UI_WIDGET_GRAPH;
    a.common.active_page=3;
    if (!md_configuration_validate(&a,NULL)) return false;
    md_ui_settings_t legacy;
    if (!md_configuration_to_legacy(&a,&legacy) || legacy.widgets[0].type!=MD_UI_WIDGET_GRAPH) return false;
    legacy.language=MD_UI_LANGUAGE_RUSSIAN;
    if (!md_configuration_update_legacy(&a,&legacy) || a.common.pages[1].widgets[0].type!=MD_UI_WIDGET_BAR) return false;
    if (!md_configuration_reset_page(&a,0) || !md_configuration_validate(&a,NULL)) return false;
    size_t n=0;
    return md_configuration_encode(&a,wire,sizeof(wire),&n) && n<=MD_CONFIG_WIRE_MAX &&
        md_configuration_decode(wire,n,&b) && b.common.page_count==4 && b.common.language==MD_UI_LANGUAGE_RUSSIAN &&
        b.common.pages[3].widgets[0].type==MD_UI_WIDGET_GRAPH;
}
bool md_ui_test_widget_precision(void)
{
    md_ui_settings_t settings; md_ui_settings_defaults(&settings);
    md_ui_signal_snapshot_t sample={.state=MD_UI_SIGNAL_VALID,.value_available=true,.value=500};
    char value[32],unit[12];
    md_ui_format_signal_precision(md_ui_signal_definition(MD_SIGNAL_ENGINE_BOOST),&sample,&settings,2,value,sizeof(value),unit,sizeof(unit));
    if (strcmp(value,"0.50") || strcmp(unit,"bar")) return false;
    settings.units=MD_UI_UNITS_IMPERIAL;
    md_ui_format_signal_precision(md_ui_signal_definition(MD_SIGNAL_ENGINE_BOOST),&sample,&settings,2,value,sizeof(value),unit,sizeof(unit));
    if (strcmp(value,"7.25") || strcmp(unit,"psi")) return false;
    sample.value_available=false;
    md_ui_format_signal_precision(md_ui_signal_definition(MD_SIGNAL_ENGINE_BOOST),&sample,&settings,2,value,sizeof(value),unit,sizeof(unit));
    return !strcmp(value,"--") && !unit[0];
}
bool md_ui_test_configuration_validation(void)
{
    if (!defaults()) return false;
    b=a; b.common.pages[0].widgets[0].maximum=NAN; if (md_configuration_validate(&b,NULL)) return false;
    b=a; b.common.pages[0].widgets[0].type=MD_UI_WIDGET_COUNT; if (md_configuration_validate(&b,NULL)) return false;
    b=a; b.common.pages[0].widgets[0].column=MD_UI_GRID_COLUMNS; if (md_configuration_validate(&b,NULL)) return false;
    b=a; b.common.pages[0].widgets[0].precision=4; if (md_configuration_validate(&b,NULL)) return false;
    b=a; b.common.page_count=5; if (md_configuration_validate(&b,NULL)) return false;
    b=a; memset(b.common.pages[0].title,'A',MD_CONFIG_TEXT_SIZE); if (md_configuration_validate(&b,NULL)) return false;
    b=a; strcpy(b.common.pages[0].title,"\xc0\x80"); if (md_configuration_validate(&b,NULL)) return false;
    b=a; strcpy(b.common.pages[0].title,"\xd0\xa2\xd0\xb5\xd1\x81\xd1\x82");
    return md_configuration_validate(&b,NULL);
}
bool md_ui_test_configuration_crc(void)
{
    if (!defaults()) return false;
    size_t n;
    if (!md_configuration_encode(&a,wire,sizeof(wire),&n)) return false;
    b=a; wire[n/2]^=0x40;
    if (md_configuration_decode(wire,n,&b) || memcmp(&a,&b,sizeof(a))) return false;
    wire[n/2]^=0x40;
    for (size_t truncated=0;truncated<n;++truncated) if (md_configuration_decode(wire,truncated,&b)) return false;
    return !md_configuration_encode(&a,wire,16,&n);
}
bool md_ui_test_history(void)
{
    md_ui_history_t h={0}; md_ui_signal_record_t r={.state=MD_UI_SIGNAL_VALID,.received=true,.value=50,.updated_ms=100,.ttl_ms=100};
    md_ui_history_observe(&h,&r,100); md_ui_history_observe(&h,&r,110);
    if (h.count!=1 || h.minimum!=50) return false;
    r.value=70; r.updated_ms=120; md_ui_history_observe(&h,&r,120);
    if (h.maximum!=70 || h.count!=2) return false;
    md_ui_history_observe(&h,&r,220); if (h.valid) return false;
    r.value=60; r.updated_ms=230; md_ui_history_observe(&h,&r,230);
    if (h.count!=1 || h.maximum!=60) return false;
    r.demo=true; r.value=10; r.updated_ms=240; md_ui_history_observe(&h,&r,240);
    if (h.count!=1 || h.minimum!=10 || !h.demo) return false;
    uint8_t first,count;
    return md_ui_presentation_slice(6,3,1,&first,&count) && first==3 && count==3 &&
        !md_ui_presentation_slice(6,3,2,&first,&count);
}
bool md_ui_test_buttons(void)
{
    md_ui_button_t b; md_ui_action_t action;
    md_ui_button_init(&b,MD_UI_ACTION_SELECT,MD_UI_ACTION_HOME);
    if (md_ui_button_sample(&b,true,10,false,&action) || md_ui_button_sample(&b,true,40,false,&action) ||
        md_ui_button_sample(&b,false,60,false,&action) || !md_ui_button_sample(&b,false,90,false,&action) || action!=MD_UI_ACTION_SELECT) return false;
    md_ui_button_sample(&b,true,100,false,&action); md_ui_button_sample(&b,true,130,false,&action);
    if (!md_ui_button_sample(&b,true,930,false,&action) || action!=MD_UI_ACTION_HOME ||
        md_ui_button_sample(&b,true,1300,false,&action)) return false;
    md_ui_button_sample(&b,false,1400,false,&action);
    if (md_ui_button_sample(&b,false,1430,false,&action)) return false;
    md_ui_button_sample(&b,true,1500,true,&action);
    if (md_ui_button_sample(&b,true,1600,false,&action) || md_ui_button_sample(&b,true,2600,false,&action)) return false;
    md_ui_button_sample(&b,false,2700,false,&action);
    if (md_ui_button_sample(&b,false,2730,false,&action)) return false;
    md_ui_button_init(&b,MD_UI_ACTION_NEXT_ITEM,MD_UI_ACTION_NEXT_ITEM);
    md_ui_button_sample(&b,true,0,false,&action); md_ui_button_sample(&b,true,30,false,&action);
    if (!md_ui_button_sample(&b,true,830,false,&action) || !md_ui_button_sample(&b,true,1010,false,&action)) return false;
    return !md_ui_vehicle_button_action(false,MD_UI_ACTION_HOME,&action) &&
        md_ui_vehicle_button_action(true,MD_UI_ACTION_HOME,&action);
}
bool md_ui_test_brightness(void)
{
    if (md_backlight_duty(0) != 0 || md_backlight_duty(100) != 4095 ||
        md_backlight_duty(50) != 1024 || md_backlight_duty(25) != 256 ||
        md_backlight_duty(10) != 41 || md_backlight_duty(75) != 2303) return false;
    for (unsigned p=1; p<=100; ++p)
        if (md_backlight_duty(p) <= md_backlight_duty(p-1)) return false;
    const uint8_t levels[] = {100,75,50,25,10,0};
    for (unsigned i=0; i<sizeof(levels); ++i) {
        md_ui_settings_t original, restored;
        uint8_t record[MD_UI_SETTINGS_WIRE_SIZE];
        size_t written;
        md_ui_settings_defaults(&original);
        if (original.brightness_pct != 70) return false;
        original.brightness_pct = levels[i];
        if (!md_ui_settings_encode(&original, record, sizeof(record), &written) ||
            !md_ui_settings_decode(record, written, &restored) ||
            restored.brightness_pct != levels[i]) return false;
    }
    md_target_profile_t target={0}; md_brightness_t b; uint8_t value;
    md_brightness_init(&b,&target,80);
    if (b.mode!=MD_BRIGHTNESS_FIXED || md_brightness_set(&b,20) || md_brightness_mode(&b,MD_BRIGHTNESS_AUTO)) return false;
    target.hardware=MD_CAP_DIMMING; md_brightness_init(&b,&target,80);
    md_ui_settings_t settings; md_ui_state_t state;
    md_ui_settings_defaults(&settings);
    md_ui_state_init(&state, &settings);
    state.screen = MD_UI_SCREEN_SETTINGS; state.selected_setting = 5;
    settings.brightness_pct = 10;
    if (!md_ui_state_dispatch(&state, &settings, MD_UI_ACTION_DECREMENT) ||
        settings.brightness_pct != 100 ||
        !md_ui_state_dispatch(&state, &settings, MD_UI_ACTION_INCREMENT) ||
        settings.brightness_pct != 10) return false;

    if (!md_brightness_set(&b,20) || md_brightness_mode(&b,MD_BRIGHTNESS_AUTO) ||
        !md_brightness_tick(&b,20,&value) || value!=79 || md_brightness_tick(&b,25,&value)) return false;
    if (md_brightness_set(&b,1) || md_brightness_set(&b,0)) return false;
    b.screen_on=false;
    return md_brightness_tick(&b,40,&value) && value==78 && !md_brightness_set(&b,101);
}
bool md_ui_test_target_capabilities(void)
{
    md_target_profile_t target=md_target_lite_proposal();
    if (target.verified || target.display_width || md_target_feature_available(&target,MD_FEATURE_TELEMETRY)) return false;
    target.implemented_features=(1U<<MD_FEATURE_WEB_CONFIG)|(1U<<MD_FEATURE_DTC_READ);
    return !md_target_feature_available(&target,MD_FEATURE_WEB_CONFIG) && !md_target_feature_available(&target,MD_FEATURE_DTC_READ);
}

bool md_ui_test_themes_and_startup(void)
{
    md_ui_settings_t original,restored; uint8_t record[MD_UI_SETTINGS_WIRE_SIZE]; size_t written;
    md_ui_settings_defaults(&original);
    if(original.brightness_pct!=70 || MD_UI_THEME_OEM!=0 || MD_UI_THEME_SPORT!=1 || MD_UI_THEME_MINIMAL!=2) return false;
    for(int theme=0;theme<MD_UI_THEME_COUNT;++theme) {
        original.theme=theme;
        if(!md_ui_settings_encode(&original,record,sizeof(record),&written) ||
           !md_ui_settings_decode(record,written,&restored) || restored.theme!=theme ||
           !md_ui_theme_name(theme)[0]) return false;
        const md_ui_palette_t night=md_ui_theme_palette(theme,false), day=md_ui_theme_palette(theme,true);
        if(night.text==night.background || day.text==day.background || night.warning==night.critical) return false;
        if(!md_configuration_from_legacy(&a,&original,NULL)) return false;
        if(!md_configuration_encode(&a,wire,sizeof(wire),&written) ||
           !md_configuration_decode(wire,written,&b) || b.common.theme!=theme) return false;
        for(int other=0;other<theme;++other) {
            const md_ui_palette_t previous=md_ui_theme_palette(other,false);
            if(!memcmp(&night,&previous,sizeof(night))) return false;
        }
    }
    for(unsigned target=10;target<=100;target+=10) {
        md_ui_startup_frame_t last=md_ui_startup_frame(0,320,240,target);
        if(last.brightness!=0 || last.width!=4 || last.height!=2 || last.complete) return false;
        for(unsigned ms=1;ms<=MD_UI_STARTUP_DURATION_MS;++ms) {
            const md_ui_startup_frame_t f=md_ui_startup_frame(ms,320,240,target);
            if(f.brightness<last.brightness || f.brightness>target || f.width<last.width ||
               f.height<last.height || f.width>320 || f.height>240) return false;
            last=f;
        }
        if(!last.complete || last.brightness!=target || last.width!=320 || last.height!=240 || last.glow_opacity) return false;
    }
    return md_ui_startup_frame(UINT32_MAX,320,240,70).brightness==70;
}
