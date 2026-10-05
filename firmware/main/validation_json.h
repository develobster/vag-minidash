#pragma once
#include "vehicle_app.h"
#include "cJSON.h"
cJSON *md_validation_json(const md_vehicle_validation_t *snapshot, const char *firmware_version,
                          uint64_t uptime_ms);
