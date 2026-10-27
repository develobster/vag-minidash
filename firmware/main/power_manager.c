#include "power_manager.h"
#include "can_pipeline.h"
#include "ui_web.h"
#include "sdkconfig.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include <stdio.h>

void md_power_boot(void)
{
#if CONFIG_MINIDASH_DEEP_SLEEP
    gpio_deep_sleep_hold_dis();
#if CONFIG_MINIDASH_CAN_TX_GPIO >= 0
    (void)gpio_set_level(CONFIG_MINIDASH_CAN_TX_GPIO,1);
    (void)gpio_hold_dis(CONFIG_MINIDASH_CAN_TX_GPIO);
    (void)gpio_set_direction(CONFIG_MINIDASH_CAN_TX_GPIO,GPIO_MODE_OUTPUT);
#endif
#if CONFIG_MINIDASH_LCD_PIN_BL >= 0
    (void)gpio_set_level(CONFIG_MINIDASH_LCD_PIN_BL,!CONFIG_MINIDASH_LCD_BL_ACTIVE_HIGH);
    (void)gpio_hold_dis(CONFIG_MINIDASH_LCD_PIN_BL);
#endif
#endif
#if CONFIG_MINIDASH_DEEP_SLEEP && CONFIG_MINIDASH_WAKE_GPIO >= 0
    (void)rtc_gpio_deinit(CONFIG_MINIDASH_WAKE_GPIO);
#endif
}
bool md_power_sleep_due(uint32_t now, uint32_t last)
{
#if CONFIG_MINIDASH_DEEP_SLEEP
    uint32_t timeout=CONFIG_MINIDASH_CAN_SLEEP_SECONDS*1000U;
    if(!last && esp_sleep_get_wakeup_cause()==ESP_SLEEP_WAKEUP_TIMER) timeout=3000;
    return !md_ui_web_active() && md_can_silent(now,last,timeout);
#else
    (void)now;(void)last;return false;
#endif
}
bool md_power_prepare(void)
{
#if CONFIG_MINIDASH_DEEP_SLEEP
    (void)esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    /* Timer is also a safety net for short RX pulses lost during boot. */
    if(esp_sleep_enable_timer_wakeup((uint64_t)CONFIG_MINIDASH_WAKE_SECONDS*1000000U)!=ESP_OK) return false;
#if CONFIG_MINIDASH_WAKE_GPIO >= 0
    const int pin=CONFIG_MINIDASH_WAKE_GPIO;
    const int reserved[]={0,3,19,20,CONFIG_MINIDASH_CAN_TX_GPIO,
        CONFIG_MINIDASH_LCD_PIN_SCLK,CONFIG_MINIDASH_LCD_PIN_MOSI,
        CONFIG_MINIDASH_LCD_PIN_MISO,CONFIG_MINIDASH_LCD_PIN_CS,
        CONFIG_MINIDASH_LCD_PIN_DC,CONFIG_MINIDASH_LCD_PIN_RST,CONFIG_MINIDASH_LCD_PIN_BL,
#if CONFIG_MINIDASH_TOUCH_ENABLE
        CONFIG_MINIDASH_TOUCH_PIN_CS,
#endif
    };
    if(!rtc_gpio_is_valid_gpio(pin)) return false;
    for(unsigned i=0;i<sizeof(reserved)/sizeof(reserved[0]);i++) if(pin==reserved[i]) return false;
    if(pin==CONFIG_MINIDASH_CAN_RX_GPIO && CONFIG_MINIDASH_WAKE_LEVEL!=0) return false;
    /* External bias/level conditioning is required; never connect Kl.15 12 V. */
    if(esp_sleep_enable_ext0_wakeup(pin,CONFIG_MINIDASH_WAKE_LEVEL)!=ESP_OK) return false;
#endif
    return true;
#else
    return false;
#endif
}
bool md_power_enter(void)
{
#if CONFIG_MINIDASH_DEEP_SLEEP
#if CONFIG_MINIDASH_WAKE_GPIO >= 0
    if(gpio_get_level(CONFIG_MINIDASH_WAKE_GPIO)==CONFIG_MINIDASH_WAKE_LEVEL) return false;
#endif
    ESP_LOGI("md_power","CAN silent: NVS committed, panel asleep, entering deep sleep");
    /* A floating transceiver TX input must not hold the vehicle bus dominant. */
    if(gpio_set_direction(CONFIG_MINIDASH_CAN_TX_GPIO,GPIO_MODE_OUTPUT)!=ESP_OK ||
        gpio_set_level(CONFIG_MINIDASH_CAN_TX_GPIO,1)!=ESP_OK ||
        gpio_hold_en(CONFIG_MINIDASH_CAN_TX_GPIO)!=ESP_OK) return false;
#if CONFIG_MINIDASH_LCD_PIN_BL >= 0
    if(gpio_hold_en(CONFIG_MINIDASH_LCD_PIN_BL)!=ESP_OK) {
        (void)gpio_hold_dis(CONFIG_MINIDASH_CAN_TX_GPIO);return false;
    }
#endif
    gpio_deep_sleep_hold_en();
    fflush(NULL);
    esp_deep_sleep_start();
#endif
    return false;
}
