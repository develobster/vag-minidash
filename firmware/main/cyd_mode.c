#include "cyd_mode.h"
#include <string.h>

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
bool md_cyd_filter_store(md_ui_signal_store_t *store,bool desired_demo)
{
    if(!store) return false;
    if(desired_demo && !store->signals[MD_SIGNAL_VEHICLE_SPEED].demo) {
        memset(store,0,sizeof(*store));
        for(unsigned i=0;i<MD_SIGNAL_COUNT;i++) store->signals[i].state=MD_UI_SIGNAL_UNAVAILABLE;
        return true;
    }
    if(!desired_demo) for(unsigned i=0;i<MD_SIGNAL_COUNT;i++)
        if(store->signals[i].demo)
            store->signals[i]=(md_ui_signal_record_t){.state=MD_UI_SIGNAL_UNAVAILABLE};
    return false;
}
bool md_cyd_demo_wifi_allowed(unsigned speed_state,bool demo)
{ return speed_state==0 && demo; }
bool md_cyd_mode_needs_apply(unsigned applied,unsigned applied_generation,
                             unsigned desired,unsigned desired_generation)
{ return applied!=desired || applied_generation!=desired_generation; }
