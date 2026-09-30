#pragma once
#include "ui_configuration.h"
#include "cJSON.h"
bool md_ui_web_start(void);
void md_ui_web_stop(void);
bool md_ui_web_active(void);
const char *md_ui_web_ssid(void);
const char *md_ui_web_password(void);
bool md_ui_web_selftest(void);
cJSON *md_configuration_json(const md_device_configuration_t *config, uint32_t revision);
bool md_configuration_parse_json(const cJSON *root, md_device_configuration_t *config, uint32_t *revision);

/* UI controller owns publication, persistence, revision checks and driving gates. */
bool md_display_configuration_read(md_device_configuration_t *config, uint32_t *revision,
                                   md_target_profile_t *target);
int md_display_configuration_apply(const md_device_configuration_t *config,
                                  uint32_t revision, bool save, bool cancel);

bool md_display_configuration_begin(void);
