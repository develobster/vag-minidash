#pragma once
#include <stdbool.h>
#include <stdint.h>
void md_power_boot(void);
bool md_power_sleep_due(uint32_t now_ms, uint32_t last_valid_can_ms);
bool md_power_prepare(void);
/* Does not return on success. False means the wake level is already active. */
bool md_power_enter(void);
