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
