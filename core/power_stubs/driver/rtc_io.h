#pragma once
#include <stdbool.h>
#include "esp_sleep.h"
esp_err_t rtc_gpio_deinit(int pin);
bool rtc_gpio_is_valid_gpio(int pin);
