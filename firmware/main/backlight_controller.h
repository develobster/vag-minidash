#pragma once
#include <stdbool.h>
#include <stdint.h>
#define MD_BACKLIGHT_FREQUENCY_HZ 5000U
#define MD_BACKLIGHT_MAX_DUTY 4095U
/* Gamma 2.0, rounded; UI percent remains independent of PWM duty. */
static inline uint32_t md_backlight_duty(uint8_t percent)
{
    const uint32_t p = percent > 100 ? 100 : percent;
    const uint32_t duty = (p * p * MD_BACKLIGHT_MAX_DUTY + 5000U) / 10000U;
    return p && !duty ? 1U : duty;
}
/* One display owner; future ambient/vehicle policy supplies the same percent API. */
bool md_backlight_init(void);
bool md_backlight_set(uint8_t percent);
bool md_backlight_power(bool on);

/* Read-only hardware snapshot; sample after at least one PWM period. */
void md_backlight_report(void);

typedef struct { uint32_t scale, cycles; } md_backlight_fade_plan_t;
static inline md_backlight_fade_plan_t md_backlight_fade_plan(uint8_t percent, uint32_t duration_ms)
{
    const uint32_t duty=md_backlight_duty(percent);
    if(!duty || !duration_ms || duration_ms>10000) return (md_backlight_fade_plan_t){0,0};
    const uint32_t scale=(duty+999)/1000;
    const uint32_t steps=duty/scale;
    uint32_t cycles=(duration_ms*MD_BACKLIGHT_FREQUENCY_HZ/1000+steps/2)/steps;
    if(cycles<1) cycles=1;
    if(cycles>1023) cycles=1023;
    return (md_backlight_fade_plan_t){scale,cycles};
}
/* One startup owner; autonomous LEDC fade, does not alter persistent settings. */
bool md_backlight_startup_fade(uint8_t percent, uint32_t duration_ms);
