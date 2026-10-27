#pragma once
#include <stdint.h>
typedef int esp_err_t;
#define ESP_OK 0
typedef enum { ESP_SLEEP_WAKEUP_UNDEFINED, ESP_SLEEP_WAKEUP_TIMER, ESP_SLEEP_WAKEUP_EXT0,
               ESP_SLEEP_WAKEUP_ALL } esp_sleep_wakeup_cause_t;
esp_sleep_wakeup_cause_t esp_sleep_get_wakeup_cause(void);
esp_err_t esp_sleep_disable_wakeup_source(esp_sleep_wakeup_cause_t source);
esp_err_t esp_sleep_enable_timer_wakeup(uint64_t microseconds);
esp_err_t esp_sleep_enable_ext0_wakeup(int pin,int level);
void esp_deep_sleep_start(void);
