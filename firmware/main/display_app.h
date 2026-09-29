#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "ui_core.h"

void md_display_app_start(void);
void md_display_app_command(const char *command);
bool md_display_app_dispatch(md_ui_input_source_t source, md_ui_action_t action);
bool md_display_app_publish_signal(const char *canonical_signal, float value,
                                   md_ui_signal_state_t state, uint32_t ttl_ms,
                                   bool demo);
bool md_display_app_publish_serial_sample(const char *frame);
bool md_display_app_set_warning(const char *canonical_signal, bool enabled,
                                md_ui_threshold_direction_t direction,
                                int32_t warning_milli, int32_t critical_milli);
