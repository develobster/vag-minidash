#include "vehicle_app.h"
#include "display_app.h"
#include "derived_engine.h"
#include "esp_attr.h"
#include "driver/twai.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "can_timing.h"
#include "can_error_probe.h"
#include "can_incident.h"
#include "can_link_tx.h"
#include "can_pipeline.h"
#include "power_manager.h"
#include "ui_web.h"
#include "freertos/queue.h"
#include "esp_app_desc.h"
#include "esp_intr_alloc.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

static SemaphoreHandle_t lock;
static md_acquisition_t engine;
static EXT_RAM_BSS_ATTR md_derived_engine_t analytics;
#if CONFIG_MINIDASH_CAN_ENABLE
static md_ui_signal_store_t canonical;
#endif
static md_vehicle_status_t status;
static atomic_uint visible, background;
static atomic_bool scan;
static atomic_bool analytics_dirty;
static atomic_uint analytics_reset;
static md_analytics_config_t pending_analytics;
static const char *TAG="md_vehicle";

bool md_vehicle_app_status(md_vehicle_status_t *out)
{
    if(!out || !lock || xSemaphoreTake(lock,pdMS_TO_TICKS(10))!=pdTRUE) return false;
    *out=status;xSemaphoreGive(lock);return true;
}
void md_vehicle_app_demand(uint32_t v,uint32_t b)
{ atomic_store(&visible,v);atomic_store(&background,b); }
void md_vehicle_app_scan(void) { atomic_store(&scan,true); }
bool md_vehicle_app_analytics_configure(const md_analytics_config_t *c)
{
    if(!md_analytics_config_valid(c) || !lock || xSemaphoreTake(lock,pdMS_TO_TICKS(10))!=pdTRUE)return false;
    pending_analytics=*c;atomic_store(&analytics_dirty,true);xSemaphoreGive(lock);return true;
}
void md_vehicle_app_analytics_reset(bool session){atomic_store(&analytics_reset,session?2U:1U);}
size_t md_vehicle_app_events(md_analytics_event_t *out,size_t capacity)
{
    if(!out || !lock || xSemaphoreTake(lock,pdMS_TO_TICKS(10))!=pdTRUE)return 0;
    size_t n=md_derived_events(&analytics,out,capacity);xSemaphoreGive(lock);return n;
}
size_t md_vehicle_app_trace(md_acq_trace_t *out,size_t capacity)
{
    if(!out || !lock || xSemaphoreTake(lock,pdMS_TO_TICKS(10))!=pdTRUE) return 0;
    size_t n=engine.trace_count;if(n>capacity)n=capacity;
    size_t first=(engine.trace_head+MD_ACQ_TRACE_SIZE-n)%MD_ACQ_TRACE_SIZE;
    for(size_t i=0;i<n;i++) out[i]=engine.trace[(first+i)%MD_ACQ_TRACE_SIZE];
    xSemaphoreGive(lock);return n;
}

bool md_vehicle_app_validation(md_vehicle_validation_t *out)
{
    if(!out || !lock || xSemaphoreTake(lock,pdMS_TO_TICKS(10))!=pdTRUE)return false;
    memset(out,0,sizeof(*out));out->status=status;
    out->timestamp_ms=(uint32_t)(esp_timer_get_time()/1000);
    out->validation_mode=engine.config.car_validation_mode;
    out->standard_obd=engine.config.allow_standard_obd;
    out->trace_sequence=engine.trace_sequence;out->exchange_sequence=engine.exchange_sequence;
    out->exchange_overwritten=engine.exchange_overwritten;
    memcpy(out->supported,engine.supported,sizeof(out->supported));
    out->trace_count=engine.trace_count;out->exchange_count=engine.exchange_count;
    size_t first=(engine.trace_head+MD_ACQ_TRACE_SIZE-engine.trace_count)%MD_ACQ_TRACE_SIZE;
    for(size_t i=0;i<out->trace_count;i++)out->trace[i]=engine.trace[(first+i)%MD_ACQ_TRACE_SIZE];
    first=(engine.exchange_head+MD_ACQ_EXCHANGE_SIZE-engine.exchange_count)%MD_ACQ_EXCHANGE_SIZE;
    for(size_t i=0;i<out->exchange_count;i++)out->exchanges[i]=engine.exchanges[(first+i)%MD_ACQ_EXCHANGE_SIZE];
    xSemaphoreGive(lock);return true;
}

