#pragma once
#include "driver/twai.h"
/* Explicit nominal timing: 8 MHz TQ / 16 TQ = 500 kbit/s, sample at 14/16.
 * Avoid SDK-version-dependent convenience macro sample points (75% in 5.5.5). */
static inline twai_timing_config_t md_can_timing_500k(void)
{
    const twai_timing_config_t timing={.clk_src=TWAI_CLK_SRC_DEFAULT,
        .quanta_resolution_hz=8000000,.tseg_1=13,.tseg_2=2,.sjw=2};
    return timing;
}
