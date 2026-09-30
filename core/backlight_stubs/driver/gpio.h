#pragma once
#include <stdint.h>
typedef int esp_err_t;
#define ESP_OK 0
#define GPIO_MODE_OUTPUT 1
#define GPIO_IS_VALID_OUTPUT_GPIO(pin) ((pin)>=0 && (pin)<=48)
typedef struct { uint64_t pin_bit_mask; int mode; } gpio_config_t;
esp_err_t gpio_set_level(int pin, uint32_t level);
esp_err_t gpio_config(const gpio_config_t *config);
