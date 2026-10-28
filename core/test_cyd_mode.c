#include "cyd_mode.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    assert(md_cyd_mode_restore(0) == MD_CYD_MONITOR);
    assert(md_cyd_mode_restore(1) == MD_CYD_DEMO);
    assert(md_cyd_mode_restore(2) == MD_CYD_DEMO);
    assert(md_cyd_mode_restore(255) == MD_CYD_DEMO);
    md_cyd_mode_t next = MD_CYD_DEMO;
    for (int mode = 0; mode < 3; ++mode)
        for (int ready = 0; ready < 2; ++ready)
            for (int confirmed = 0; confirmed < 2; ++confirmed) {
                bool allowed = md_cyd_mode_arm(mode, ready, confirmed, &next);
                assert(allowed == (mode == MD_CYD_MONITOR && ready && confirmed));
                if (allowed) assert(next == MD_CYD_OBD);
            }
    assert(!md_cyd_mode_arm(MD_CYD_MONITOR, true, true, NULL));
    assert(md_cyd_mode_tx_allowed(MD_CYD_OBD));
    assert(!md_cyd_mode_tx_allowed(MD_CYD_DEMO));
    assert(!md_cyd_mode_tx_allowed(MD_CYD_MONITOR));
    /* Re-entering either source and reboot always revoke an armed session. */
    assert(!md_cyd_mode_tx_allowed(md_cyd_mode_demo(false)));
    assert(!md_cyd_mode_tx_allowed(md_cyd_mode_demo(true)));
    for (int tx = -1; tx <= 40; ++tx)
        for (int rx = -1; rx <= 40; ++rx)
            assert(md_cyd_can_pins_valid(tx, rx) == (tx == 22 && (rx == 27 || rx == 35)));
    md_ui_signal_store_t store={0};
    store.signals[MD_SIGNAL_VEHICLE_SPEED]=(md_ui_signal_record_t){.received=true,.demo=true,.value=70};
    store.signals[MD_SIGNAL_ENGINE_RPM]=(md_ui_signal_record_t){.received=true,.demo=false,.value=800};
    assert(!md_cyd_filter_store(&store,false,4,4));
    assert(!store.signals[MD_SIGNAL_VEHICLE_SPEED].received);
    assert(store.signals[MD_SIGNAL_ENGINE_RPM].received);
    assert(md_cyd_filter_store(&store,true,4,4));
    assert(!store.signals[MD_SIGNAL_ENGINE_RPM].received);
    store.signals[MD_SIGNAL_VEHICLE_SPEED]=(md_ui_signal_record_t){.received=true,.demo=true,.value=70};
    assert(!md_cyd_filter_store(&store,true,4,4));
    assert(store.signals[MD_SIGNAL_VEHICLE_SPEED].received);
    /* A double toggle leaves the same mode but must reject the old snapshot. */
    store.signals[MD_SIGNAL_ENGINE_RPM]=(md_ui_signal_record_t){.received=true,.demo=false,.value=900};
    assert(md_cyd_filter_store(&store,false,4,6));
    assert(!store.signals[MD_SIGNAL_ENGINE_RPM].received && !store.signals[MD_SIGNAL_VEHICLE_SPEED].received);
    for(unsigned state=0;state<4;state++) for(unsigned demo=0;demo<2;demo++)
        assert(md_cyd_demo_wifi_allowed(state,demo)==(state==0 && demo));
    assert(!md_cyd_mode_needs_apply(MD_CYD_MONITOR,4,MD_CYD_MONITOR,4));
    assert(md_cyd_mode_needs_apply(MD_CYD_MONITOR,4,MD_CYD_DEMO,5));
    /* A quick toggle back still forces a driver transition and status reset. */
    assert(md_cyd_mode_needs_apply(MD_CYD_MONITOR,4,MD_CYD_MONITOR,6));
    puts("CYD source persistence, TX interlocks and GPIO exclusions: PASS");
    return 0;
}
