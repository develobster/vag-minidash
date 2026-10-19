#pragma once
#include <stdint.h>
typedef int esp_err_t;
typedef void *twai_handle_t;
typedef uint32_t TickType_t;
typedef struct { uint32_t identifier;uint8_t data_length_code,data[8]; } twai_message_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_TIMEOUT 0x107
#define TWAI_ALERT_TX_SUCCESS 1U
#define TWAI_ALERT_TX_FAILED 2U
#define TWAI_ALERT_BUS_OFF 4U
#define TWAI_ALERT_RX_QUEUE_FULL 8U
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
esp_err_t twai_read_alerts_v2(twai_handle_t,uint32_t *,TickType_t);
esp_err_t twai_transmit_v2(twai_handle_t,const twai_message_t *,TickType_t);
esp_err_t twai_stop_v2(twai_handle_t);
