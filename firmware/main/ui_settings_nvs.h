#pragma once

#include <stdbool.h>
#include "ui_core.h"

bool md_ui_settings_storage_init(void);
bool md_ui_settings_storage_load(md_ui_settings_t *settings);
bool md_ui_settings_storage_save(const md_ui_settings_t *settings);
