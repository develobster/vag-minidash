#include "can_incident.h"
#include "ui_settings_nvs.h"
#include "nvs.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
bool md_can_incident_save(const md_can_incident_t *record)
{
    nvs_handle_t handle;
    if(!md_ui_settings_storage_init() || nvs_open("md_can_diag",NVS_READWRITE,&handle)!=ESP_OK)return false;
    esp_err_t error=nvs_set_blob(handle,"incident",record,sizeof(*record));
    if(error==ESP_OK)error=nvs_commit(handle);
    nvs_close(handle);
    if(error!=ESP_OK)ESP_LOGW("md_can_diag","Incident save failed: %s",esp_err_to_name(error));
    return error==ESP_OK;
}
void md_can_incident_dump(void)
{
    nvs_handle_t handle;
    if(!md_ui_settings_storage_init() || nvs_open("md_can_diag",NVS_READONLY,&handle)!=ESP_OK)return;
    md_can_incident_t record={0};size_t length=sizeof(record);
    esp_err_t error=nvs_get_blob(handle,"incident",&record,&length);nvs_close(handle);
    if(error!=ESP_OK || length!=sizeof(record) || record.schema!=1)return;
    record.firmware[sizeof(record.firmware)-1]=0;
    printf("MINIDASH_CAN_INCIDENT firmware=%s uptime_ms=%lu rx=%lu tx_confirmed=%lu tx_failed=%lu timeouts=%lu arb=%lu bus=%lu TEC_peak=%lu REC_peak=%lu ACK=%lu BIT=%lu FORM=%lu STUFF=%lu OTHER=%lu rx_standard=%lu rx_extended=%lu last_id=%08lX last_extended=%lu\n",
        record.firmware,(unsigned long)record.uptime_ms,(unsigned long)record.rx,(unsigned long)record.tx,
        (unsigned long)record.tx_failed,(unsigned long)record.timeouts,(unsigned long)record.arbitration,
        (unsigned long)record.bus_errors,(unsigned long)record.peak_tec,(unsigned long)record.peak_rec,
        (unsigned long)record.errors.ack,(unsigned long)record.errors.bit,(unsigned long)record.errors.form,
        (unsigned long)record.errors.stuff,(unsigned long)record.errors.other,
        (unsigned long)record.rx_standard,(unsigned long)record.rx_extended,
        (unsigned long)record.last_id,(unsigned long)record.last_extended);
    fflush(stdout);
}
#ifdef MINIDASH_USB_CAN_BENCH
bool md_can_incident_selftest(const md_can_incident_t *record)
{
    nvs_handle_t handle;
    if(!md_ui_settings_storage_init() || nvs_open("md_can_diag",NVS_READWRITE,&handle)!=ESP_OK)return false;
    md_can_incident_t previous={0},loaded={0};size_t length=sizeof(previous);
    esp_err_t previous_result=nvs_get_blob(handle,"incident",&previous,&length);
    nvs_close(handle);
    /* Preserve unknown/newer records rather than truncating them for a test. */
    if(previous_result!=ESP_ERR_NVS_NOT_FOUND && (previous_result!=ESP_OK || length!=sizeof(previous)))return false;
    bool passed=md_can_incident_save(record);
    if(nvs_open("md_can_diag",NVS_READWRITE,&handle)!=ESP_OK)return false;
    length=sizeof(loaded);
    passed=passed && nvs_get_blob(handle,"incident",&loaded,&length)==ESP_OK && length==sizeof(loaded) && !memcmp(record,&loaded,length);
    esp_err_t restored=previous_result==ESP_OK?nvs_set_blob(handle,"incident",&previous,sizeof(previous)):nvs_erase_key(handle,"incident");
    if(restored==ESP_OK)restored=nvs_commit(handle);
    nvs_close(handle);
    return passed && restored==ESP_OK;
}
#endif
