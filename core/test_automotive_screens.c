#include "automotive_screens.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif
#define CHECK(x) do{if(!(x)){fprintf(stderr,"AUTO UI FAIL %d: %s\n",__LINE__,#x);abort();}}while(0)
static uint16_t pixels[320*320];
static uint8_t buffer1[320*16*2],buffer2[320*16*2];
static unsigned canvas_width,canvas_height,flushes,dirty_pixels;
static uint32_t ticks;
static uint32_t clock_ms(void) {return ticks;}
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *data)
{
    CHECK(a->x1>=0 && a->y1>=0 && a->x2<(int)canvas_width && a->y2<(int)canvas_height);
    unsigned w=(unsigned)(a->x2-a->x1+1),h=(unsigned)(a->y2-a->y1+1);
    for(unsigned y=0;y<h;y++) memcpy(pixels+(a->y1+y)*canvas_width+a->x1,data+y*w*2,w*2);
    dirty_pixels+=w*h;++flushes;lv_display_flush_ready(d);
}
static void image(const char *dir,unsigned number)
{
    char path[512];snprintf(path,sizeof(path),"%s/%ux%u-%u.ppm",dir,canvas_width,canvas_height,number);
    FILE *f=fopen(path,"wb");CHECK(f);fprintf(f,"P6\n%u %u\n255\n",canvas_width,canvas_height);
    for(unsigned i=0;i<canvas_width*canvas_height;i++) {
        uint16_t p=pixels[i];uint8_t rgb[]={((p>>11)&31)*255/31,((p>>5)&63)*255/63,(p&31)*255/31};
        CHECK(fwrite(rgb,1,3,f)==3);
    }
    CHECK(!fclose(f));
}
static void canvas(unsigned w,unsigned h,const char *dir)
{
    canvas_width=w;canvas_height=h;
    lv_display_t *display=lv_display_create(w,h);CHECK(display);
    lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display,buffer1,buffer2,sizeof(buffer1),LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display,flush);
    md_ui_palette_t palette=md_ui_theme_palette(MD_UI_THEME_SPORT,false);
    CHECK(md_automotive_create(w,h,&palette));
    md_dashboard_frame_t f={0};vag_db_config_defaults(&f.config);
    vag_can_db_t db;vag_can_db_init(&db,&f.config);
    const double values[VAG_SIGNAL_COUNT]={6200,123,50,98,105,45,85,4,1.3,1,11,80,45,1,320,-15,650,8,2.3};
    for(unsigned i=0;i<VAG_SIGNAL_COUNT;i++) CHECK(vag_can_db_publish(&db,i,values[i],0,60000,true,true));
    f.vehicle=db.telemetry;f.drag=(drag_result_t){.state=DRAG_FINISHED,.elapsed_s=12.34,
        .seconds={7.2,12.34,5.14},.result_mask=7,.peak_mask=3,.peak_rpm=6500,.peak_boost_bar=2.3,
        .sample_interval_ms=20,.readiness_per_mille=1000};
    for(unsigned i=0;i<5;i++) {
        md_ui_screen_t s=(md_ui_screen_t)(MD_UI_SCREEN_SPORT_BOOST+i);
#if CONFIG_MINIDASH_CYD
        lv_obj_t *placeholder=lv_obj_create(NULL);CHECK(placeholder);
        lv_screen_load(placeholder);md_automotive_release();
        CHECK(md_automotive_open(s));
#endif
        md_automotive_frame(&f);md_automotive_update(s,100);lv_screen_load(md_automotive_root(s));
#if CONFIG_MINIDASH_CYD
        lv_obj_delete(placeholder);
#endif
        CHECK(md_automotive_geometry_valid(s));dirty_pixels=0;flushes=0;lv_refr_now(display);
        CHECK(flushes && dirty_pixels);if(i==4) CHECK(pixels[0]==0);
        /* Screen-load/layout events may invalidate during the first refresh.
         * Settle startup before measuring application-induced dirty regions. */
        unsigned settled=0;
        for(unsigned attempt=0;attempt<4;attempt++) {
            unsigned previous=flushes;lv_refr_now(display);
            if(flushes==previous) {settled=1;break;}
        }
        fprintf(stderr,"AUTO UI %ux%u page=%u startup_flushes=%u settled=%u\n",
                w,h,i,flushes,settled);
        CHECK(settled);image(dir,i);
        md_automotive_language(MD_UI_LANGUAGE_RUSSIAN);
        md_automotive_update(s,100);lv_refr_now(display);
        lv_obj_t *translated=md_automotive_root(s);bool russian=false;
        for(unsigned j=0;j<lv_obj_get_child_count(translated);j++) {
            lv_obj_t *child=lv_obj_get_child(translated,j);
            if(!lv_obj_check_type(child,&lv_label_class)) continue;
            const char *text=lv_label_get_text(child);
            bool has_cyrillic=false;
            for(const unsigned char *q=(const unsigned char *)text;*q;q++)
                if(*q==0xd0 || *q==0xd1) {has_cyrillic=true;break;}
            if(!has_cyrillic) continue;
            russian=true;const lv_font_t *font=lv_obj_get_style_text_font(child,0);
            lv_font_glyph_dsc_t glyph;CHECK(lv_font_get_glyph_dsc(font,&glyph,0x0410,0));
            lv_point_t size;lv_text_get_size(&size,text,font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
            if(size.x>lv_obj_get_width(child)) fprintf(stderr,"Text too wide: %s %d > %d\n",text,(int)size.x,(int)lv_obj_get_width(child));
            CHECK(size.x<=lv_obj_get_width(child));
        }
        CHECK(russian);image(dir,i+10);
        md_automotive_language(MD_UI_LANGUAGE_ENGLISH);md_automotive_update(s,100);lv_refr_now(display);
        for(unsigned attempt=0;attempt<4;attempt++) {unsigned previous=flushes;lv_refr_now(display);if(flushes==previous) break;}
        unsigned before=flushes;md_automotive_update(s,100);lv_refr_now(display);CHECK(flushes==before);
        md_dashboard_frame_t changed=f;
        if(i==0 || i==4) changed.vehicle.value[VAG_SPEED]+=1;
        else if(i==1) changed.vehicle.value[VAG_OIL]=125;
        else if(i==2) changed.vehicle.value[VAG_REGEN]=0;
        else {changed.drag.seconds[1]+=.01;changed.drag.elapsed_s+=.01;}
        md_automotive_frame(&changed);dirty_pixels=0;flushes=0;md_automotive_update(s,110);lv_refr_now(display);
        CHECK(flushes && dirty_pixels<w*h/2);
        if(i==4) {
            md_dashboard_frame_t demo={0};vag_db_config_defaults(&demo.config);
            md_ui_signal_store_init(&demo.signals);
            CHECK(md_ui_signal_store_publish_id(&demo.signals,MD_SIGNAL_VEHICLE_SPEED,55,MD_UI_SIGNAL_VALID,100,500,true));
            md_automotive_frame(&demo);md_automotive_update(s,100);
            lv_obj_t *root=md_automotive_root(s);bool marked=false,shown=false;
            for(unsigned j=0;j<lv_obj_get_child_count(root);j++) {
                lv_obj_t *child=lv_obj_get_child(root,j);
                if(!lv_obj_check_type(child,&lv_label_class)) continue;
                const char *text=lv_label_get_text(child);
                if(strstr(text,"DEMO")) marked=true;
                if(strstr(text,"55")) shown=true;
            }
            CHECK(marked && shown && demo.vehicle.observed==0 && demo.drag.result_mask==0);
        }
    }
    lv_display_delete(display);
}
int main(int argc,char **argv)
{
    CHECK(argc==2);
#ifdef _WIN32
    (void)_mkdir(argv[1]);
#else
    (void)mkdir(argv[1],0700);
#endif
    lv_init();lv_tick_set_cb(clock_ms);
    canvas(320,240,argv[1]);canvas(240,320,argv[1]);
    puts("AUTO UI PASS: 5 real LVGL screens, landscape/portrait bounds, unchanged=no flush, dirty regions under half screen");
    return 0;
}
