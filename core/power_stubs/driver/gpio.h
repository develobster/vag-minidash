#pragma once
#include "esp_sleep.h"
typedef enum { GPIO_MODE_INPUT, GPIO_MODE_OUTPUT } gpio_mode_t;
void gpio_deep_sleep_hold_dis(void);
void gpio_deep_sleep_hold_en(void);
esp_err_t gpio_hold_dis(int pin);
esp_err_t gpio_hold_en(int pin);
esp_err_t gpio_set_level(int pin,int level);
int gpio_get_level(int pin);
esp_err_t gpio_set_direction(int pin,gpio_mode_t mode);
