#include "backlight_controller.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "sdkconfig.h"
#if CONFIG_MINIDASH_LCD_PIN_BL >= 0
static bool ready;
static bool powered = true;
static bool fade_installed;
static uint8_t requested;
static bool apply(void)
{
    const uint8_t effective = powered ? requested : 0;
    uint32_t duty = md_backlight_duty(effective);
    if (!CONFIG_MINIDASH_LCD_BL_ACTIVE_HIGH) duty = MD_BACKLIGHT_MAX_DUTY - duty;
    esp_err_t err = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    if (err == ESP_OK) err = ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    if (err != ESP_OK) {
        ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, !CONFIG_MINIDASH_LCD_BL_ACTIVE_HIGH);
        ready = false;
        ESP_LOGE("md_backlight", "PWM failed: %s; output stopped", esp_err_to_name(err));
        return false;
    }
    ESP_LOGI("md_backlight", "requested=%u effective=%u duty=%lu/%u readback_previous_cycle=%lu",
             requested, effective, (unsigned long)duty, MD_BACKLIGHT_MAX_DUTY,
             (unsigned long)ledc_get_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
    return true;
}
#endif
bool md_backlight_init(void)
{
#if CONFIG_MINIDASH_LCD_PIN_BL >= 0
    if (ready) return true;
    const int pin = CONFIG_MINIDASH_LCD_PIN_BL;
    /* Reject shared SPI/touch, straps, USB/JTAG, module memory and console pins. */
    if (!GPIO_IS_VALID_OUTPUT_GPIO(pin) || pin == 0 || pin == 3 || pin == 45 || pin == 46 ||
        (pin >= 19 && pin <= 20) || (pin >= 26 && pin <= 37) || pin >= 39 ||
        pin == CONFIG_MINIDASH_LCD_PIN_SCLK || pin == CONFIG_MINIDASH_LCD_PIN_MOSI ||
        pin == CONFIG_MINIDASH_LCD_PIN_MISO || pin == CONFIG_MINIDASH_LCD_PIN_CS ||
        pin == CONFIG_MINIDASH_LCD_PIN_DC || pin == CONFIG_MINIDASH_LCD_PIN_RST) return false;
#if CONFIG_MINIDASH_TOUCH_ENABLE
    if (pin == CONFIG_MINIDASH_TOUCH_PIN_CS) return false;
#endif
    const uint32_t off = !CONFIG_MINIDASH_LCD_BL_ACTIVE_HIGH;
    /* Preload latch before enabling output; stay dark until UI restores NVS. */
    if (gpio_set_level(pin, off) != ESP_OK) return false;
    const gpio_config_t gpio = {.pin_bit_mask = 1ULL << pin, .mode = GPIO_MODE_OUTPUT};
    if (gpio_config(&gpio) != ESP_OK) return false;
    const ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE, .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_12_BIT, .freq_hz = MD_BACKLIGHT_FREQUENCY_HZ,
#if CONFIG_IDF_TARGET_ESP32
        .clk_cfg = LEDC_AUTO_CLK,
#else
        .clk_cfg = LEDC_USE_XTAL_CLK,
#endif
    };
    if (ledc_timer_config(&timer) != ESP_OK) return false;
    const ledc_channel_config_t channel = {
        .gpio_num = pin, .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0, .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE, .duty = off ? MD_BACKLIGHT_MAX_DUTY : 0,
    };
    if (ledc_channel_config(&channel) != ESP_OK) return false;
    ready = true; powered = true; requested = 0;
    ESP_LOGI("md_backlight", "INIT gpio=%d frequency=%u resolution=12 gamma=2.0 boot=off",
             pin, MD_BACKLIGHT_FREQUENCY_HZ);
    return true;
#else
    return true; /* Existing always-on wiring remains usable. */
#endif
}
bool md_backlight_set(uint8_t percent)
{
#if CONFIG_MINIDASH_LCD_PIN_BL >= 0
    if (!ready || percent > 100) return false;
    requested = percent;
    return apply();
#else
    (void)percent; return false;
#endif
}
bool md_backlight_power(bool on)
{
#if CONFIG_MINIDASH_LCD_PIN_BL >= 0
    if (!ready) return false;
    powered = on;
    return apply(); /* Do not overwrite requested brightness on sleep. */
#else
    (void)on; return false;
#endif
}

void md_backlight_report(void)
{
#if CONFIG_MINIDASH_LCD_PIN_BL >= 0
    ESP_LOGI("md_backlight", "STATUS gpio=%d frequency=%lu duty=%lu/%u",
             CONFIG_MINIDASH_LCD_PIN_BL,
             (unsigned long)ledc_get_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0),
             (unsigned long)ledc_get_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0),
             MD_BACKLIGHT_MAX_DUTY);
#endif
}

bool md_backlight_startup_fade(uint8_t percent, uint32_t duration_ms)
{
#if CONFIG_MINIDASH_LCD_PIN_BL >= 0
    if(!ready || !powered || percent<10 || percent>100 || !duration_ms || duration_ms>10000) return false;
    if(!md_backlight_set(0)) return false;
    esp_err_t err=ESP_OK;
    if(!fade_installed) {
        err=ledc_fade_func_install(0);
        if(err==ESP_OK) fade_installed=true;
    }
    const md_backlight_fade_plan_t plan=md_backlight_fade_plan(percent,duration_ms);
    uint32_t duty=md_backlight_duty(percent);
    if(!CONFIG_MINIDASH_LCD_BL_ACTIVE_HIGH) duty=MD_BACKLIGHT_MAX_DUTY-duty;
    if(err==ESP_OK) err=ledc_set_fade_with_step(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0,duty,plan.scale,plan.cycles);
    if(err==ESP_OK) err=ledc_fade_start(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0,LEDC_FADE_NO_WAIT);
    if(err!=ESP_OK) {
        ledc_stop(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0,!CONFIG_MINIDASH_LCD_BL_ACTIVE_HIGH);
        ready=false;
        ESP_LOGE("md_backlight","Startup fade failed: %s; output stopped",esp_err_to_name(err));
        return false;
    }
    requested=percent;
    ESP_LOGI("md_backlight","FADE target=%u duty=%lu duration_ms=%lu scale=%lu cycles=%lu",
             percent,(unsigned long)duty,(unsigned long)duration_ms,(unsigned long)plan.scale,(unsigned long)plan.cycles);
    return true;
#else
    (void)percent; (void)duration_ms; return true;
#endif
}
