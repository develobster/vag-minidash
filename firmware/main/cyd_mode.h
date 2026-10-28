#pragma once
#include <stdbool.h>
#include "ui_core.h"

typedef enum { MD_CYD_DEMO, MD_CYD_MONITOR, MD_CYD_OBD } md_cyd_mode_t;
/* Stored state deliberately cannot restore transmission after a restart. */
md_cyd_mode_t md_cyd_mode_restore(unsigned stored_demo);
md_cyd_mode_t md_cyd_mode_demo(bool demo);
bool md_cyd_mode_arm(md_cyd_mode_t current, bool ready,
                     bool confirmed, md_cyd_mode_t *next);
bool md_cyd_mode_tx_allowed(md_cyd_mode_t mode);
bool md_cyd_can_pins_valid(int tx, int rx);
/* Filter a possibly old UI mailbox frame during a source change. Returns true
 * when vehicle/Drag payloads must also be cleared. */
bool md_cyd_filter_store(md_ui_signal_store_t *store, bool desired_demo,
                         unsigned frame_generation, unsigned current_generation);
bool md_cyd_demo_wifi_allowed(unsigned speed_state, bool demo);
bool md_cyd_mode_needs_apply(unsigned applied, unsigned applied_generation,
                             unsigned desired, unsigned desired_generation);
