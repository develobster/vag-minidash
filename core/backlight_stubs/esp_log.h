#pragma once
#include <stdio.h>
#define ESP_LOGI(tag,...) do { printf(__VA_ARGS__); puts(""); } while(0)
#define ESP_LOGE(tag,...) ESP_LOGI(tag,__VA_ARGS__)
static inline const char *esp_err_to_name(int err) { (void)err; return "MOCK_ERROR"; }
