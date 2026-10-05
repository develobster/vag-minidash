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
int main(void)
{
    unsigned failed=0;
#define TEST(F) do { bool passed=(F)(); printf("%s: %s\n",#F,passed?"PASS":"FAIL"); if (!passed) ++failed; } while(0)
    TEST(md_ui_test_rotation); TEST(md_ui_test_widget_precision); TEST(md_ui_test_configuration); TEST(md_ui_test_configuration_validation);
    TEST(md_ui_test_configuration_crc); TEST(md_ui_test_history);
    TEST(md_ui_test_buttons); TEST(md_ui_test_brightness); TEST(md_ui_test_target_capabilities); TEST(md_ui_test_themes_and_startup);
    TEST(test_russian_signal_labels);
    printf("SIZES config=%u page=%u widget=%u store=%u history=%u settings=%u button=%u\n",
        (unsigned)sizeof(md_device_configuration_t),(unsigned)sizeof(md_ui_dashboard_page_t),
        (unsigned)sizeof(md_ui_page_widget_t),(unsigned)sizeof(md_ui_signal_store_t),
        (unsigned)sizeof(md_ui_history_t),(unsigned)sizeof(md_ui_settings_t),(unsigned)sizeof(md_ui_button_t));
    printf("MINIDASH_PORTABLE_TEST_SUMMARY pass=%u fail=%u\n",11-failed,failed);
    return failed ? 1 : 0;
}
