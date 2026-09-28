#include "display_app.h"
#include <stdio.h>
#include <string.h>
#include "display_manager.h"
#include "ili9341_backend.h"
#include "st7789_backend.h"
#include "xpt2046_touch.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static QueueHandle_t navigation_queue;

static md_display_backend_t *configured_backend(void)
{
#if CONFIG_MINIDASH_LCD_CONTROLLER_ILI9341
    return md_ili9341_backend();
#else
    return md_st7789_backend();
#endif
}

static md_driver_t configured_driver(void)
{
#if CONFIG_MINIDASH_LCD_CONTROLLER_ILI9341
    return MD_DRIVER_ILI9341;
#else
    return MD_DRIVER_ST7789;
#endif
}

static const char *configured_driver_name(void)
{
#if CONFIG_MINIDASH_LCD_CONTROLLER_ILI9341
    return "ILI9341";
#else
    return "ST7789";
#endif
}

static void __attribute__((unused)) display_task(void *argument)
{
    (void)argument;
    md_display_profile_t profile;
    md_display_manager_t manager;
    if (!md_display_profile_select(configured_driver(), CONFIG_MINIDASH_DISPLAY_ROTATION,
                                   &profile)) {
        printf("MINIDASH_DISPLAY_INIT status=FAIL reason=invalid_profile\n");
        vTaskDelete(NULL);
        return;
    }
    uint16_t *frame=heap_caps_malloc((size_t)profile.width*profile.height*2,
                                      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!frame || !md_display_open(&manager, configured_backend(), &profile, frame)) {
        printf("MINIDASH_DISPLAY_INIT status=FAIL driver=%s reason=allocation_or_panel_init\n",
               configured_driver_name());
        heap_caps_free(frame);
        vTaskDelete(NULL);
        return;
    }

    printf("MINIDASH_DISPLAY_INIT status=PASS driver=%s width=%u height=%u rotation=%u pins=sclk:%d,mosi:%d,cs:%d,dc:%d,rst:%d\n",
           configured_driver_name(), profile.width, profile.height, profile.rotation,
           CONFIG_MINIDASH_LCD_PIN_SCLK, CONFIG_MINIDASH_LCD_PIN_MOSI,
           CONFIG_MINIDASH_LCD_PIN_CS, CONFIG_MINIDASH_LCD_PIN_DC,
           CONFIG_MINIDASH_LCD_PIN_RST);

#if CONFIG_MINIDASH_DISPLAY_SMOKE_TEST
    const struct {
        const char *name;
        uint16_t color;
        uint32_t hold_ms;
    } patterns[] = {
        {"RED", 0xf800, 700},
        {"GREEN", 0x07e0, 700},
        {"BLUE", 0x001f, 700},
        {"BLACK", 0x0000, 500},
    };
    bool smoke_passed = true;
    for (size_t i = 0; i < sizeof(patterns) / sizeof(patterns[0]); ++i) {
        const bool passed = md_display_present_solid(&manager, patterns[i].color);
        printf("MINIDASH_DISPLAY_PATTERN name=%s status=%s\n",
               patterns[i].name, passed ? "PASS" : "FAIL");
        if (!passed) {
            smoke_passed = false;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(patterns[i].hold_ms));
    }
    if (smoke_passed) {
        smoke_passed = md_display_render_smoke_ok(&manager);
    }
    printf("MINIDASH_DISPLAY_TEST status=%s driver=%s width=%u height=%u rotation=%u final_text=MINIDASH_DISPLAY_OK id_readback=unavailable\n",
           smoke_passed ? "PASS" : "FAIL", configured_driver_name(),
           profile.width, profile.height, profile.rotation);
    for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
#endif

    md_xpt2046_t touch = {0};
#if CONFIG_MINIDASH_TOUCH_ENABLE
    const bool touch_ready = md_xpt2046_open(&touch);
    printf("MINIDASH_TOUCH_INIT status=%s controller=XPT2046 mode=polling clock_hz=1000000 miso=%d cs=%d irq=unconnected shared_sck=%d shared_mosi=%d\n",
           touch_ready ? "PASS" : "FAIL", CONFIG_MINIDASH_LCD_PIN_MISO,
           CONFIG_MINIDASH_TOUCH_PIN_CS, CONFIG_MINIDASH_LCD_PIN_SCLK,
           CONFIG_MINIDASH_LCD_PIN_MOSI);
#else
    const bool touch_ready = false;
    printf("MINIDASH_TOUCH_INIT status=DISABLED\n");
#endif

    md_signal_store_t store;
    md_store_init(&store);
    md_input_manager_t input={.page=MD_PAGE_DRIVE,.sleeping=false};
    printf("MINIDASH_DISPLAY ready width=%u height=%u touch=%s\n",
           profile.width,profile.height,touch_ready ? "ready" : "unavailable");
    uint32_t last_publish_ms = 0;
    uint32_t last_render_ms = 0;
    uint32_t release_since_ms = 0;
    unsigned pressed_samples = 0;
    bool touch_latched = false;
    bool render_needed = true;
    for (;;) {
        md_navigation_t action;
        if (xQueueReceive(navigation_queue,&action,pdMS_TO_TICKS(20))==pdTRUE) {
            bool was_sleeping=input.sleeping;
            md_input_dispatch(&input,MD_INPUT_CONSOLE,action);
            if (was_sleeping!=input.sleeping) md_display_power(&manager,!input.sleeping);
            render_needed = true;
        }
        uint32_t now_ms=(uint32_t)(esp_timer_get_time()/1000);
        if (touch_ready) {
            md_touch_sample_t sample;
            if (md_xpt2046_read(&touch,&sample)) {
                if (sample.pressed) {
                    release_since_ms = 0;
                    if (!touch_latched && ++pressed_samples >= 2) {
                        touch_latched = true;
                        pressed_samples = 0;
                        md_input_dispatch(&input,MD_INPUT_TOUCH,MD_NAV_NEXT);
                        render_needed = true;
                        printf("MINIDASH_TOUCH down raw_x=%u raw_y=%u z1=%u z2=%u pressure=%u action=NEXT\n",
                               sample.x,sample.y,sample.z1,sample.z2,sample.pressure);
                    }
                } else {
                    pressed_samples = 0;
                    if (touch_latched) {
                        if (!release_since_ms) release_since_ms = now_ms;
                        else if ((uint32_t)(now_ms-release_since_ms) >= 100) {
                            touch_latched = false;
                            release_since_ms = 0;
                        }
                    }
                }
            }
        }
        if ((uint32_t)(now_ms-last_publish_ms) >= 1000) {
            md_demo_publish(&store,now_ms);
            last_publish_ms = now_ms;
            render_needed = true;
        }
        if (render_needed || (uint32_t)(now_ms-last_render_ms) >= 500) {
            md_ui_model_t model;
            md_ui_project(&store,&input,now_ms,&model);
            md_display_render(&manager,&model);
            last_render_ms = now_ms;
            render_needed = false;
        }
    }
}

void md_display_app_start(void)
{
#if CONFIG_MINIDASH_DISPLAY_ENABLE
    navigation_queue=xQueueCreate(4,sizeof(md_navigation_t));
    if (!navigation_queue || xTaskCreate(display_task,"display_demo",8192,NULL,3,NULL)!=pdPASS)
        printf("MINIDASH_DISPLAY task_start_failed\n");
#else
    printf("MINIDASH_DISPLAY awaiting_verified_pinout\n");
#endif
}

void md_display_app_command(const char *command)
{
    if (!navigation_queue) {
        printf("MINIDASH_DISPLAY unavailable\n");
        return;
    }
    md_navigation_t action;
    if (strcmp(command,"next")==0) action=MD_NAV_NEXT;
    else if (strcmp(command,"prev")==0) action=MD_NAV_PREVIOUS;
    else if (strcmp(command,"sleep")==0) action=MD_NAV_SLEEP;
    else action=MD_NAV_WAKE;
    if (xQueueSend(navigation_queue,&action,0)!=pdTRUE)
        printf("MINIDASH_DISPLAY input_queue_full\n");
}
