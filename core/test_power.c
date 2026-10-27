#include "power_manager.h"
#include "sdkconfig.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include <stdbool.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"POWER FAIL %d: %s\n",__LINE__,#x);abort();}}while(0)
static bool ap_active,hold_enabled,held[49],input[49];
static int levels[49],wake_pin=-1,wake_level=-1,fail_timer,fail_hold=-1,sleeps;
static uint64_t timer_us;
static esp_sleep_wakeup_cause_t cause;
static jmp_buf sleeping;
bool md_ui_web_active(void) {return ap_active;}
esp_sleep_wakeup_cause_t esp_sleep_get_wakeup_cause(void) {return cause;}
esp_err_t esp_sleep_disable_wakeup_source(esp_sleep_wakeup_cause_t s)
{CHECK(s==ESP_SLEEP_WAKEUP_ALL);return ESP_OK;}
esp_err_t esp_sleep_enable_timer_wakeup(uint64_t us) {timer_us=us;return fail_timer;}
esp_err_t esp_sleep_enable_ext0_wakeup(int pin,int level)
{wake_pin=pin;wake_level=level;return ESP_OK;}
void gpio_deep_sleep_hold_dis(void) {hold_enabled=false;}
void gpio_deep_sleep_hold_en(void) {hold_enabled=true;}
esp_err_t gpio_hold_dis(int pin) {held[pin]=false;return ESP_OK;}
esp_err_t gpio_hold_en(int pin) {if(pin==fail_hold)return -1;held[pin]=true;return ESP_OK;}
esp_err_t gpio_set_level(int pin,int level) {levels[pin]=level;return ESP_OK;}
int gpio_get_level(int pin) {CHECK(input[pin]);return levels[pin];}
esp_err_t gpio_set_direction(int pin,gpio_mode_t mode)
{input[pin]=mode==GPIO_MODE_INPUT;return ESP_OK;}
esp_err_t rtc_gpio_deinit(int pin) {(void)pin;return ESP_OK;}
bool rtc_gpio_is_valid_gpio(int pin) {return pin>=0 && pin<=21;}
void esp_deep_sleep_start(void) {++sleeps;longjmp(sleeping,1);}
int main(void)
{
#if CONFIG_MINIDASH_DEEP_SLEEP
    input[CONFIG_MINIDASH_CAN_RX_GPIO]=true;
    md_power_boot();CHECK(levels[CONFIG_MINIDASH_CAN_TX_GPIO]==1);
    CHECK(!levels[CONFIG_MINIDASH_LCD_PIN_BL] && !hold_enabled);
    CHECK(!md_power_sleep_due(29999,0));CHECK(md_power_sleep_due(30000,0));
    ap_active=true;CHECK(!md_power_sleep_due(30000,0));ap_active=false;
    cause=ESP_SLEEP_WAKEUP_TIMER;
    CHECK(!md_power_sleep_due(2999,0));CHECK(md_power_sleep_due(3000,0));
    CHECK(!md_power_sleep_due(3001,2000));cause=ESP_SLEEP_WAKEUP_UNDEFINED;
    fail_timer=-1;CHECK(!md_power_prepare());fail_timer=0;
#if CONFIG_MINIDASH_WAKE_GPIO >= 0
    levels[CONFIG_MINIDASH_WAKE_GPIO]=!CONFIG_MINIDASH_WAKE_LEVEL;
    CHECK(md_power_prepare() && input[CONFIG_MINIDASH_WAKE_GPIO]);
    CHECK(wake_pin==CONFIG_MINIDASH_WAKE_GPIO && wake_level==CONFIG_MINIDASH_WAKE_LEVEL);
    levels[CONFIG_MINIDASH_WAKE_GPIO]=CONFIG_MINIDASH_WAKE_LEVEL;
    CHECK(!md_power_prepare()); /* active ignition must not blank the display */
    CHECK(!md_power_enter() && !sleeps);
    levels[CONFIG_MINIDASH_WAKE_GPIO]=!CONFIG_MINIDASH_WAKE_LEVEL;
#else
    CHECK(wake_pin==-1);
#endif
    CHECK(md_power_prepare() && timer_us==300000000U);
    fail_hold=CONFIG_MINIDASH_CAN_TX_GPIO;
    CHECK(!md_power_enter() && !sleeps);fail_hold=-1;
    fail_hold=CONFIG_MINIDASH_LCD_PIN_BL;
    CHECK(!md_power_enter() && !held[CONFIG_MINIDASH_CAN_TX_GPIO]);fail_hold=-1;
    if(setjmp(sleeping)==0) {(void)md_power_enter();CHECK(false);}
    CHECK(sleeps==1 && hold_enabled && held[CONFIG_MINIDASH_CAN_TX_GPIO]);
    CHECK(levels[CONFIG_MINIDASH_CAN_TX_GPIO]==1 && held[CONFIG_MINIDASH_LCD_PIN_BL]);
    md_power_boot();CHECK(!hold_enabled && !held[CONFIG_MINIDASH_CAN_TX_GPIO]);
#else
    md_power_boot();CHECK(!md_power_sleep_due(30000,0));
    CHECK(!md_power_prepare() && !md_power_enter() && !sleeps);
#endif
    puts("POWER PASS: silence/AP policy, active-wake rejection, timer, GPIO holds and abort paths");
    return 0;
}