#if CONFIG_MINIDASH_CAN_ENABLE
static twai_handle_t can;
static DRAM_ATTR md_can_ring_t rx_ring;
static QueueHandle_t rx_commands;
static SemaphoreHandle_t rx_command_done;
static bool rx_command_ok;
static unsigned rx_epoch;
static uint32_t epoch_dropped;
static atomic_uint last_valid_rx;
/* The RX task owns install/uninstall as well as receive. A mode command is
 * acknowledged only after the blocking receive has returned: no stale handle. */
typedef enum { CAN_LISTEN, CAN_NORMAL, CAN_STOP } can_command_t;
static bool normal_mode;
static uint32_t dropped_base;
static uint32_t arbitration_base, bus_error_base;
static uint32_t deferred_alerts;
static uint32_t rx_standard,rx_extended,last_rx_id;
static bool last_rx_extended;
static void read_can_status(const twai_status_info_t *info)
{
    status.rx_driver_dropped=dropped_base+info->rx_missed_count+info->rx_overrun_count;
    status.rx_ring_dropped=atomic_load(&rx_ring.dropped);
    status.rx_epoch_dropped=epoch_dropped;
    status.rx_ring_high_water=atomic_load(&rx_ring.high_water);
    status.rx_dropped=status.rx_driver_dropped+status.rx_ring_dropped+epoch_dropped;
    status.arbitration_lost=arbitration_base+info->arb_lost_count;
    status.bus_errors=bus_error_base+info->bus_error_count;
    status.tx_error_counter=info->tx_error_counter;
    status.rx_error_counter=info->rx_error_counter;
    if(status.tx_error_counter>status.peak_tx_error_counter)status.peak_tx_error_counter=status.tx_error_counter;
    if(status.rx_error_counter>status.peak_rx_error_counter)status.peak_rx_error_counter=status.rx_error_counter;
    md_can_error_counts(&status.can_errors);
}
static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time()/1000); }
static bool send_can(void *unused,const md_can_frame_t *f)
{
    (void)unused;
    if(!can || !normal_mode || f->extended || f->remote || f->dlc!=8) return false;
    twai_message_t m={.identifier=f->id,.data_length_code=8,.ss=1};
    memcpy(m.data,f->data,8);
    /* One in-flight frame, single shot, bounded wait. Queue acceptance is not
     * a successful physical transmission. Preserve unrelated driver alerts. */
    esp_err_t error=md_can_transmit_confirmed(can,&m,&deferred_alerts);
    if(error==ESP_ERR_TIMEOUT)normal_mode=false;
    return error==ESP_OK;
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
static bool install_can_owned(can_command_t command)
{
    if(can) {
        twai_status_info_t info;
        if(twai_get_status_info_v2(can,&info)==ESP_OK) {
            read_can_status(&info);
            dropped_base=status.rx_driver_dropped;
            arbitration_base=status.arbitration_lost;
            bus_error_base=status.bus_errors;
        }
        (void)twai_stop_v2(can);
        /* Account for driver-queued frames deliberately discarded by a mode
         * change as well as old-epoch frames already in the application ring. */
        if(twai_get_status_info_v2(can,&info)==ESP_OK) epoch_dropped+=info.msgs_to_rx;
        if(twai_driver_uninstall_v2(can)!=ESP_OK) return false;
        can=NULL;
    }
    normal_mode=false;
    ++rx_epoch;
    if(command==CAN_STOP) return true;
    bool normal=command==CAN_NORMAL;
    twai_general_config_t g=TWAI_GENERAL_CONFIG_DEFAULT(CONFIG_MINIDASH_CAN_TX_GPIO,
        CONFIG_MINIDASH_CAN_RX_GPIO,normal?TWAI_MODE_NORMAL:TWAI_MODE_LISTEN_ONLY);
    g.tx_queue_len=0;g.rx_queue_len=128;
#if CONFIG_TWAI_ISR_IN_IRAM
    g.intr_flags=ESP_INTR_FLAG_IRAM;
#endif
    g.alerts_enabled=TWAI_ALERT_BUS_OFF|TWAI_ALERT_RX_QUEUE_FULL|TWAI_ALERT_TX_FAILED|TWAI_ALERT_TX_SUCCESS;
    twai_timing_config_t timing=md_can_timing_500k();
    twai_filter_config_t filter=TWAI_FILTER_CONFIG_ACCEPT_ALL();
    if(twai_driver_install_v2(&g,&timing,&filter,&can)!=ESP_OK) return false;
    if(twai_start_v2(can)==ESP_OK){normal_mode=normal;return true;}
    (void)twai_driver_uninstall_v2(can);can=NULL;return false;
}
static void can_rx_task(void *unused)
{
    (void)unused;
    for(;;) {
        can_command_t command;
        if(xQueueReceive(rx_commands,&command,can?0:portMAX_DELAY)==pdTRUE) {
            rx_command_ok=install_can_owned(command);
            xSemaphoreGive(rx_command_done);
        }
        if(!can) continue;
        twai_message_t message;
        if(twai_receive_v2(can,&message,pdMS_TO_TICKS(5)+1)!=ESP_OK) {
            vTaskDelay(1); continue;
        }
        md_can_sample_t sample={.ms=now_ms(),.epoch=rx_epoch,
            .frame={.id=message.identifier,.dlc=message.data_length_code,
                .extended=message.extd,.remote=message.rtr}};
        memcpy(sample.frame.data,message.data,8);
        if(!sample.frame.remote && sample.frame.dlc<=8 &&
            sample.frame.id<=(sample.frame.extended?0x1fffffffU:0x7ffU))
            atomic_store(&last_valid_rx,sample.ms);
        (void)md_can_ring_push(&rx_ring,&sample);
    }
}
static bool set_can_mode(can_command_t command)
{
    if(xQueueSend(rx_commands,&command,portMAX_DELAY)!=pdTRUE) return false;
    if(xSemaphoreTake(rx_command_done,portMAX_DELAY)!=pdTRUE) return false;
    return rx_command_ok;
}
static bool install_can(bool normal)
{ return set_can_mode(normal?CAN_NORMAL:CAN_LISTEN); }
static void update_status(uint32_t now)
{
    status.state=engine.state;status.ecu_count=engine.ecu_count;status.ecu_id=engine.response_id;
    status.rx=engine.rx_count;status.tx=engine.tx_count;status.timeouts=engine.timeout_count;
    status.malformed=engine.malformed_count;status.rejected=engine.rejected_count;
    status.tx_failed=engine.tx_failed;status.trace_overwritten=engine.trace_overwritten;
    status.dtc_count=engine.dtc_count;status.dtc_valid=engine.dtc_valid;
    status.dtc_truncated=engine.dtc_truncated;status.dtc_ms=engine.dtc_ms;
    memcpy(status.dtc,engine.dtc,sizeof(status.dtc));memcpy(status.vin,engine.vin,sizeof(status.vin));
    md_acq_publish_canonical(&engine,&canonical,status.sources,now);
    static uint32_t last_derived;
    if((uint32_t)(now-last_derived)>=100 || engine.state==MD_ACQ_FAULT) {
        last_derived=now;md_derived_tick(&analytics,&canonical,now);
    }
    for(unsigned i=0;i<MD_SIGNAL_COUNT;i++) {
        status.signals[i]=md_ui_signal_snapshot(&canonical,i,now);
        if(md_derived_definition(i))status.sources[i]=MD_SOURCE_COUNT;
    }
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
    const md_acq_config_t config={.allow_standard_obd=allow_obd,
#if CONFIG_MINIDASH_CAR_VALIDATION_MODE
        .car_validation_mode=true,
#endif
        .startup_ms=1500,.require_bus_activity=true,.request_gap_ms=200,.profile=NULL};
    ESP_LOGI(TAG,"CAN 500 kbit/s sample=87.5%% TX=%d RX=%d standard_obd=%s VAG_profile=none UDS=off",
        CONFIG_MINIDASH_CAN_TX_GPIO,CONFIG_MINIDASH_CAN_RX_GPIO,allow_obd?"on":"off");
    xSemaphoreTake(lock,portMAX_DELAY);
    md_acq_init(&engine,&config,send_can,NULL,now_ms());
    md_acq_set_tx_ready(&engine,false);
    md_ui_signal_store_init(&canonical);(void)md_derived_init(&analytics,&pending_analytics,now_ms());
    xSemaphoreGive(lock);
    uint32_t recovery_at=0,last_publish=0;
    md_can_recovery_t recovery={0};
    bool recovering=false,validating=false;
    bool fault=false,first=true,was_quiet=false,motion_latched=false,incident_attempted=false;
    for(;;) {
        uint32_t now=now_ms();
        if(xSemaphoreTake(lock,pdMS_TO_TICKS(10))!=pdTRUE) {vTaskDelay(pdMS_TO_TICKS(10));continue;}
        if(atomic_exchange(&analytics_dirty,false))(void)md_derived_configure(&analytics,&pending_analytics,now);
        unsigned reset=atomic_exchange(&analytics_reset,0);
        if(reset==1)md_derived_reset_trip(&analytics,now);
        if(reset==2)md_derived_reset_session(&analytics,now);
        if(first) {
            first=false;status.ready=pins_valid() && install_can(false);
            if(!status.ready) {snprintf(status.fault,sizeof(status.fault),"CHECK CAN PINS/DRIVER");
                md_acq_bus_available(&engine,false,now);fault=true;
                md_can_recovery_fault(&recovery,now);recovery_at=recovery.retry_at;}
        }
        uint32_t alerts=0;
        if(can) {
            (void)twai_read_alerts_v2(can,&alerts,0);alerts|=deferred_alerts;deferred_alerts=0;
            if((alerts&TWAI_ALERT_BUS_OFF) && !fault) {status.bus_offs++;fault=true;
                recovering=validating=false;md_can_recovery_fault(&recovery,now);recovery_at=recovery.retry_at;
                status.ready=false;md_acq_bus_available(&engine,false,now);md_derived_can_error(&analytics,&canonical,now,alerts);}
        }
        twai_status_info_t info;
        if(can && twai_get_status_info_v2(can,&info)==ESP_OK) {
            read_can_status(&info);
            if(alerts&TWAI_ALERT_TX_FAILED)
                ESP_LOGW(TAG,"TX failed: arbitration=%lu bus_errors=%lu TEC=%lu REC=%lu",
                    (unsigned long)status.arbitration_lost,(unsigned long)status.bus_errors,
                    (unsigned long)status.tx_error_counter,(unsigned long)status.rx_error_counter);
        }
        bool requested=atomic_exchange(&scan,false);
        /* A manual scan cannot bypass bus-off backoff or hardware recovery. */
        if(fault && pins_valid() && (int32_t)(now-recovery_at)>=0) {
            twai_status_info_t recovery_info={0};
            bool known=can && twai_get_status_info_v2(can,&recovery_info)==ESP_OK;
            if(known && recovery_info.state==TWAI_STATE_BUS_OFF) {
                if(twai_initiate_recovery_v2(can)==ESP_OK) {recovering=true;recovery_at=now+5000;}
                else {md_can_recovery_fault(&recovery,now);recovery_at=recovery.retry_at;}
            } else if(known && recovery_info.state==TWAI_STATE_RECOVERING) {
                /* Do not reset the TEC by reinstalling a recovering controller. */
                recovery_at=now+5000;
            } else {
                status.ready=install_can(false);
                if(status.ready) {
                    fault=recovering=false;validating=true;status.fault[0]=0;
                    md_can_recovery_listen(&recovery,now,status.bus_errors);
                    md_acq_bus_available(&engine,true,now);
                } else {md_can_recovery_fault(&recovery,now);recovery_at=recovery.retry_at;}
            }
        }
        if(recovering && can) {
            twai_status_info_t recovery_info;
            if(twai_get_status_info_v2(can,&recovery_info)==ESP_OK && recovery_info.state==TWAI_STATE_STOPPED)
                recovery_at=now;
        }
        if(!fault) {
            bool quiet=engine.state==MD_ACQ_QUIET;
            if(quiet && !was_quiet) {
                status.ready=install_can(false);
                if(!status.ready) {fault=true;recovery_at=now+1000;md_acq_bus_available(&engine,false,now);}
            }
            was_quiet=quiet;
            if(requested) {
                if(quiet) {md_acq_restart(&engine,now);}
                else (void)md_acq_request_dtc(&engine,now);
            }
            if(validating && status.bus_errors!=recovery.errors)
                md_can_recovery_listen(&recovery,now,status.bus_errors);
            if(validating && md_can_recovery_valid(&recovery,now,atomic_load(&last_valid_rx),status.bus_errors)) {
                validating=false; /* attempts reset only after sustained normal operation below */
            }
            if(!fault && !validating && !normal_mode && md_acq_wants_tx(&engine,now)) {
                status.ready=install_can(true);
                if(!status.ready) {fault=true;recovery_at=now+1000;md_acq_bus_available(&engine,false,now);}
            }
            /* Bounded work and queues: a saturated bus cannot monopolise this task. */
            for(unsigned i=0;!fault && i<MD_CAN_RING_CAPACITY;i++) {
                md_can_sample_t sample;if(!md_can_ring_pop(&rx_ring,&sample))break;
                if(sample.epoch!=rx_epoch) {++epoch_dropped;continue;}
                md_can_frame_t *f=&sample.frame;
                if(f->extended)rx_extended++;else rx_standard++;
                last_rx_id=f->id;last_rx_extended=f->extended;
                if(validating && !f->remote && f->dlc<=8 &&
                    (int32_t)(sample.ms-recovery.listen_since)>=0) ++recovery.frames;
                /* Old queued replies must not revive an expired transaction. */
                md_acq_receive_at(&engine,f,sample.ms,now_ms());
            }
            now=now_ms();
            /* Standard source background collection maintains observed trip peaks.
             * Performance speed stays at the planner's 200 ms visible priority.
             * The existing 100 ms global request floor is never bypassed. */
            uint32_t stats_demand=0;
            for(unsigned id=0;id<MD_SIGNAL_RAW_COUNT;id++) stats_demand|=md_derived_raw_demand(id);
            /* The acquisition core limits validation demand to four sources. */
            md_acq_set_demand(&engine,atomic_load(&visible)|(UINT32_C(1)<<MD_SIGNAL_VEHICLE_SPEED),atomic_load(&background)|stats_demand);
            unsigned queued=atomic_load(&rx_ring.head)-atomic_load(&rx_ring.tail);
            engine.config.request_gap_ms=md_ui_web_active() || queued>MD_CAN_RING_CAPACITY/2?200U:100U;
            md_acq_set_tx_ready(&engine,normal_mode && !validating);
            if(!fault)md_acq_tick(&engine,now);
        }
        update_status(now);
        if(!incident_attempted && engine.state==MD_ACQ_QUIET && status.rx && status.tx_failed) {
            md_can_incident_t record={.schema=1,.uptime_ms=now,.rx=status.rx,.tx=status.tx,
                .tx_failed=status.tx_failed,.timeouts=status.timeouts,.arbitration=status.arbitration_lost,
                .bus_errors=status.bus_errors,.peak_tec=status.peak_tx_error_counter,.peak_rec=status.peak_rx_error_counter,
                .rx_standard=rx_standard,.rx_extended=rx_extended,.last_id=last_rx_id,.last_extended=last_rx_extended,
                .errors=status.can_errors};
            snprintf(record.firmware,sizeof(record.firmware),"%s",esp_app_get_description()->version);
            incident_attempted=true;(void)md_can_incident_save(&record);
        }
        md_ui_signal_snapshot_t speed=status.signals[MD_SIGNAL_VEHICLE_SPEED];
        if(normal_mode && recovery.attempts && now-recovery.listen_since>=60000U &&
            now-atomic_load(&last_valid_rx)<100U) recovery.attempts=0;
        if((uint32_t)(now-last_publish)>=100) md_display_app_publish_store(&canonical);
        xSemaphoreGive(lock);
        /* Never call UI under CAN mutex: fixed lock ordering. */
        if((uint32_t)(now-last_publish)>=100) {
            last_publish=now;
            md_ui_web_vehicle_speed(speed.state==MD_UI_SIGNAL_VALID && !speed.demo,
                speed.value,now-speed.age_ms);
            if(speed.state==MD_UI_SIGNAL_VALID) motion_latched=speed.value>0;
            md_ui_context_t context=motion_latched?MD_UI_CONTEXT_DRIVING:
                speed.state==MD_UI_SIGNAL_VALID?MD_UI_CONTEXT_PARKED:MD_UI_CONTEXT_UNKNOWN;
            (void)md_display_app_set_vehicle_context(context,context!=MD_UI_CONTEXT_UNKNOWN);
        }
        if(!fault && !validating && md_power_sleep_due(now_ms(),atomic_load(&last_valid_rx))) {
            if(md_power_prepare() && md_display_app_prepare_sleep(5000)) {
                /* Quiesce the sole RX owner; the timer is always a fallback. */
                xSemaphoreTake(lock,portMAX_DELAY);
                bool stopped=md_power_sleep_due(now_ms(),atomic_load(&last_valid_rx)) && set_can_mode(CAN_STOP);
                xSemaphoreGive(lock);
                if(stopped) (void)md_power_enter();
                xSemaphoreTake(lock,portMAX_DELAY);
                status.ready=install_can(false);
                xSemaphoreGive(lock);
            }
            md_display_app_cancel_sleep();
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
#endif
void md_vehicle_app_prepare(void)
{
    if(lock) return;
    lock=xSemaphoreCreateMutex();if(!lock){ESP_LOGE(TAG,"Status mutex allocation failed");return;}
    for(unsigned i=0;i<MD_SIGNAL_COUNT;i++) {
        status.signals[i].state=MD_UI_SIGNAL_UNAVAILABLE;status.sources[i]=MD_SOURCE_COUNT;
    }
    md_analytics_config_defaults(&pending_analytics);
#if CONFIG_MINIDASH_CAN_ENABLE
    md_can_ring_init(&rx_ring);atomic_init(&last_valid_rx,0);
    rx_commands=xQueueCreate(1,sizeof(can_command_t));rx_command_done=xSemaphoreCreateBinary();
    md_can_incident_dump();
    status.configured=true;status.state=MD_ACQ_LISTEN;
#else
    status.state=MD_ACQ_FAULT;snprintf(status.fault,sizeof(status.fault),"CAN NOT CONFIGURED");
    ESP_LOGI(TAG,"CAN disabled; provision reviewed hardware preset to enable");
#endif
}
void md_vehicle_app_start(void)
{
#if CONFIG_MINIDASH_CAN_ENABLE
    static bool started;
    if(!lock || started) return;
    /* UI readers already exist. Publish task-creation failure under the lock. */
    xSemaphoreTake(lock,portMAX_DELAY);
    if(rx_commands && rx_command_done &&
        xTaskCreatePinnedToCore(can_rx_task,"can_rx",4096,NULL,18,NULL,1)==pdPASS)
        started=xTaskCreatePinnedToCore(vehicle_task,"vehicle",8192,NULL,5,NULL,0)==pdPASS;
    if(!started) {
        status.state=MD_ACQ_FAULT;snprintf(status.fault,sizeof(status.fault),"TASK ALLOCATION");
    }
    xSemaphoreGive(lock);
#endif
}
