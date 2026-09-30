#include "ui_touch_nvs.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#include "nvs.h"
#include "ui_settings_nvs.h"

#define TOUCH_CAL_VERSION 1U

typedef struct {
    uint32_t version;
    md_touch_map_t map;
    uint32_t checksum;
} stored_map_t;

static uint32_t checksum(const uint8_t *data, size_t length)
{
    uint32_t hash = 2166136261U;
    for (size_t i = 0; i < length; ++i) hash = (hash ^ data[i]) * 16777619U;
    return hash;
}

static bool map_valid(const md_touch_map_t *map)
{
    if (!map || map->width < 100 || map->height < 100 || map->rotation > 3) return false;
    for (int i = 0; i < 3; ++i)
        if (!isfinite(map->x[i]) || !isfinite(map->y[i])) return false;
    return true;
}

bool md_touch_map_storage_load(md_touch_map_t *map,
                               uint16_t width, uint16_t height, uint8_t rotation)
{
    if (!map || !md_ui_settings_storage_init()) return false;
    nvs_handle_t handle;
    if (nvs_open("minidash_ui", NVS_READONLY, &handle) != ESP_OK) return false;
    stored_map_t record = {0};
    size_t size = sizeof(record);
    const esp_err_t err = nvs_get_blob(handle, "touch_cal", &record, &size);
    nvs_close(handle);
    if (err != ESP_OK || size != sizeof(record) || record.version != TOUCH_CAL_VERSION ||
        record.checksum != checksum((const uint8_t *)&record,
                                    offsetof(stored_map_t, checksum)) ||
        !map_valid(&record.map) || record.map.width != width ||
        record.map.height != height || record.map.rotation != rotation) return false;
    *map = record.map;
    return true;
}

bool md_touch_map_storage_save(const md_touch_map_t *map)
{
    if (!map_valid(map) || !md_ui_settings_storage_init()) return false;
    stored_map_t record = {.version = TOUCH_CAL_VERSION, .map = *map};
    record.checksum = checksum((const uint8_t *)&record,
                               offsetof(stored_map_t, checksum));
    nvs_handle_t handle;
    if (nvs_open("minidash_ui", NVS_READWRITE, &handle) != ESP_OK) return false;
    esp_err_t err = nvs_set_blob(handle, "touch_cal", &record, sizeof(record));
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err == ESP_OK;
}
