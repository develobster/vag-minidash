#include "cyd_mode.h"

md_cyd_mode_t md_cyd_mode_restore(unsigned stored_demo)
{ return stored_demo == 0 ? MD_CYD_MONITOR : MD_CYD_DEMO; }
md_cyd_mode_t md_cyd_mode_demo(bool demo)
{ return demo ? MD_CYD_DEMO : MD_CYD_MONITOR; }
bool md_cyd_mode_arm(md_cyd_mode_t current, bool ready,
                     bool confirmed, md_cyd_mode_t *next)
{
    if (!next || current != MD_CYD_MONITOR || !ready || !confirmed)
        return false;
    *next = MD_CYD_OBD;
    return true;
}
bool md_cyd_mode_tx_allowed(md_cyd_mode_t mode)
{ return mode == MD_CYD_OBD; }
bool md_cyd_can_pins_valid(int tx, int rx)
{
    /* Reviewed candidate pair only. All TFT, touch, UART, flash, SD, RGB,
     * LDR and audio pins are excluded. GPIO35 is an input-only alternative. */
    return tx == 22 && (rx == 27 || rx == 35);
}
