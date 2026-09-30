#pragma once

#include "ui_touch_map.h"

bool md_touch_map_storage_load(md_touch_map_t *map,
                               uint16_t width, uint16_t height, uint8_t rotation);
bool md_touch_map_storage_save(const md_touch_map_t *map);
