#include <stdbool.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_bt.h"
#include "esp_chip_info.h"
#include "esp_err.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_intr_alloc.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "display_app.h"
#include "ui_web.h"
#include "ui_core_selftest.h"
#include "vehicle_app.h"

/* Board self-tests are development tooling; CAN runs in its own task. */
#define PSRAM_TEST_BYTES (6U * 1024U * 1024U)
#define INTERNAL_TEST_BYTES 1024U

RTC_DATA_ATTR static uint32_t s_boot_count;
static uint32_t s_pass_count;
static uint32_t s_fail_count;
static esp_netif_t *s_wifi_netif;
static bool s_console_input_blocking;

static const char *reset_reason_name(esp_reset_reason_t reason)
{
    switch (reason) {
    case ESP_RST_POWERON: return "POWERON";
    case ESP_RST_EXT: return "EXT";
    case ESP_RST_SW: return "SW";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT: return "INT_WDT";
    case ESP_RST_TASK_WDT: return "TASK_WDT";
    case ESP_RST_WDT: return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_SDIO: return "SDIO";
    default: return "OTHER";
    }
}

static void report_test(const char *name, bool passed, const char *details)
{
    printf("MINIDASH_TEST name=%s status=%s details=%s\n",
           name, passed ? "PASS" : "FAIL", details);
    fflush(stdout);
    if (passed) {
        ++s_pass_count;
    } else {
        ++s_fail_count;
    }
}

static bool test_chip_info(void)
{
    esp_chip_info_t info = {0};
    esp_chip_info(&info);

    const bool passed = info.model == CHIP_ESP32S3 && info.cores == 2;
    char details[128];
    snprintf(details, sizeof(details),
             "model=%s cores=%u revision=%u.%u features=0x%lx idf=%s",
             info.model == CHIP_ESP32S3 ? "ESP32-S3" : "unexpected",
             (unsigned)info.cores, (unsigned)(info.revision / 100),
             (unsigned)(info.revision % 100),
             (unsigned long)info.features, esp_get_idf_version());
    report_test("chip_info", passed, details);
    return passed;
}

static bool test_flash_size(void)
{
    uint32_t size = 0;
    const esp_err_t err = esp_flash_get_physical_size(NULL, &size);
    const bool passed = err == ESP_OK && size == 16U * 1024U * 1024U;
    char details[128];
    snprintf(details, sizeof(details), "physical_bytes=%lu expected_bytes=16777216 result=%s",
             (unsigned long)size, esp_err_to_name(err));
    report_test("flash_size", passed, details);
    return passed;
}

