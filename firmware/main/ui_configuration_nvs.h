#pragma once
#include "ui_configuration.h"
bool md_configuration_storage_load(md_device_configuration_t *config,
                                    const md_target_profile_t *target);
bool md_configuration_storage_save(const md_device_configuration_t *config);
