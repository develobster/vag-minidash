#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Hardware availability and implemented product features are independent. */
enum {
    MD_CAP_COLOR = 1U << 0, MD_CAP_MONOCHROME = 1U << 1,
    MD_CAP_TOUCH = 1U << 2, MD_CAP_BUTTONS = 1U << 3,
    MD_CAP_WIFI = 1U << 4, MD_CAP_BLE = 1U << 5,
    MD_CAP_DIMMING = 1U << 6, MD_CAP_CAN = 1U << 7,
    MD_CAP_LARGE_MEMORY = 1U << 8, MD_CAP_PSRAM = 1U << 9,
    MD_CAP_LOG_STORAGE = 1U << 10, MD_CAP_AMBIENT_SOURCE = 1U << 11,
};
typedef enum {
    MD_FEATURE_TELEMETRY, MD_FEATURE_WARNINGS, MD_FEATURE_PAGES,
    MD_FEATURE_FOCUS, MD_FEATURE_CONFIG_IO, MD_FEATURE_WEB_CONFIG,
    MD_FEATURE_UDS, MD_FEATURE_DTC_READ, MD_FEATURE_DTC_CLEAR,
    MD_FEATURE_LOGGING, MD_FEATURE_CODING, MD_FEATURE_COUNT,
} md_feature_t;
typedef struct {
    const char *name;
    uint32_t hardware;
    uint32_t implemented_features;
    uint16_t display_width, display_height;
    uint32_t flash_bytes, ram_bytes;
    uint8_t visible_widgets;
    bool verified;
} md_target_profile_t;
bool md_target_has(const md_target_profile_t *target, uint32_t capabilities);
bool md_target_feature_available(const md_target_profile_t *target, md_feature_t feature);
/* Proposal: no controller, geometry, pins or working STM32 adapter is asserted. */
md_target_profile_t md_target_lite_proposal(void);