static bool test_psram(void)
{
    const size_t total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    const size_t free_before = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    uint32_t *memory = heap_caps_malloc(PSRAM_TEST_BYTES,
                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    bool passed = total >= 7U * 1024U * 1024U && memory != NULL;

    if (memory != NULL) {
        volatile uint32_t *words = (volatile uint32_t *)memory;
        const size_t count = PSRAM_TEST_BYTES / sizeof(uint32_t);
        for (size_t i = 0; i < count; ++i) {
            words[i] = 0x5AA50000U ^ ((uint32_t)i * 0x9E3779B9U);
        }
        for (size_t i = 0; i < count; ++i) {
            if (words[i] != (0x5AA50000U ^ ((uint32_t)i * 0x9E3779B9U))) {
                passed = false;
                break;
            }
        }
        if (passed) {
            for (size_t i = 0; i < count; ++i) {
                words[i] = ~(0x5AA50000U ^ ((uint32_t)i * 0x9E3779B9U));
            }
            for (size_t i = 0; i < count; ++i) {
                if (words[i] != ~(0x5AA50000U ^ ((uint32_t)i * 0x9E3779B9U))) {
                    passed = false;
                    break;
                }
            }
        }
        heap_caps_free(memory);
    }

    char details[160];
    snprintf(details, sizeof(details),
             "reported_bytes=%lu free_before_bytes=%lu tested_bytes=%u pattern_passes=2",
             (unsigned long)total, (unsigned long)free_before,
             (unsigned)PSRAM_TEST_BYTES);
    report_test("psram_rw", passed, details);
    return passed;
}

static bool test_internal_heap(void)
{
    const uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    const size_t total = heap_caps_get_total_size(caps);
    const size_t free_before = heap_caps_get_free_size(caps);
    volatile uint8_t *memory = heap_caps_malloc(INTERNAL_TEST_BYTES, caps);
    bool passed = total > 0 && memory != NULL;

    if (memory != NULL) {
        for (size_t i = 0; i < INTERNAL_TEST_BYTES; ++i) {
            memory[i] = (uint8_t)(i ^ 0xA5U);
        }
        for (size_t i = 0; i < INTERNAL_TEST_BYTES; ++i) {
            if (memory[i] != (uint8_t)(i ^ 0xA5U)) {
                passed = false;
                break;
            }
        }
        heap_caps_free((void *)memory);
    }

    char details[144];
    snprintf(details, sizeof(details), "total_bytes=%lu free_before_bytes=%lu test_bytes=%u",
             (unsigned long)total, (unsigned long)free_before,
             (unsigned)INTERNAL_TEST_BYTES);
    report_test("internal_heap_rw", passed, details);
    return passed;
}

static bool test_gpio_driver(void)
{
    /* Installing the service touches no pin and drives no electrical level. */
    const esp_err_t err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    const bool passed = err == ESP_OK || err == ESP_ERR_INVALID_STATE;
    const bool installed_here = err == ESP_OK;
    if (installed_here) {
        gpio_uninstall_isr_service();
    }
    char details[112];
    snprintf(details, sizeof(details), "driver=%s pins_driven=0 pinout_known=0",
             err == ESP_ERR_INVALID_STATE ? "already_ready" : esp_err_to_name(err));
    report_test("gpio_driver", passed, details);
    return passed;
}

static bool ensure_wifi_base(void)
{
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return false;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return false;
    }

    if (s_wifi_netif == NULL) {
        s_wifi_netif = esp_netif_create_default_wifi_sta();
    }
    return s_wifi_netif != NULL;
}

static bool test_wifi_scan(void)
{
    bool passed = ensure_wifi_base();
    esp_err_t err = passed ? ESP_OK : ESP_FAIL;
    uint16_t ap_count = 0;
    bool wifi_started = false;
    bool wifi_initialized = false;

    if (passed) {
        wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
        err = esp_wifi_init(&init_config);
        passed = err == ESP_OK;
        wifi_initialized = passed;
    }
    if (passed) {
        err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
        passed = err == ESP_OK;
    }
    if (passed) {
        err = esp_wifi_set_mode(WIFI_MODE_STA);
        passed = err == ESP_OK;
    }
    if (passed) {
        err = esp_wifi_start();
        passed = err == ESP_OK;
        wifi_started = passed;
    }
    if (passed) {
        wifi_scan_config_t scan = {0};
        scan.scan_type = WIFI_SCAN_TYPE_PASSIVE;
        scan.scan_time.passive = 120;
        err = esp_wifi_scan_start(&scan, true);
        passed = err == ESP_OK;
    }
    if (passed) {
        err = esp_wifi_scan_get_ap_num(&ap_count);
        passed = err == ESP_OK;
    }

    if (wifi_started) {
        const esp_err_t stop_err = esp_wifi_stop();
        if (stop_err != ESP_OK) {
            passed = false;
            err = stop_err;
        }
    }
    if (wifi_initialized) {
        const esp_err_t deinit_err = esp_wifi_deinit();
        if (deinit_err != ESP_OK) {
            passed = false;
            err = deinit_err;
        }
    }

    char details[128];
    snprintf(details, sizeof(details), "scan=passive credentials=none aps=%u result=%s",
             (unsigned)ap_count, esp_err_to_name(err));
    report_test("wifi_passive_scan", passed, details);
    return passed;
}

