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
    puts("CYD source persistence, TX interlocks and GPIO exclusions: PASS");
    return 0;
}
