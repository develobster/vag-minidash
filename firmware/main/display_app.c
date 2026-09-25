#include "display_app.h"
#include <stdio.h>
#include <string.h>
#include "display_manager.h"
#include "st7789_backend.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static QueueHandle_t navigation_queue;

static void __attribute__((unused)) display_task(void *argument)
{
    (void)argument;
    md_display_profile_t profile;
    md_display_manager_t manager;
    if (!md_display_profile_select(MD_DRIVER_ST7789,CONFIG_MINIDASH_DISPLAY_ROTATION,&profile)) {
        printf("MINIDASH_DISPLAY invalid_profile\n");
        vTaskDelete(NULL);
        return;
    }
    uint16_t *frame=heap_caps_malloc((size_t)profile.width*profile.height*2,
                                      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!frame || !md_display_open(&manager,md_st7789_backend(),&profile,frame)) {
        printf("MINIDASH_DISPLAY disabled_or_pinout_unconfirmed\n");
        heap_caps_free(frame);
        vTaskDelete(NULL);
        return;
    }
    md_signal_store_t store;
    md_store_init(&store);
    md_input_manager_t input={.page=MD_PAGE_DRIVE,.sleeping=false};
    printf("MINIDASH_DISPLAY ready width=%u height=%u touch=unverified\n",
           profile.width,profile.height);
    for (;;) {
        md_navigation_t action;
        if (xQueueReceive(navigation_queue,&action,pdMS_TO_TICKS(1000))==pdTRUE) {
            bool was_sleeping=input.sleeping;
            md_input_dispatch(&input,MD_INPUT_CONSOLE,action);
            if (was_sleeping!=input.sleeping) md_display_power(&manager,!input.sleeping);
        }
        uint32_t now_ms=(uint32_t)(esp_timer_get_time()/1000);
        md_demo_publish(&store,now_ms);
        md_ui_model_t model;
        md_ui_project(&store,&input,now_ms,&model);
        md_display_render(&manager,&model);
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
