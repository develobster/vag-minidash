#include "ui_settings_nvs.h"

#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#define UI_SETTINGS_NAMESPACE "minidash_ui"
#define UI_SETTINGS_KEY "settings"

static const char *TAG = "md_ui_settings";
static bool s_nvs_ready;

bool md_ui_settings_storage_init(void)
{
    if (s_nvs_ready) return true;
    const esp_err_t err = nvs_flash_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "NVS unavailable (%s); using defaults without erasing flash",
                 esp_err_to_name(err));
        return false;
    }
    s_nvs_ready = true;
    return true;
}

bool md_ui_settings_storage_load(md_ui_settings_t *settings)
{
    if (!settings) return false;
    md_ui_settings_defaults(settings);
    if (!md_ui_settings_storage_init()) return false;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(UI_SETTINGS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) return true;
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Could not open settings (%s); using defaults", esp_err_to_name(err));
        return false;
    }

    size_t size = 0;
    err = nvs_get_blob(handle, UI_SETTINGS_KEY, NULL, &size);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle);
        return true;
    }
    uint8_t wire[MD_UI_SETTINGS_WIRE_SIZE];
    if (err == ESP_OK && (size == MD_UI_SETTINGS_V1_WIRE_SIZE ||
                          size == MD_UI_SETTINGS_V2_WIRE_SIZE ||
                          size == sizeof(wire))) {
        err = nvs_get_blob(handle, UI_SETTINGS_KEY, wire, &size);
    } else if (err == ESP_OK) {
        err = ESP_ERR_INVALID_SIZE;
    }
    nvs_close(handle);
    if (err != ESP_OK || !md_ui_settings_decode(wire, size, settings)) {
        md_ui_settings_defaults(settings);
        ESP_LOGW(TAG, "Stored settings are invalid; using defaults");
        return false;
    }
    if (wire[4] < 3U && !md_ui_settings_storage_save(settings)) {
        ESP_LOGW(TAG, "Older settings loaded but version-3 migration could not be saved");
    }
    return true;
}

bool md_ui_settings_storage_save(const md_ui_settings_t *settings)
{
    if (!settings || !md_ui_settings_validate(settings) ||
        !md_ui_settings_storage_init()) return false;

    uint8_t wire[MD_UI_SETTINGS_WIRE_SIZE];
    size_t size = 0;
    if (!md_ui_settings_encode(settings, wire, sizeof(wire), &size)) return false;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(UI_SETTINGS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Could not open settings for writing (%s)", esp_err_to_name(err));
        return false;
    }
    err = nvs_set_blob(handle, UI_SETTINGS_KEY, wire, size);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Could not save settings (%s)", esp_err_to_name(err));
        return false;
    }
    return true;
}
