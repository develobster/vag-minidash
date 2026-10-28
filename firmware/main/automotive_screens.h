#pragma once
#include "can_pipeline.h"
#include "ui_theme.h"
#include "lvgl.h"
bool md_automotive_screen(md_ui_screen_t screen);
bool md_automotive_create(uint16_t width,uint16_t height,const md_ui_palette_t *palette);
bool md_automotive_open(md_ui_screen_t screen);
void md_automotive_release(void);
lv_obj_t *md_automotive_root(md_ui_screen_t screen);
void md_automotive_retheme(const md_ui_palette_t *palette);
void md_automotive_frame(const md_dashboard_frame_t *frame);
void md_automotive_update(md_ui_screen_t screen,uint32_t now_ms);
/* Native/software-render acceptance uses the same objects and layout. */
bool md_automotive_geometry_valid(md_ui_screen_t screen);
