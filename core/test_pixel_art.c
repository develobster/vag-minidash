#include "ui_pixel_art.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {int w,h,x,y;unsigned count;unsigned long area;bool json,comma;} output_t;
static void collect(void *context,int x,int y,int w,int h,uint32_t rgb)
{
    output_t *out=context;
    assert(x>=0 && y>=0 && w>0 && h>0 && x+w<=out->w && y+h<=out->h);
    assert(rgb<=0xffffff);
    out->count++;out->area+=(unsigned long)w*h;
    if(out->json) {
        printf("%s[%d,%d,%d,%d,%u]",out->comma?",":"",x+out->x,y+out->y,w,h,rgb);
        out->comma=true;
    }
}
static void preview(void)
{
    printf("{\"themes\":[");
    for(unsigned theme=MD_UI_THEME_PIXEL_ARCADE;theme<=MD_UI_THEME_PIXEL_HANDHELD;theme++) {
        md_ui_palette_t p=md_ui_theme_palette(theme,false);
        printf("%s{\"id\":%u,\"name\":\"%s\",\"palette\":[%u,%u,%u,%u,%u,%u,%u,%u],\"decor\":[",
            theme==MD_UI_THEME_PIXEL_ARCADE?"":",",theme,md_ui_theme_name(theme),
            p.background,p.surface,p.surface_alt,p.border,p.text,p.muted,p.accent,p.unavailable);
        output_t out={.w=320,.h=240,.json=true};
        md_pixel_background(theme,&p,320,240,collect,&out);
        const int cards[][4]={{10,39,174,112},{190,39,120,78},{190,121,120,86},{10,157,174,57}};
        const unsigned levels[]={480,370,540,420};
        for(unsigned i=0;i<4;i++) {
            out.w=cards[i][2];out.h=cards[i][3];out.x=cards[i][0];out.y=cards[i][1];
            collect(&out,0,0,out.w,out.h,p.surface);
            md_pixel_card(theme,&p,out.w,out.h,levels[i],true,collect,&out);
        }
        printf("],\"frames\":[");
        for(unsigned progress=0;progress<=1000;progress+=50) {
            printf("%s[",progress?",":"");
            output_t frame={.w=320,.h=240,.json=true};
            md_pixel_transition(theme,&p,320,240,progress,collect,&frame);
            printf("]");
        }
        printf("]}");
    }
    puts("]}");
}
int main(int argc,char **argv)
{
    if(argc==2 && !strcmp(argv[1],"--preview")) {preview();return 0;}
    assert(MD_UI_THEME_PORSCHE==10 && MD_UI_THEME_PIXEL_ARCADE==11);
    const int sizes[][2]={{320,240},{240,320},{17,19},{480,320}};
    for(unsigned theme=0;theme<MD_UI_THEME_COUNT;theme++)for(unsigned n=0;n<4;n++) {
        int w=sizes[n][0],h=sizes[n][1];
        md_ui_palette_t p=md_ui_theme_palette(theme,false);
        unsigned long previous=(unsigned long)w*h;
        for(unsigned progress=0;progress<=1000;progress++) {
            output_t out={.w=w,.h=h};
            md_pixel_transition(theme,&p,w,h,progress,collect,&out);
            assert(out.count<=48 && out.area<=previous);
            if(!progress && md_ui_theme_is_pixel(theme))assert(out.area==(unsigned long)w*h);
            if(progress==1000 || !md_ui_theme_is_pixel(theme))assert(out.count==0);
            previous=out.area;
        }
        for(unsigned level=0;level<=1100;level+=100) {
            output_t out={.w=w,.h=h};
            md_pixel_card(theme,&p,w,h,level,true,collect,&out);
            assert(out.count<=44);
            out.count=0;md_pixel_background(theme,&p,w,h,collect,&out);
            assert(out.count<=160);
        }
    }
    puts("MINIDASH_PIXEL_ART_TEST bounds/coverage/monotonicity/legacy=PASS");return 0;
}
