#include "vehicle_app.h"
#include "display_app.h"
#include "driver/twai.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

static SemaphoreHandle_t lock;
static md_acquisition_t engine;
static md_vehicle_status_t status;
static atomic_uint visible, background;
static atomic_bool scan;
static const char *TAG="md_vehicle";

bool md_vehicle_app_status(md_vehicle_status_t *out)
{
    if(!out || !lock || xSemaphoreTake(lock,pdMS_TO_TICKS(10))!=pdTRUE) return false;
    *out=status;xSemaphoreGive(lock);return true;
}
void md_vehicle_app_demand(uint32_t v,uint32_t b)
{ atomic_store(&visible,v);atomic_store(&background,b); }
void md_vehicle_app_scan(void) { atomic_store(&scan,true); }
size_t md_vehicle_app_trace(md_acq_trace_t *out,size_t capacity)
{
    if(!out || !lock || xSemaphoreTake(lock,pdMS_TO_TICKS(10))!=pdTRUE) return 0;
    size_t n=engine.trace_count;if(n>capacity)n=capacity;
    size_t first=(engine.trace_head+MD_ACQ_TRACE_SIZE-n)%MD_ACQ_TRACE_SIZE;
    for(size_t i=0;i<n;i++) out[i]=engine.trace[(first+i)%MD_ACQ_TRACE_SIZE];
    xSemaphoreGive(lock);return n;
}

