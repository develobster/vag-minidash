#pragma once
#include "ui_core.h"
/* Deterministic demo data. Never represents an observed vehicle measurement. */
bool md_simulation_value(md_ui_signal_id_t id, uint32_t elapsed_ms, double *value);