static bool test_ble_controller(void)
{
    esp_bt_controller_config_t config = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_bt_controller_init(&config);
    bool initialized = err == ESP_OK;
    bool enabled = false;
    bool passed = initialized;

    if (passed) {
        err = esp_bt_controller_enable(ESP_BT_MODE_BLE);
        enabled = err == ESP_OK;
        passed = enabled;
    }
    if (enabled) {
        const esp_err_t disable_err = esp_bt_controller_disable();
        if (disable_err != ESP_OK) {
            passed = false;
            err = disable_err;
        }
    }
    if (initialized) {
        const esp_err_t deinit_err = esp_bt_controller_deinit();
        if (deinit_err != ESP_OK) {
            passed = false;
            err = deinit_err;
        }
    }

    char details[96];
    snprintf(details, sizeof(details), "mode=BLE controller_init_enable=%s",
             esp_err_to_name(err));
    report_test("ble_controller", passed, details);
    return passed;
}

static bool test_task_watchdog(void)
{
    bool initialized_here = false;
    bool subscribed_here = false;
    esp_err_t err = esp_task_wdt_status(NULL);
    bool current_task_subscribed = err == ESP_OK;

    if (err == ESP_ERR_INVALID_STATE) {
        const esp_task_wdt_config_t config = {
            .timeout_ms = 5000,
            .idle_core_mask = 0,
            .trigger_panic = false,
        };
        err = esp_task_wdt_init(&config);
        initialized_here = err == ESP_OK;
        current_task_subscribed = false;
    }

    bool passed = err == ESP_OK || err == ESP_ERR_NOT_FOUND;
    if (passed && !current_task_subscribed) {
        err = esp_task_wdt_add(NULL);
        subscribed_here = err == ESP_OK;
        passed = subscribed_here;
    }
    if (passed) {
        for (unsigned i = 0; i < 10; ++i) {
            err = esp_task_wdt_reset();
            if (err != ESP_OK) {
                passed = false;
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    if (subscribed_here) {
        const esp_err_t remove_err = esp_task_wdt_delete(NULL);
        if (remove_err != ESP_OK) {
            passed = false;
            err = remove_err;
        }
    }
    if (initialized_here) {
        const esp_err_t deinit_err = esp_task_wdt_deinit();
        if (deinit_err != ESP_OK) {
            passed = false;
            err = deinit_err;
        }
    }

    char details[112];
    snprintf(details, sizeof(details), "feed_cycles=10 deliberate_timeout=0 result=%s",
             esp_err_to_name(err));
    report_test("task_watchdog_feed", passed, details);
    return passed;
}

static void __attribute__((unused)) run_self_test(void)
{
    s_pass_count = 0;
    s_fail_count = 0;
    printf("MINIDASH_TEST_START\n");
    fflush(stdout);

    test_chip_info();
    test_flash_size();
    test_psram();
    test_internal_heap();
    test_gpio_driver();
    test_wifi_scan();
    test_ble_controller();
    test_task_watchdog();

#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    report_test("usb_serial_jtag", s_console_input_blocking,
                s_console_input_blocking
                    ? "configured_console=USB_SERIAL_JTAG input_blocking=1"
                    : "configured_console=USB_SERIAL_JTAG input_blocking=0");
#else
    report_test("usb_serial_jtag", false, "configured_console_is_not_USB_SERIAL_JTAG");
#endif

    printf("MINIDASH_SUMMARY pass=%lu fail=%lu\n",
           (unsigned long)s_pass_count, (unsigned long)s_fail_count);
    fflush(stdout);
}

static void print_ready(void)
{
    printf("MINIDASH_READY boot=%lu commands=run,ui-test,web-test,config,config-export,reboot,navigation\n",
           (unsigned long)s_boot_count);
    fflush(stdout);
}

void app_main(void)
{
    ++s_boot_count;
    const int input_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    s_console_input_blocking = input_flags >= 0 &&
        fcntl(STDIN_FILENO, F_SETFL, input_flags & ~O_NONBLOCK) == 0;
    printf("MINIDASH_BOOT count=%lu reset=%s idf=%s\n",
           (unsigned long)s_boot_count,
           reset_reason_name(esp_reset_reason()), esp_get_idf_version());
    printf("MINIDASH_INFO board=ESP32-S3 cores_expected=2 flash_expected_bytes=16777216 psram_expected_bytes=8388608\n");
    fflush(stdout);
    print_ready();
    md_display_app_start();
    md_vehicle_app_start();

    char line[128];
    for (;;) {
        if (fgets(line, sizeof(line), stdin) == NULL) {
            /* A non-blocking VFS may report temporary lack of input as EOF. */
            clearerr(stdin);
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        if (!strchr(line, '\n')) {
            int ch;
            do {
                ch = getchar();
            } while (ch != '\n' && ch != EOF);
            printf("MINIDASH_COMMAND status=REJECTED reason=LINE_TOO_LONG\n");
            fflush(stdout);
            continue;
        }
        line[strcspn(line, "\r\n")] = '\0';
        if (strncmp(line, "MDS1|", 5) == 0) {
#if CONFIG_MINIDASH_HOST_INJECTION
            if (!md_display_app_publish_serial_sample(line)) {
                printf("MINIDASH_SAMPLE status=REJECTED\n");
                fflush(stdout);
            }
#else
            printf("MINIDASH_SAMPLE status=REJECTED reason=HOST_INJECTION_DISABLED\n");
#endif
        } else if (strcmp(line, "run") == 0) {
#if !CONFIG_MINIDASH_CAN_ENABLE
            if (!md_ui_web_active()) run_self_test();
            else printf("MINIDASH_TEST_REJECTED config_mode_active\n");
#else
            printf("MINIDASH_TEST_REJECTED standalone_runtime_active\n");
#endif
            print_ready();
        } else if (strcmp(line, "status") == 0) {
            printf("MINIDASH_BOOT count=%lu reset=%s idf=%s\n",
                   (unsigned long)s_boot_count,
                   reset_reason_name(esp_reset_reason()), esp_get_idf_version());
            print_ready();
        } else if (strcmp(line, "config-legacy") == 0) {
            printf("MINIDASH_CONFIG_RECOVERY status=%s\n",md_display_app_restore_legacy()?"SAVED":"REJECTED");
            fflush(stdout);
        } else if (strcmp(line, "config-export") == 0) {
            md_display_app_configuration_dump();
        } else if (strcmp(line, "config") == 0) {
            md_display_app_config_mode();
        } else if (strcmp(line, "web-test") == 0) {
            md_ui_web_selftest();
        } else if (strcmp(line, "ui-test") == 0) {
            (void)md_ui_core_selftest_run();
            print_ready();
        } else if (strcmp(line, "reboot") == 0) {
            printf("MINIDASH_REBOOT_REQUESTED\n");
            fflush(stdout);
            vTaskDelay(pdMS_TO_TICKS(100));
            esp_restart();
        } else if (strcmp(line, "next") == 0 || strcmp(line, "prev") == 0 ||
                   strcmp(line, "select") == 0 || strcmp(line, "back") == 0 ||
                   strcmp(line, "home") == 0 ||
                   strcmp(line, "settings") == 0 || strcmp(line, "up") == 0 ||
                   strcmp(line, "down") == 0 || strcmp(line, "plus") == 0 ||
                   strcmp(line, "minus") == 0 || strcmp(line, "sleep") == 0 ||
                   strcmp(line, "wake") == 0) {
            md_display_app_command(line);
        } else if (line[0] != '\0') {
            printf("MINIDASH_COMMAND unknown\n");
            fflush(stdout);
        }
    }
}
