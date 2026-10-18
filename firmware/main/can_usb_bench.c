#include "can_usb_bench.h"
#include "driver/twai.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <string.h>

void md_can_usb_bench(void)
{
    const char *tag="can_usb_bench";
    twai_handle_t handle=NULL;
    twai_general_config_t general=TWAI_GENERAL_CONFIG_DEFAULT(
        CONFIG_MINIDASH_CAN_TX_GPIO,CONFIG_MINIDASH_CAN_RX_GPIO,TWAI_MODE_LISTEN_ONLY);
    general.tx_queue_len=0;general.rx_queue_len=8;
    twai_timing_config_t timing=TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t filter=TWAI_FILTER_CONFIG_ACCEPT_ALL();
    ESP_LOGW(tag,"USB-ONLY BENCH IMAGE. OBD MUST BE DISCONNECTED. Production acquisition disabled.");
    esp_err_t error=twai_driver_install_v2(&general,&timing,&filter,&handle);
    if(error!=ESP_OK)goto done;
    error=twai_start_v2(handle);if(error!=ESP_OK)goto done;
    /* Refuse a visibly active bus. This is supplementary to physical isolation,
     * not a way to establish that an attached vehicle is safe to test. */
    twai_message_t received;
    if(twai_receive_v2(handle,&received,pdMS_TO_TICKS(1000))==ESP_OK) {
        ESP_LOGE(tag,"REFUSED: bus traffic present");error=ESP_ERR_INVALID_STATE;goto done;
    }
    twai_stop_v2(handle);twai_driver_uninstall_v2(handle);handle=NULL;
    general.mode=TWAI_MODE_NO_ACK;
    error=twai_driver_install_v2(&general,&timing,&filter,&handle);
    if(error!=ESP_OK)goto done;
    error=twai_start_v2(handle);if(error!=ESP_OK)goto done;
    unsigned passed=0;
    for(unsigned i=0;i<20;i++) {
        twai_message_t frame={.identifier=0x321,.data_length_code=8,.self=1,.ss=1};
        for(unsigned b=0;b<8;b++)frame.data[b]=(uint8_t)((i*17+b*37)^0x55);
        esp_err_t sent=twai_transmit_v2(handle,&frame,0);
        esp_err_t read=twai_receive_v2(handle,&received,pdMS_TO_TICKS(100));
        bool match=sent==ESP_OK && read==ESP_OK && received.identifier==frame.identifier &&
            !received.extd && !received.rtr && received.data_length_code==8 &&
            memcmp(received.data,frame.data,8)==0;
        if(match)passed++;
        twai_status_info_t info={0};twai_get_status_info_v2(handle,&info);
        ESP_LOGI(tag,"frame=%u match=%u send=%s receive=%s errors=%lu TEC=%lu REC=%lu",
            i,match,esp_err_to_name(sent),esp_err_to_name(read),
            (unsigned long)info.bus_error_count,(unsigned long)info.tx_error_counter,
            (unsigned long)info.rx_error_counter);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    ESP_LOGI(tag,"RESULT loopback=%u/20 (external transceiver feedback, no ACK required)",passed);
done:
    if(handle){twai_stop_v2(handle);twai_driver_uninstall_v2(handle);}
    ESP_LOGW(tag,"STOPPED status=%s. Restore production image before attaching OBD.",esp_err_to_name(error));
}
