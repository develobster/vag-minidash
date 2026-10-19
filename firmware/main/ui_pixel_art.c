#include "ui_pixel_art.h"

bool md_ui_theme_is_pixel(md_ui_theme_id_t theme)
{
    return theme==MD_UI_THEME_PIXEL_ARCADE || theme==MD_UI_THEME_PIXEL_DUNGEON || theme==MD_UI_THEME_PIXEL_HANDHELD;
}
static void rect(md_pixel_rect_fn draw,void *context,int x,int y,int w,int h,uint32_t rgb)
{ if(w>0 && h>0)draw(context,x,y,w,h,rgb); }
void md_pixel_background(md_ui_theme_id_t theme,const md_ui_palette_t *p,
    int w,int h,md_pixel_rect_fn draw,void *context)
{
    if(!p || !draw || !md_ui_theme_is_pixel(theme) || w<16 || h<16)return;
    for(int x=0;x<w;x+=16) {
        rect(draw,context,x,0,x+8<w?8:w-x,2,p->border);
        rect(draw,context,x,h-2,x+8<w?8:w-x,2,p->border);
    }
    /* A restrained tile grid appears only in the gaps around opaque cards. */
    for(int y=32;y<h-16;y+=48)for(int x=8;x<w;x+=48)
        rect(draw,context,x,y,2,2,p->surface_alt);
}
void md_pixel_card(md_ui_theme_id_t theme,const md_ui_palette_t *p,
    int w,int h,unsigned level,bool available,md_pixel_rect_fn draw,void *context)
{
    if(!p || !draw || !md_ui_theme_is_pixel(theme) || w<24 || h<24)return;
    rect(draw,context,2,2,w-4,2,p->border);
    rect(draw,context,2,4,2,h-8,p->border);
    rect(draw,context,4,h-4,w-6,2,p->background);
    rect(draw,context,w-4,4,2,h-8,p->background);
    for(int i=0;i<2;i++) {
        int x=i?w-10:6;
        rect(draw,context,x,6,4,2,p->accent);
        if(theme==MD_UI_THEME_PIXEL_DUNGEON)rect(draw,context,x,8,2,2,p->muted);
    }
    if(h<48)return;
    unsigned slots=(unsigned)(w-16)/8;
    if(slots>36)slots=36;
    if(level>1000)level=1000;
    for(unsigned i=0;i<slots;i++) {
        bool filled=available && i*1000U<level*slots;
        uint32_t ink=filled?p->accent:p->surface_alt;
        if(theme==MD_UI_THEME_PIXEL_HANDHELD)ink=filled?p->accent:p->border;
        rect(draw,context,8+(int)i*8,h-5,6,2,ink);
    }
}
void md_pixel_transition(md_ui_theme_id_t theme,const md_ui_palette_t *p,
    int w,int h,unsigned progress,md_pixel_rect_fn draw,void *context)
{
    if(!p || !draw || !md_ui_theme_is_pixel(theme) || w<=0 || h<=0 || progress>=1000)return;
    if(theme==MD_UI_THEME_PIXEL_DUNGEON) {
        /* Uneven falling columns, inspired by old FPS menu wipes. */
        for(unsigned i=0;i<12;i++) {
            unsigned delay=(i*37U+i*i*11U)%240U;
            unsigned travel=progress>delay?(progress-delay)*1000U/(1000U-delay):0;
            int y=(int)(travel*(unsigned)h/1000U)/4*4;
            int x=(int)i*w/12,next=(int)(i+1)*w/12;
            rect(draw,context,x,y,next-x,h-y,i%3?p->background:p->surface);
        }
    } else if(theme==MD_UI_THEME_PIXEL_ARCADE) {
        /* Six staggered block rows clear from left to right. */
        for(unsigned i=0;i<6;i++) {
            unsigned delay=i*35;
            unsigned travel=progress>delay?(progress-delay)*1000U/(1000U-delay):0;
            int x=(int)(travel*(unsigned)w/1000U)/8*8;
            int y=(int)i*h/6,next=(int)(i+1)*h/6;
            rect(draw,context,x,y,w-x,next-y,i%2?p->background:p->surface_alt);
        }
    } else {
        static const uint8_t order[16]={0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
        for(unsigned y=0;y<6;y++)for(unsigned x=0;x<8;x++) {
            if(progress>order[(y%4)*4+x%4]*60U)continue;
            int left=(int)x*w/8,right=(int)(x+1)*w/8;
            int top=(int)y*h/6,bottom=(int)(y+1)*h/6;
            rect(draw,context,left,top,right-left,bottom-top,p->background);
        }
    }
}
