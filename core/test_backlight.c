/* Exercise the actual IDF controller against deterministic driver doubles. */
#include <assert.h>
#include <stdio.h>
#include "backlight_controller.h"
#include "driver/ledc.h"
static int configured, latch, initialized, fail_update, stopped;
static uint32_t pending, applied;
esp_err_t gpio_set_level(int pin,uint32_t value) { assert(pin==4 && !configured); latch=(int)value; return ESP_OK; }
esp_err_t gpio_config(const gpio_config_t *c) { assert(latch==0 && c->pin_bit_mask==(1ULL<<4)); configured=1; return ESP_OK; }
esp_err_t ledc_timer_config(const ledc_timer_config_t *c) { assert(configured && c->freq_hz==5000 && c->duty_resolution==12 && c->clk_cfg==LEDC_USE_XTAL_CLK); return ESP_OK; }
esp_err_t ledc_channel_config(const ledc_channel_config_t *c) { assert(c->gpio_num==4 && c->duty==0); applied=pending=c->duty; initialized=1; return ESP_OK; }
esp_err_t ledc_set_duty(int mode,int channel,uint32_t duty) { assert(initialized && mode==0 && channel==0 && duty<=4095); pending=duty; return ESP_OK; }
esp_err_t ledc_update_duty(int mode,int channel) { (void)mode;(void)channel; if(fail_update) return -1; applied=pending; return ESP_OK; }
uint32_t ledc_get_duty(int mode,int channel) { (void)mode;(void)channel; return applied; }
esp_err_t ledc_stop(int mode,int channel,uint32_t level) { (void)mode;(void)channel; assert(level==0); stopped=1; applied=0; return ESP_OK; }
uint32_t ledc_get_freq(int mode,int timer) { (void)mode;(void)timer; return 5000; }
int main(void)
{
    assert(!md_backlight_set(80));
    assert(md_backlight_init() && applied==0);
    assert(md_backlight_init() && applied==0);
    const uint8_t levels[]={100,75,50,25,10,0};
    const uint32_t duties[]={4095,2303,1024,256,41,0};
    for(unsigned i=0;i<sizeof(levels);++i) {
        assert(md_backlight_set(levels[i]) && applied==duties[i]);
    }
    assert(!md_backlight_set(101) && applied==0);
    assert(md_backlight_set(75));
    assert(md_backlight_power(false) && applied==0);
    assert(md_backlight_power(true) && applied==2303);
    assert(md_backlight_power(false));
    assert(md_backlight_set(50) && applied==0);
    assert(md_backlight_power(true) && applied==1024);
    fail_update=1;
    assert(!md_backlight_set(80) && stopped && applied==0);
    assert(!md_backlight_set(100));
    puts("Backlight boot, gamma, sleep/wake and fail-off contracts PASS");
    return 0;
}
