#include "ui_startup.h"
static uint32_t smooth(uint32_t elapsed, uint32_t duration)
{
    const uint32_t p = elapsed >= duration ? 1000 : elapsed * 1000 / duration;
    return p * p * (3000 - 2 * p) / 1000000;
}
md_ui_startup_frame_t md_ui_startup_frame(uint32_t elapsed, uint16_t width,
                                         uint16_t height, uint8_t target)
{
    if (target > 100) target = 100;
    if (elapsed >= MD_UI_STARTUP_DURATION_MS)
        return (md_ui_startup_frame_t){width,height,target,0,true};
    const uint32_t horizontal = smooth(elapsed, 300);
    const uint32_t vertical = elapsed > 250 ? smooth(elapsed-250, 1500) : 0;
    const uint16_t initial_width = width < 4 ? width : 4;
    const uint16_t initial_height = height < 2 ? height : 2;
    return (md_ui_startup_frame_t){
        .width = initial_width + (width-initial_width)*horizontal/1000,
        .height = initial_height + (height-initial_height)*vertical/1000,
        .brightness = (target*smooth(elapsed,MD_UI_STARTUP_DURATION_MS)+500)/1000,
        .glow_opacity = elapsed < 500 ? 255 : 255*(MD_UI_STARTUP_DURATION_MS-elapsed)/1500,
        .complete = false,
    };
}
