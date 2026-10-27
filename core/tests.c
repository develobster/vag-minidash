#include "ui_platform_selftest.h"
#include "ui_configuration.h"
#include "ui_i18n.h"
#include <stdio.h>
#include <string.h>
static bool test_russian_signal_labels(void)
{
    for (int id=0; id<MD_SIGNAL_COUNT; ++id) {
        const char *full=md_ui_i18n_signal_label(MD_UI_LANGUAGE_RUSSIAN,id);
        const char *compact=md_ui_i18n_compact_signal_label(MD_UI_LANGUAGE_RUSSIAN,id,NULL);
        if (!full || !compact || !*full || !*compact || strlen(full)>=64 ||
            strlen(compact)>=32 || strcmp(full,md_ui_signal_definitions[id].label)==0)
            return false;
        if (id>=MD_SIGNAL_ENGINE_MANIFOLD_PRESSURE &&
            strcmp(md_ui_i18n_signal_label(MD_UI_LANGUAGE_ENGLISH,id),
                md_ui_signal_definitions[id].label)!=0) return false;
    }
    return strcmp(md_ui_i18n_signal_label(MD_UI_LANGUAGE_RUSSIAN,MD_SIGNAL_COUNT),"СИГНАЛ")==0 &&
        strcmp(md_ui_i18n_compact_signal_label(MD_UI_LANGUAGE_RUSSIAN,-1,"custom"),"custom")==0;
}
static bool test_automotive_navigation_and_persistence(void)
{
    md_ui_settings_t settings,decoded;md_ui_settings_defaults(&settings);
    md_ui_state_t state;md_ui_state_init(&state,&settings);
    const md_ui_screen_t cycle[]={MD_UI_SCREEN_PERFORMANCE,MD_UI_SCREEN_TEMPERATURES,
        MD_UI_SCREEN_DRIVETRAIN,MD_UI_SCREEN_DIAGNOSTICS,MD_UI_SCREEN_CUSTOM,
        MD_UI_SCREEN_SPORT_BOOST,MD_UI_SCREEN_THERMALS_HEALTH,MD_UI_SCREEN_DPF_DIESEL,
        MD_UI_SCREEN_DRAG,MD_UI_SCREEN_NIGHT_MINIMAL,MD_UI_SCREEN_DASHBOARD};
    for(unsigned i=0;i<sizeof(cycle)/sizeof(cycle[0]);i++) {
        if(!md_ui_state_dispatch(&state,&settings,MD_UI_ACTION_NEXT_SCREEN) || state.screen!=cycle[i]) return false;
    }
    if(!md_ui_state_dispatch(&state,&settings,MD_UI_ACTION_PREVIOUS_SCREEN) || state.screen!=MD_UI_SCREEN_NIGHT_MINIMAL) return false;
    uint8_t wire[MD_UI_SETTINGS_WIRE_SIZE];size_t written;
    for(unsigned i=MD_UI_SCREEN_SPORT_BOOST;i<=MD_UI_SCREEN_NIGHT_MINIMAL;i++) {
        settings.last_screen=i;
        if(!md_ui_settings_encode(&settings,wire,sizeof(wire),&written) || !md_ui_settings_decode(wire,written,&decoded)) return false;
        md_ui_state_init(&state,&decoded);if(state.screen!=i) return false;
        if(!md_ui_state_dispatch(&state,&settings,MD_UI_ACTION_OPEN_SETTINGS) ||
           !md_ui_state_dispatch(&state,&settings,MD_UI_ACTION_BACK) || state.screen!=i) return false;
    }
    return true;
}
int main(void)
{
    unsigned failed=0;
#define TEST(F) do { bool passed=(F)(); printf("%s: %s\n",#F,passed?"PASS":"FAIL"); if (!passed) ++failed; } while(0)
    TEST(md_ui_test_rotation); TEST(md_ui_test_widget_precision); TEST(md_ui_test_configuration); TEST(md_ui_test_configuration_validation);
    TEST(md_ui_test_configuration_crc); TEST(md_ui_test_history);
    TEST(md_ui_test_buttons); TEST(md_ui_test_brightness); TEST(md_ui_test_target_capabilities); TEST(md_ui_test_themes_and_startup);
    TEST(test_russian_signal_labels); TEST(test_automotive_navigation_and_persistence);
    printf("SIZES config=%u page=%u widget=%u store=%u history=%u settings=%u button=%u\n",
        (unsigned)sizeof(md_device_configuration_t),(unsigned)sizeof(md_ui_dashboard_page_t),
        (unsigned)sizeof(md_ui_page_widget_t),(unsigned)sizeof(md_ui_signal_store_t),
        (unsigned)sizeof(md_ui_history_t),(unsigned)sizeof(md_ui_settings_t),(unsigned)sizeof(md_ui_button_t));
    printf("MINIDASH_PORTABLE_TEST_SUMMARY pass=%u fail=%u\n",11-failed,failed);
    return failed ? 1 : 0;
}
