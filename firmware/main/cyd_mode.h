#pragma once
#include <stdbool.h>

typedef enum { MD_CYD_DEMO, MD_CYD_MONITOR, MD_CYD_OBD } md_cyd_mode_t;
/* Stored state deliberately cannot restore transmission after a restart. */
md_cyd_mode_t md_cyd_mode_restore(unsigned stored_demo);
md_cyd_mode_t md_cyd_mode_demo(bool demo);
bool md_cyd_mode_arm(md_cyd_mode_t current, bool ready,
                     bool confirmed, md_cyd_mode_t *next);
bool md_cyd_mode_tx_allowed(md_cyd_mode_t mode);
bool md_cyd_can_pins_valid(int tx, int rx);
