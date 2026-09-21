#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
// Deliberately no CAN initialization: board pinout/transceiver are not verified.
void app_main(void) {
    ESP_LOGI("minidash", "Bring-up skeleton; CAN disabled; no display driver yet");
    while (1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
}
