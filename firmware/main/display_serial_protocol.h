#pragma once

#include <stdbool.h>

#include "ui_core.h"

typedef struct {
    char canonical_signal[MD_UI_SIGNAL_NAME_LEN];
    float value;
    md_ui_signal_state_t state;
    uint32_t ttl_ms;
} md_display_serial_sample_t;

bool md_display_serial_sample_decode(const char *frame,
                                     md_display_serial_sample_t *sample);
