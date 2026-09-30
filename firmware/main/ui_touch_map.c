#include "ui_touch_map.h"

#include <math.h>
#include <stddef.h>

static bool solve3(const double input[3][3], const double rhs[3], float result[3])
{
    double a[3][4];
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) a[row][col] = input[row][col];
        a[row][3] = rhs[row];
    }
    for (int pivot = 0; pivot < 3; ++pivot) {
        int best = pivot;
        for (int row = pivot + 1; row < 3; ++row)
            if (fabs(a[row][pivot]) > fabs(a[best][pivot])) best = row;
        if (fabs(a[best][pivot]) < 1e-5) return false;
        for (int col = pivot; col < 4; ++col) {
            const double swap = a[pivot][col];
            a[pivot][col] = a[best][col];
            a[best][col] = swap;
        }
        const double scale = a[pivot][pivot];
        for (int col = pivot; col < 4; ++col) a[pivot][col] /= scale;
        for (int row = 0; row < 3; ++row) {
            if (row == pivot) continue;
            const double factor = a[row][pivot];
            for (int col = pivot; col < 4; ++col)
                a[row][col] -= factor * a[pivot][col];
        }
    }
    for (int i = 0; i < 3; ++i) result[i] = (float)a[i][3];
    return true;
}

bool md_touch_map_fit(md_touch_map_t *map,
                      const md_touch_cal_point_t points[MD_TOUCH_CAL_POINTS],
                      uint16_t width, uint16_t height, uint8_t rotation)
{
    if (!map || !points || width < 100 || height < 100 || rotation > 3) return false;
    double normal[3][3] = {{0}};
    double bx[3] = {0}, by[3] = {0};
    for (int i = 0; i < MD_TOUCH_CAL_POINTS; ++i) {
        if (!isfinite(points[i].raw_x) || !isfinite(points[i].raw_y) ||
            points[i].raw_x < 0 || points[i].raw_x > 4095 ||
            points[i].raw_y < 0 || points[i].raw_y > 4095) return false;
        const double v[3] = {1, points[i].raw_x, points[i].raw_y};
        for (int j = 0; j < 3; ++j) {
            bx[j] += v[j] * points[i].screen_x;
            by[j] += v[j] * points[i].screen_y;
            for (int k = 0; k < 3; ++k) normal[j][k] += v[j] * v[k];
        }
    }
    md_touch_map_t fitted = {.width = width, .height = height, .rotation = rotation};
    if (!solve3(normal, bx, fitted.x) || !solve3(normal, by, fitted.y)) return false;
    for (int i = 0; i < MD_TOUCH_CAL_POINTS; ++i) {
        const float x = fitted.x[0] + fitted.x[1] * points[i].raw_x +
                        fitted.x[2] * points[i].raw_y;
        const float y = fitted.y[0] + fitted.y[1] * points[i].raw_x +
                        fitted.y[2] * points[i].raw_y;
        if (hypotf(x - points[i].screen_x, y - points[i].screen_y) > 18.0f)
            return false;
    }
    *map = fitted;
    return true;
}

bool md_touch_map_apply(const md_touch_map_t *map, uint16_t raw_x, uint16_t raw_y,
                        int16_t *screen_x, int16_t *screen_y)
{
    if (!map || !screen_x || !screen_y || !map->width || !map->height) return false;
    const float x = map->x[0] + map->x[1] * raw_x + map->x[2] * raw_y;
    const float y = map->y[0] + map->y[1] * raw_x + map->y[2] * raw_y;
    if (!isfinite(x) || !isfinite(y) || x < -12 || y < -12 ||
        x >= map->width + 12 || y >= map->height + 12) return false;
    *screen_x = (int16_t)fmaxf(0, fminf(map->width - 1, x));
    *screen_y = (int16_t)fmaxf(0, fminf(map->height - 1, y));
    return true;
}

md_touch_hit_t md_touch_hit_zone(int16_t x, int16_t y,
                                  uint16_t width, uint16_t height)
{
    if (x < 0 || y < 0 || x >= width || y >= height) return MD_TOUCH_HIT_NONE;
    const int edge = width * 56 / 320;
    if (x < edge) return MD_TOUCH_HIT_PREVIOUS;
    if (x >= width - edge) return MD_TOUCH_HIT_NEXT;
    return MD_TOUCH_HIT_CONTENT;
}