#if CONFIG_MINIDASH_CAN_ENABLE
static twai_handle_t can;
static bool normal_mode;
static uint32_t dropped_base;
static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time()/1000); }
static bool send_can(void *unused,const md_can_frame_t *f)
{
    (void)unused;
    if(!can || !normal_mode || f->extended || f->remote || f->dlc!=8) return false;
    twai_message_t m={.identifier=f->id,.data_length_code=8,.ss=1};
    memcpy(m.data,f->data,8);
    /* Single shot: no unlimited hardware retries on an unacknowledged bus. */
    return twai_transmit_v2(can,&m,0)==ESP_OK;
}
static bool pins_valid(void)
{
    const int tx=CONFIG_MINIDASH_CAN_TX_GPIO,rx=CONFIG_MINIDASH_CAN_RX_GPIO;
    if(tx<0 || rx<0 || tx==rx || !GPIO_IS_VALID_OUTPUT_GPIO(tx) || !GPIO_IS_VALID_GPIO(rx)) return false;
    const int reserved[]={0,3,19,20,26,27,28,29,30,31,32,33,34,35,36,37,45,46,
        CONFIG_MINIDASH_LCD_PIN_SCLK,CONFIG_MINIDASH_LCD_PIN_MOSI,CONFIG_MINIDASH_LCD_PIN_MISO,
        CONFIG_MINIDASH_LCD_PIN_CS,CONFIG_MINIDASH_LCD_PIN_DC,CONFIG_MINIDASH_LCD_PIN_RST,
        CONFIG_MINIDASH_LCD_PIN_BL,
#if CONFIG_MINIDASH_TOUCH_ENABLE
        CONFIG_MINIDASH_TOUCH_PIN_CS,
#endif
    };
    for(size_t i=0;i<sizeof(reserved)/sizeof(reserved[0]);i++)
        if(tx==reserved[i] || rx==reserved[i])return false;
    return true;
}
static bool install_can(bool normal)
{
    if(can) {
        twai_status_info_t info;
        if(twai_get_status_info_v2(can,&info)==ESP_OK) dropped_base+=info.rx_missed_count+info.rx_overrun_count;
        (void)twai_stop_v2(can);(void)twai_driver_uninstall_v2(can);can=NULL;
    }
    normal_mode=false;
    twai_general_config_t g=TWAI_GENERAL_CONFIG_DEFAULT(CONFIG_MINIDASH_CAN_TX_GPIO,
        CONFIG_MINIDASH_CAN_RX_GPIO,normal?TWAI_MODE_NORMAL:TWAI_MODE_LISTEN_ONLY);
    g.tx_queue_len=0;g.rx_queue_len=64;
    g.alerts_enabled=TWAI_ALERT_BUS_OFF|TWAI_ALERT_RX_QUEUE_FULL|TWAI_ALERT_TX_FAILED;
    twai_timing_config_t timing=TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t filter=TWAI_FILTER_CONFIG_ACCEPT_ALL();
    if(twai_driver_install_v2(&g,&timing,&filter,&can)!=ESP_OK) return false;
    if(twai_start_v2(can)==ESP_OK){normal_mode=normal;return true;}
    (void)twai_driver_uninstall_v2(can);can=NULL;return false;
}
static void update_status(uint32_t now)
{
    status.state=engine.state;status.ecu_count=engine.ecu_count;status.ecu_id=engine.response_id;
    status.rx=engine.rx_count;status.tx=engine.tx_count;status.timeouts=engine.timeout_count;
    status.malformed=engine.malformed_count;status.rejected=engine.rejected_count;
    status.tx_failed=engine.tx_failed;status.trace_overwritten=engine.trace_overwritten;
    status.dtc_count=engine.dtc_count;status.dtc_valid=engine.dtc_valid;
    status.dtc_truncated=engine.dtc_truncated;status.dtc_ms=engine.dtc_ms;
    memcpy(status.dtc,engine.dtc,sizeof(status.dtc));memcpy(status.vin,engine.vin,sizeof(status.vin));
    for(unsigned i=0;i<MD_SIGNAL_COUNT;i++) status.signals[i]=md_acq_snapshot(&engine,i,now,&status.sources[i]);
}
static void vehicle_task(void *unused)
{
    (void)unused;
#if CONFIG_MINIDASH_STANDARD_OBD
    const bool allow_obd=true;
#else
    const bool allow_obd=false;
#endif
    /* No VAG profile is shipped: existing community knowledge is not verified. */
    const md_acq_config_t config={.allow_standard_obd=allow_obd,.startup_ms=1500,.profile=NULL};
    md_acq_init(&engine,&config,send_can,NULL,now_ms());
    uint32_t recovery_at=0,last_publish=0,can_started=now_ms(),recovery_attempts=0;
    bool fault=false,first=true,was_quiet=false,motion_latched=false;
    for(;;) {
        uint32_t now=now_ms();
        if(xSemaphoreTake(lock,pdMS_TO_TICKS(10))!=pdTRUE) {vTaskDelay(pdMS_TO_TICKS(10));continue;}
        if(first) {
            first=false;status.ready=pins_valid() && install_can(false);
            if(!status.ready) {snprintf(status.fault,sizeof(status.fault),"CHECK CAN PINS/DRIVER");
                md_acq_bus_available(&engine,false,now);fault=true;recovery_attempts=3;}
        }
        uint32_t alerts=0;
        if(can && twai_read_alerts_v2(can,&alerts,0)==ESP_OK) {
            if(alerts&TWAI_ALERT_BUS_OFF) {status.bus_offs++;fault=true;recovery_at=now+1000;
                status.ready=false;md_acq_bus_available(&engine,false,now);}
            if(alerts&TWAI_ALERT_TX_FAILED) engine.tx_failed++;
        }
        twai_status_info_t info;
        if(can && twai_get_status_info_v2(can,&info)==ESP_OK) status.rx_dropped=dropped_base+info.rx_missed_count+info.rx_overrun_count;
        bool requested=atomic_exchange(&scan,false);
        if(requested && fault && pins_valid()) {recovery_attempts=0;recovery_at=now;}
        if(fault && recovery_attempts<3 && (int32_t)(now-recovery_at)>=0) {
            recovery_attempts++;status.ready=install_can(false);recovery_at=now+5000;
            if(status.ready) {fault=false;status.fault[0]=0;can_started=now;md_acq_bus_available(&engine,true,now);}
        }
        if(!fault) {
            bool quiet=engine.state==MD_ACQ_QUIET;
            if(quiet && !was_quiet) {
                status.ready=install_can(false);
                if(!status.ready) {fault=true;recovery_at=now+1000;md_acq_bus_available(&engine,false,now);}
            }
            was_quiet=quiet;
            if(requested) {
                if(quiet) {md_acq_restart(&engine,now);can_started=now;}
                else (void)md_acq_request_dtc(&engine,now);
            }
            if(!fault && allow_obd && !normal_mode && engine.state!=MD_ACQ_QUIET && (uint32_t)(now-can_started)>=1500) {
                status.ready=install_can(true);
                if(!status.ready) {fault=true;recovery_at=now+1000;md_acq_bus_available(&engine,false,now);}
            }
            /* Bounded work and queues: a saturated bus cannot monopolise this task. */
            for(unsigned i=0;can && !fault && i<32;i++) {
                twai_message_t m;if(twai_receive_v2(can,&m,0)!=ESP_OK)break;
                md_can_frame_t f={.id=m.identifier,.dlc=m.data_length_code,.extended=m.extd,.remote=m.rtr};
                memcpy(f.data,m.data,8);md_acq_receive(&engine,&f,now);
            }
            md_acq_set_demand(&engine,atomic_load(&visible),atomic_load(&background));
            if(!fault)md_acq_tick(&engine,now);
        }
        update_status(now);
        md_vehicle_status_t copy=status;
        xSemaphoreGive(lock);
        /* Never call UI under CAN mutex: fixed lock ordering. */
        if((uint32_t)(now-last_publish)>=100) {
            last_publish=now;
            for(unsigned i=0;i<MD_SIGNAL_COUNT;i++) {
                md_ui_signal_snapshot_t *s=&copy.signals[i];
                md_display_app_publish_signal(md_ui_signal_definitions[i].canonical_name,s->value,s->state,300,false);
            }
            md_ui_signal_snapshot_t speed=copy.signals[MD_SIGNAL_VEHICLE_SPEED];
            if(speed.state==MD_UI_SIGNAL_VALID) motion_latched=speed.value>0;
            md_ui_context_t context=motion_latched?MD_UI_CONTEXT_DRIVING:
                speed.state==MD_UI_SIGNAL_VALID?MD_UI_CONTEXT_PARKED:MD_UI_CONTEXT_UNKNOWN;
            (void)md_display_app_set_vehicle_context(context,context!=MD_UI_CONTEXT_UNKNOWN);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
#endif
void md_vehicle_app_start(void)
{
    lock=xSemaphoreCreateMutex();if(!lock){ESP_LOGE(TAG,"Status mutex allocation failed");return;}
#if CONFIG_MINIDASH_CAN_ENABLE
    status.configured=true;status.state=MD_ACQ_LISTEN;
    if(xTaskCreate(vehicle_task,"vehicle",6144,NULL,5,NULL)!=pdPASS) {
        status.state=MD_ACQ_FAULT;snprintf(status.fault,sizeof(status.fault),"TASK ALLOCATION");
    }
#else
    status.state=MD_ACQ_FAULT;snprintf(status.fault,sizeof(status.fault),"CAN NOT CONFIGURED");
    ESP_LOGI(TAG,"CAN disabled; provision reviewed hardware preset to enable");
#endif
}
