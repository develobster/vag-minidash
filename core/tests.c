#include "ui_platform_selftest.h"
#include "ui_configuration.h"
#include <stdio.h>
int main(void)
{
    unsigned failed=0;
#define TEST(F) do { bool passed=(F)(); printf("%s: %s\n",#F,passed?"PASS":"FAIL"); if (!passed) ++failed; } while(0)
    TEST(md_ui_test_widget_precision); TEST(md_ui_test_configuration); TEST(md_ui_test_configuration_validation);
    TEST(md_ui_test_configuration_crc); TEST(md_ui_test_history);
    TEST(md_ui_test_buttons); TEST(md_ui_test_brightness); TEST(md_ui_test_target_capabilities);
    printf("SIZES config=%u page=%u widget=%u store=%u history=%u settings=%u button=%u\n",
        (unsigned)sizeof(md_device_configuration_t),(unsigned)sizeof(md_ui_dashboard_page_t),
        (unsigned)sizeof(md_ui_page_widget_t),(unsigned)sizeof(md_ui_signal_store_t),
        (unsigned)sizeof(md_ui_history_t),(unsigned)sizeof(md_ui_settings_t),(unsigned)sizeof(md_ui_button_t));
    printf("MINIDASH_PORTABLE_TEST_SUMMARY pass=%u fail=%u\n",8-failed,failed);
    return failed ? 1 : 0;
}
