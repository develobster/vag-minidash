#include "automotive_screens.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define PAGE_COUNT 5U
#define MAX_CELLS 12U
typedef struct {lv_obj_t *label;char text[64];uint32_t color;} cached_label_t;
typedef struct {
    lv_obj_t *root,*bar,*segments[8];
    cached_label_t cell[MAX_CELLS],source;
    unsigned used,shift_mask;
    int bar_value;
} auto_page_t;
static auto_page_t pages[PAGE_COUNT];
static md_dashboard_frame_t frame;
static md_ui_palette_t palette;
static uint16_t width,height;
static const char *const titles[]={"SPORT / BOOST","THERMALS & HEALTH","DPF / DIESEL","DRAG / 0-100","NIGHT / MINIMAL"};
static int xp(int x) {return x*width/320;}
static int yp(int y) {return y*height/240;}
bool md_automotive_screen(md_ui_screen_t s)
{return s>=MD_UI_SCREEN_SPORT_BOOST && s<=MD_UI_SCREEN_NIGHT_MINIMAL;}
static auto_page_t *page(md_ui_screen_t s)
{return md_automotive_screen(s)?&pages[s-MD_UI_SCREEN_SPORT_BOOST]:NULL;}
lv_obj_t *md_automotive_root(md_ui_screen_t s)
{auto_page_t *p=page(s);return p?p->root:NULL;}
static lv_obj_t *label(auto_page_t *p,const char *text,int x,int y,int w,int h,const lv_font_t *font,uint32_t color)
{
    if(width<320) {
        if(font==&lv_font_montserrat_48) font=&lv_font_montserrat_28;
        else if(font==&lv_font_montserrat_28) font=&lv_font_montserrat_20;
        else if(font==&lv_font_montserrat_20) font=&lv_font_montserrat_16;
        else if(font==&lv_font_montserrat_12) font=&lv_font_montserrat_10;
    }
    lv_obj_t *l=lv_label_create(p->root);if(!l) return NULL;
    lv_label_set_long_mode(l,LV_LABEL_LONG_MODE_CLIP);lv_label_set_text(l,text);
    lv_obj_set_pos(l,xp(x),yp(y));lv_obj_set_size(l,xp(w),yp(h));
    lv_obj_set_style_text_font(l,font,0);lv_obj_set_style_text_color(l,lv_color_hex(color),0);
    return l;
}
static void field(auto_page_t *p,const char *title,int x,int y,int w,int h,const lv_font_t *font)
{
    if(p->used>=MAX_CELLS) return;
    (void)label(p,title,x,y,w,16,&lv_font_montserrat_12,palette.muted);
    cached_label_t *c=&p->cell[p->used++];
    c->label=label(p,"--",x,y+18,w,h-18,font,palette.text);c->color=UINT32_MAX;
}
static void set(cached_label_t *c,const char *text,uint32_t color)
{
    if(!c->label) return;
    if(strcmp(c->text,text)) {snprintf(c->text,sizeof(c->text),"%s",text);lv_label_set_text(c->label,c->text);}
    if(c->color!=color) {lv_obj_set_style_text_color(c->label,lv_color_hex(color),0);c->color=color;}
}
static vag_quality_t value(auto_page_t *p,unsigned index,vag_signal_t signal,uint32_t now,const char *unit,unsigned decimals)
{
    double number;char text[64];
    vag_quality_t q=vag_telemetry_sample(&frame.vehicle,signal,now,&number);
    if(q!=VAG_VALID && q!=VAG_CANDIDATE) snprintf(text,sizeof(text),"-- %s",unit);
    else snprintf(text,sizeof(text),"%s%.*f%s%s",q==VAG_CANDIDATE?"~":"",(int)decimals,number,unit[0]?" ":"",unit);
    uint32_t color=q==VAG_VALID?(p==&pages[4]?0xffffff:palette.text):q==VAG_CANDIDATE?palette.warning:palette.unavailable;
    if(q==VAG_VALID && (signal==VAG_COOLANT || signal==VAG_OIL || signal==VAG_DSG_TEMP || signal==VAG_IAT))
        color=number<75?0x4ba7ff:number<=110?0x47d88a:number<=120?0xffbc42:0xff4d5a;
    if(index<p->used) set(&p->cell[index],text,color);
    return q;
}
bool md_automotive_create(uint16_t w,uint16_t h,const md_ui_palette_t *colors)
{
    if(!colors || !w || !h) return false;
    width=w;height=h;palette=*colors;memset(pages,0,sizeof(pages));
    for(unsigned i=0;i<PAGE_COUNT;i++) {
        auto_page_t *p=&pages[i];p->bar_value=-1;p->shift_mask=UINT32_MAX;
        p->root=lv_obj_create(NULL);if(!p->root) return false;
        lv_obj_remove_style_all(p->root);lv_obj_set_size(p->root,w,h);
        lv_obj_set_style_bg_opa(p->root,LV_OPA_COVER,0);
        lv_obj_set_style_bg_color(p->root,lv_color_hex(i==4?0:palette.background),0);
        lv_obj_remove_flag(p->root,LV_OBJ_FLAG_SCROLLABLE);
        (void)label(p,titles[i],8,5,304,20,&lv_font_montserrat_16,palette.text);
        p->source.label=label(p,"NO DATA",8,27,304,15,&lv_font_montserrat_10,palette.muted);
        p->source.color=UINT32_MAX;
        (void)label(p,"< PREV      TAP NEXT       NEXT >",8,221,304,17,&lv_font_montserrat_10,palette.muted);
    }
    auto_page_t *p=&pages[0];
    for(unsigned i=0;i<8;i++) {
        p->segments[i]=lv_obj_create(p->root);if(!p->segments[i]) return false;
        lv_obj_remove_style_all(p->segments[i]);lv_obj_set_pos(p->segments[i],xp(8+i*38),yp(46));
        lv_obj_set_size(p->segments[i],xp(34),yp(9));lv_obj_set_style_bg_opa(p->segments[i],LV_OPA_COVER,0);
    }
    field(p,"BOOST / bar gauge",10,63,290,82,&lv_font_montserrat_48);
    p->bar=lv_bar_create(p->root);if(!p->bar) return false;
    lv_obj_set_pos(p->bar,xp(10),yp(143));lv_obj_set_size(p->bar,xp(300),yp(8));lv_bar_set_range(p->bar,0,300);
    field(p,"km/h",10,162,78,50,&lv_font_montserrat_20);
    field(p,"GEAR",90,162,62,50,&lv_font_montserrat_20);
    field(p,"RPM",156,162,78,50,&lv_font_montserrat_20);
    field(p,"OIL C",238,162,74,50,&lv_font_montserrat_20);
    p=&pages[1];
    field(p,"COOLANT C",10,49,144,72,&lv_font_montserrat_28);
    field(p,"ENGINE OIL C",166,49,144,72,&lv_font_montserrat_28);
    field(p,"DSG OIL C",10,132,144,72,&lv_font_montserrat_28);
    field(p,"INTAKE AIR C",166,132,144,72,&lv_font_montserrat_28);
    p=&pages[2];
    field(p,"REGENERATION",10,49,300,66,&lv_font_montserrat_28);
    field(p,"SOOT g",10,125,140,76,&lv_font_montserrat_20);
    field(p,"DP hPa",166,125,144,44,&lv_font_montserrat_20);
    field(p,"HEADROOM %",166,171,144,44,&lv_font_montserrat_20);
    p=&pages[3];
    field(p,"0-100 km/h",10,48,225,73,&lv_font_montserrat_48);
    p->bar=lv_bar_create(p->root);if(!p->bar) return false;
    lv_obj_set_pos(p->bar,xp(10),yp(121));lv_obj_set_size(p->bar,xp(300),yp(4));lv_bar_set_range(p->bar,0,1000);
    field(p,"STATE",240,48,72,60,&lv_font_montserrat_12);
    field(p,"0-60 s",10,128,90,47,&lv_font_montserrat_20);
    field(p,"60-100 s",110,128,100,47,&lv_font_montserrat_20);
    field(p,"PEAK RPM",10,177,130,39,&lv_font_montserrat_16);
    field(p,"PEAK BAR",166,177,144,39,&lv_font_montserrat_16);
    (void)label(p,"RESET",245,110,65,18,&lv_font_montserrat_12,palette.accent);
    p=&pages[4];
    field(p,"SPEED km/h",12,52,292,88,&lv_font_montserrat_48);
    field(p,"GEAR",12,157,112,55,&lv_font_montserrat_28);
    field(p,"BOOST bar",166,157,144,55,&lv_font_montserrat_28);
    md_automotive_retheme(colors);return true;
}
void md_automotive_retheme(const md_ui_palette_t *colors)
{
    if(!colors) return;
    palette=*colors;
    for(unsigned i=0;i<PAGE_COUNT;i++) {
        auto_page_t *p=&pages[i];if(!p->root) continue;
        lv_obj_set_style_bg_color(p->root,lv_color_hex(i==4?0:palette.background),0);
        for(unsigned j=0;j<lv_obj_get_child_count(p->root);j++) {
            lv_obj_t *child=lv_obj_get_child(p->root,j);
            lv_obj_set_style_text_color(child,lv_color_hex(i==4?0xffffff:palette.text),0);
        }
        for(unsigned j=0;j<p->used;j++) p->cell[j].color=UINT32_MAX;
        p->source.color=UINT32_MAX;p->shift_mask=UINT32_MAX;p->bar_value=-1;
    }
}
void md_automotive_frame(const md_dashboard_frame_t *input)
{if(input) frame=*input;}
static void thermals(auto_page_t *p,uint32_t now)
{
    static const vag_signal_t signals[]={VAG_COOLANT,VAG_OIL,VAG_DSG_TEMP,VAG_IAT};
    for(unsigned i=0;i<4;i++) {
        (void)value(p,i,signals[i],now,"",0);
    }
}
void md_automotive_update(md_ui_screen_t screen,uint32_t now)
{
    auto_page_t *p=page(screen);if(!p || !p->root) return;
    char text[64];
    snprintf(text,sizeof(text),"%s | %s | ~ candidate",vag_platform_name(frame.config.platform),
        frame.config.layout==VAG_LAYOUT_SOURCED?"SOURCED":"EXPERIMENTAL");
    if(screen==MD_UI_SCREEN_SPORT_BOOST)
        snprintf(text,sizeof(text),"%s | SHIFT %u | ~ candidate",vag_platform_name(frame.config.platform),frame.config.shift_rpm?frame.config.shift_rpm:6500);
    set(&p->source,text,palette.muted);
    if(screen==MD_UI_SCREEN_SPORT_BOOST) {
        vag_quality_t q=value(p,0,VAG_BOOST,now,"",2);double v=0;
        if(q==VAG_VALID || q==VAG_CANDIDATE) (void)vag_telemetry_sample(&frame.vehicle,VAG_BOOST,now,&v);
        int bar=(int)(fmax(0,fmin(3,v))*100);
        if(bar!=p->bar_value) {lv_bar_set_value(p->bar,bar,LV_ANIM_OFF);p->bar_value=bar;}
        value(p,1,VAG_SPEED,now,"",0);value(p,2,VAG_GEAR,now,"",0);
        value(p,3,VAG_RPM,now,"",0);value(p,4,VAG_OIL,now,"",0);
        unsigned mask=0;double rpm;
        if(vag_telemetry_sample(&frame.vehicle,VAG_RPM,now,&rpm)==VAG_VALID) {
            unsigned shift=frame.config.shift_rpm?frame.config.shift_rpm:6500;
            for(unsigned i=0;i<8;i++) if(rpm>=shift*(.65+.35*(i+1)/8.0)) mask|=1U<<i;
        }
        if(mask!=p->shift_mask) {
            for(unsigned i=0;i<8;i++) lv_obj_set_style_bg_color(p->segments[i],
                lv_color_hex((mask&(1U<<i))?(i<5?0x47d88a:i<7?0xffbc42:0xff4d5a):palette.border),0);
            p->shift_mask=mask;
        }
    } else if(screen==MD_UI_SCREEN_THERMALS_HEALTH) thermals(p,now);
    else if(screen==MD_UI_SCREEN_DPF_DIESEL) {
        double regen=0;vag_quality_t q=vag_telemetry_sample(&frame.vehicle,VAG_REGEN,now,&regen);
        if(q!=VAG_VALID && q!=VAG_CANDIDATE) snprintf(text,sizeof(text),"NO DPF DATA");
        else snprintf(text,sizeof(text),"%s%s",q==VAG_CANDIDATE?"~":"",regen?"REGEN":"INACTIVE");
        set(&p->cell[0],text,(regen && (q==VAG_VALID || q==VAG_CANDIDATE))?(now/500%2?palette.warning:palette.muted):palette.text);
        value(p,1,VAG_SOOT,now,"",1);value(p,2,VAG_DPF_PRESSURE,now,"",1);
        value(p,3,VAG_REGEN_REMAINING,now,"",0);
    } else if(screen==MD_UI_SCREEN_DRAG) {
        const drag_result_t *r=&frame.drag;
        if(p->bar_value!=r->readiness_per_mille) {
            lv_bar_set_value(p->bar,r->readiness_per_mille,LV_ANIM_OFF);p->bar_value=r->readiness_per_mille;
        }
        if(r->state==DRAG_RUNNING || (r->result_mask&2)) snprintf(text,sizeof(text),"%.2f s",r->state==DRAG_RUNNING?r->elapsed_s:r->seconds[1]);
        else snprintf(text,sizeof(text),"--.-- s");
        set(&p->cell[0],text,palette.text);
        set(&p->cell[1],drag_state_name(r->state),r->state==DRAG_READY?palette.normal:palette.accent);
        for(unsigned i=0;i<2;i++) {
            unsigned segment=i?2:0;
            if(r->result_mask&(1U<<segment)) snprintf(text,sizeof(text),"%.2f",r->seconds[segment]);else snprintf(text,sizeof(text),"--");
            set(&p->cell[2+i],text,palette.text);
        }
        if(r->peak_mask&1) snprintf(text,sizeof(text),"%.0f",r->peak_rpm);else snprintf(text,sizeof(text),"--");
        set(&p->cell[4],text,palette.text);
        if(r->peak_mask&2) snprintf(text,sizeof(text),"%.2f",r->peak_boost_bar);else snprintf(text,sizeof(text),"--");
        set(&p->cell[5],text,palette.text);
        snprintf(text,sizeof(text),"%s | sample <= %lu ms",drag_reason_name(r->reason),(unsigned long)r->sample_interval_ms);
        set(&p->source,text,palette.muted);
    } else {
        value(p,0,VAG_SPEED,now,"",0);value(p,1,VAG_GEAR,now,"",0);value(p,2,VAG_BOOST,now,"",2);
    }
}
bool md_automotive_geometry_valid(md_ui_screen_t screen)
{
    auto_page_t *p=page(screen);if(!p || !p->root) return false;
    lv_obj_update_layout(p->root);
    for(unsigned i=0;i<lv_obj_get_child_count(p->root);i++) {
        lv_obj_t *child=lv_obj_get_child(p->root,i);lv_area_t a;lv_obj_get_coords(child,&a);
        if(a.x1<0 || a.y1<0 || a.x2>=width || a.y2>=height) return false;
    }
    return true;
}
