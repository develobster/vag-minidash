#include "ui_configuration_nvs.h"
#include "ui_settings_nvs.h"
#include "nvs.h"
#include "esp_log.h"
#include <stdlib.h>
static const char *TAG = "md_config";
bool md_configuration_storage_save(const md_device_configuration_t *config)
{
    if (!md_ui_settings_storage_init()) return false;
    uint8_t *wire = malloc(MD_CONFIG_WIRE_MAX);
    if (!wire) return false;
    size_t length;
    if (!md_configuration_encode(config, wire, MD_CONFIG_WIRE_MAX, &length)) { free(wire); return false; }
    nvs_handle_t handle;
    esp_err_t err = nvs_open("minidash_ui", NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        err = nvs_set_blob(handle, "configuration", wire, length);
        if (err == ESP_OK) err = nvs_commit(handle);
        nvs_close(handle);
    }
    free(wire);
    if (err != ESP_OK) ESP_LOGW(TAG, "Configuration commit failed: %s", esp_err_to_name(err));
    return err == ESP_OK;
}
bool md_configuration_storage_load(md_device_configuration_t *config,
                                    const md_target_profile_t *target)
{
    if (!config) return false;
    md_ui_settings_t legacy;
    (void)md_ui_settings_storage_load(&legacy);
    if (!md_configuration_from_legacy(config, &legacy, target)) {
        ESP_LOGW(TAG, "Legacy config cannot be represented; preserved original record");
        md_ui_settings_defaults(&legacy);
        if (!md_configuration_from_legacy(config, &legacy, target)) return false;
    }
    nvs_handle_t handle;
    if (nvs_open("minidash_ui", NVS_READONLY, &handle) != ESP_OK) return true;
    size_t length = 0;
    esp_err_t err = nvs_get_blob(handle, "configuration", NULL, &length);
    if (err == ESP_ERR_NVS_NOT_FOUND) { nvs_close(handle); return true; }
    uint8_t *wire = length <= MD_CONFIG_WIRE_MAX && length ? malloc(length) : NULL;
    bool ok = false;
    if (wire && err == ESP_OK) {
        err = nvs_get_blob(handle, "configuration", wire, &length);
        md_device_configuration_t *decoded = malloc(sizeof(*decoded));
        if (decoded) {
            ok = err == ESP_OK && md_configuration_decode(wire, length, decoded) &&
                md_configuration_validate(decoded, target);
            if (ok) *config = *decoded;
            free(decoded);
        }
    }
    free(wire); nvs_close(handle);
    if (!ok) ESP_LOGW(TAG, "Configuration invalid; using legacy fallback without erasing records");
    return ok;
}
