#pragma once
#include "ui_theme.h"
/* Original integer-grid artwork shared by LVGL and the native preview tool.
 * No canvas, frame copy, sprites, allocations or vehicle-state mutations. */
typedef void (*md_pixel_rect_fn)(void *context,int x,int y,int width,int height,uint32_t rgb);
bool md_ui_theme_is_pixel(md_ui_theme_id_t theme);
void md_pixel_background(md_ui_theme_id_t theme,const md_ui_palette_t *palette,
    int width,int height,md_pixel_rect_fn draw,void *context);
void md_pixel_card(md_ui_theme_id_t theme,const md_ui_palette_t *palette,
    int width,int height,unsigned level,bool available,md_pixel_rect_fn draw,void *context);
/* progress 0..1000: fully covered -> completely revealed, <=48 rectangles. */
void md_pixel_transition(md_ui_theme_id_t theme,const md_ui_palette_t *palette,
    int width,int height,unsigned progress,md_pixel_rect_fn draw,void *context);
