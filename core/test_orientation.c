#include "display_manager.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    for(unsigned r=0;r<4;r++) {
        md_display_profile_t profile;md_display_orientation_t o;
        assert(md_display_profile_select(MD_DRIVER_ILI9341,r,&profile));
        assert(profile.width==(r%2?320:240) && profile.height==(r%2?240:320));
        assert(md_display_orientation_select(&profile,&o));
        assert(o.swap_xy==(r%2!=0));
#if CONFIG_MINIDASH_CYD
        assert(o.mirror_x==(r==0 || r==3)); assert(o.mirror_y==(r>=2));
#else
        assert(o.mirror_x==(r>=2)); assert(o.mirror_y==(r==1 || r==2));
#endif
        assert(md_display_profile_select(MD_DRIVER_ST7789,r,&profile));
        assert(md_display_orientation_select(&profile,&o));
        assert(o.swap_xy==(r%2==0));
        assert(o.mirror_x==(r==3)); assert(o.mirror_y==(r==1 || r==2));
    }
    puts("Board orientation table passed");return 0;
}
