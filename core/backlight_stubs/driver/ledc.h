#pragma once
#include "gpio.h"
#define LEDC_LOW_SPEED_MODE 0
#define LEDC_CHANNEL_0 0
#define LEDC_TIMER_0 0
#define LEDC_TIMER_12_BIT 12
#define LEDC_USE_XTAL_CLK 3
#define LEDC_INTR_DISABLE 0
typedef struct { int speed_mode,timer_num,duty_resolution; uint32_t freq_hz; int clk_cfg; } ledc_timer_config_t;
typedef struct { int gpio_num,speed_mode,channel,timer_sel,intr_type; uint32_t duty; } ledc_channel_config_t;
esp_err_t ledc_set_duty(int mode,int channel,uint32_t duty);
esp_err_t ledc_update_duty(int mode,int channel);
uint32_t ledc_get_duty(int mode,int channel);
esp_err_t ledc_stop(int mode,int channel,uint32_t level);
esp_err_t ledc_timer_config(const ledc_timer_config_t *config);
esp_err_t ledc_channel_config(const ledc_channel_config_t *config);

uint32_t ledc_get_freq(int mode,int timer);

#define LEDC_FADE_NO_WAIT 0
esp_err_t ledc_fade_func_install(int flags);
esp_err_t ledc_set_fade_with_step(int mode,int channel,uint32_t duty,uint32_t scale,uint32_t cycles);
esp_err_t ledc_fade_start(int mode,int channel,int wait);
