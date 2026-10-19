#include "can_link_tx.h"
#include "freertos/task.h"
esp_err_t md_can_transmit_confirmed(twai_handle_t can,const twai_message_t *frame,uint32_t *deferred)
{
    uint32_t alerts=0;
    if(twai_read_alerts_v2(can,&alerts,0)==ESP_OK)*deferred|=alerts&~(TWAI_ALERT_TX_SUCCESS|TWAI_ALERT_TX_FAILED);
    esp_err_t error=twai_transmit_v2(can,frame,0);
    if(error!=ESP_OK)return error;
    TickType_t started=xTaskGetTickCount(),budget=pdMS_TO_TICKS(20);
    for(;;) {
        TickType_t spent=xTaskGetTickCount()-started;
        if(spent>=budget) {
            (void)twai_stop_v2(can);
            return ESP_ERR_TIMEOUT;
        }
        if(twai_read_alerts_v2(can,&alerts,budget-spent)!=ESP_OK)continue;
        *deferred|=alerts&~(TWAI_ALERT_TX_SUCCESS|TWAI_ALERT_TX_FAILED);
        if(alerts&(TWAI_ALERT_TX_FAILED|TWAI_ALERT_BUS_OFF))return ESP_FAIL;
        if(alerts&TWAI_ALERT_TX_SUCCESS)return ESP_OK;
    }
}
