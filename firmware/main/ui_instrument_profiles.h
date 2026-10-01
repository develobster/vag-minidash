#pragma once
#include "ui_core.h"

typedef struct {
    const char *id, *name;
    float rpm_max, speed_max, coolant_min, coolant_max;
} md_instrument_profile_t;
typedef struct {
    const md_instrument_profile_t *profile;
    uint32_t updated_ms, ttl_ms;
} md_instrument_selection_t;

const md_instrument_profile_t *md_instrument_profile_find(const char *id);
const md_instrument_profile_t *md_instrument_profile_default(void);
bool md_instrument_profile_range(const md_instrument_profile_t *profile,
    md_ui_signal_id_t signal, float *minimum, float *maximum);
bool md_instrument_frame_decode(const char *frame, char *id, size_t capacity, uint32_t *ttl_ms);
bool md_instrument_select(md_instrument_selection_t *selection, const char *id,
    uint32_t ttl_ms, uint32_t now_ms);
const md_instrument_profile_t *md_instrument_active(const md_instrument_selection_t *selection,
    uint32_t now_ms, bool *matched);
