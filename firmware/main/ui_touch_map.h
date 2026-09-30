#pragma once


#include <stdbool.h>
#include <stdint.h>

#define MD_TOUCH_CAL_POINTS 5

typedef struct { float raw_x, raw_y, screen_x, screen_y; } md_touch_cal_point_t;
typedef struct {
    float x[3];
    float y[3];
    uint16_t width, height;
    uint8_t rotation;
} md_touch_map_t;

typedef enum {
    MD_TOUCH_HIT_NONE = 0,
    MD_TOUCH_HIT_PREVIOUS,
    MD_TOUCH_HIT_NEXT,
    MD_TOUCH_HIT_CONTENT,
} md_touch_hit_t;

bool md_touch_map_fit(md_touch_map_t *map,
                      const md_touch_cal_point_t points[MD_TOUCH_CAL_POINTS],
                      uint16_t width, uint16_t height, uint8_t rotation);
bool md_touch_map_apply(const md_touch_map_t *map, uint16_t raw_x, uint16_t raw_y,
                        int16_t *screen_x, int16_t *screen_y);
md_touch_hit_t md_touch_hit_zone(int16_t x, int16_t y,
                                  uint16_t width, uint16_t height);

void md_touch_rotate_point(float *x, float *y, unsigned width, unsigned height, unsigned rotation, bool inverse);
