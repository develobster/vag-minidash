#include "validation_json.h"
#include <math.h>

static const char *quality(md_ui_signal_state_t state)
{
    static const char *const names[]={"valid","stale","unavailable","unsupported","error"};
    return (unsigned)state<sizeof(names)/sizeof(names[0])?names[state]:"error";
}
static const char *source(md_source_t s)
{
    static const char *const names[]={"passive","uds","obd"};
    return (unsigned)s<MD_SOURCE_COUNT?names[s]:"none";
}
static void raw(cJSON *row, const uint8_t *data, size_t count)
{
    char hex[MD_DIAG_PAYLOAD_MAX*2+1];const char *digits="0123456789abcdef";
    if(count>MD_DIAG_PAYLOAD_MAX)count=MD_DIAG_PAYLOAD_MAX;
    for(size_t i=0;i<count;i++){hex[i*2]=digits[data[i]>>4];hex[i*2+1]=digits[data[i]&15];}
    hex[count*2]=0;cJSON_AddStringToObject(row,"raw",hex);
}
cJSON *md_validation_json(const md_vehicle_validation_t *v, const char *version, uint64_t uptime)
{
    if(!v)return NULL;
    cJSON *root=cJSON_CreateObject();
    cJSON_AddNumberToObject(root,"schema",1);
    cJSON_AddStringToObject(root,"firmware_version",version?version:"");
    cJSON_AddNumberToObject(root,"uptime_ms",(double)uptime);
    cJSON_AddNumberToObject(root,"timestamp_ms",v->timestamp_ms);
    cJSON_AddBoolToObject(root,"validation_mode",v->validation_mode);
    cJSON_AddBoolToObject(root,"standard_obd",v->standard_obd);
    cJSON_AddBoolToObject(root,"vehicle_validated",false);
    cJSON_AddStringToObject(root,"state",md_acq_state_name(v->status.state));
    cJSON_AddStringToObject(root,"fault",v->status.fault);
    cJSON_AddStringToObject(root,"vin",v->status.vin);
    cJSON_AddNumberToObject(root,"ecu_response_id",v->status.ecu_id);
    cJSON_AddNumberToObject(root,"rx",v->status.rx);cJSON_AddNumberToObject(root,"tx",v->status.tx);
    cJSON_AddNumberToObject(root,"timeouts",v->status.timeouts);
    cJSON_AddNumberToObject(root,"malformed",v->status.malformed);
    cJSON_AddNumberToObject(root,"rejected",v->status.rejected);
    cJSON_AddNumberToObject(root,"tx_failed",v->status.tx_failed);
    cJSON_AddNumberToObject(root,"arbitration_lost",v->status.arbitration_lost);
    cJSON_AddNumberToObject(root,"bus_errors",v->status.bus_errors);
    cJSON_AddNumberToObject(root,"tx_error_counter",v->status.tx_error_counter);
    cJSON_AddNumberToObject(root,"rx_error_counter",v->status.rx_error_counter);
    cJSON_AddNumberToObject(root,"peak_tx_error_counter",v->status.peak_tx_error_counter);
    cJSON_AddNumberToObject(root,"peak_rx_error_counter",v->status.peak_rx_error_counter);
    cJSON_AddNumberToObject(root,"bus_offs",v->status.bus_offs);
    cJSON_AddNumberToObject(root,"rx_dropped",v->status.rx_dropped);
    cJSON_AddNumberToObject(root,"trace_overwritten",v->status.trace_overwritten);
    cJSON_AddNumberToObject(root,"exchange_overwritten",v->exchange_overwritten);
    cJSON_AddNumberToObject(root,"trace_sequence",v->trace_sequence);
    cJSON_AddNumberToObject(root,"exchange_sequence",v->exchange_sequence);
    cJSON *supported=cJSON_AddArrayToObject(root,"supported_pid_bitmaps");
    for(unsigned i=0;i<3;i++)cJSON_AddItemToArray(supported,cJSON_CreateNumber(v->supported[i]));
    cJSON *frames=cJSON_AddArrayToObject(root,"frames");
    for(unsigned i=0;i<v->trace_count && i<MD_ACQ_TRACE_SIZE;i++) {
        const md_acq_trace_t *f=&v->trace[i];cJSON *r=cJSON_CreateObject();
        cJSON_AddNumberToObject(r,"sequence",f->sequence);cJSON_AddNumberToObject(r,"timestamp_ms",f->ms);
        cJSON_AddStringToObject(r,"direction",f->tx?"tx":"rx");
        cJSON_AddNumberToObject(r,"id",f->frame.id);cJSON_AddNumberToObject(r,"dlc",f->frame.dlc);
        cJSON_AddBoolToObject(r,"extended",f->frame.extended);cJSON_AddBoolToObject(r,"remote",f->frame.remote);
        raw(r,f->frame.data,f->frame.dlc<=8?f->frame.dlc:8);cJSON_AddItemToArray(frames,r);
    }
    cJSON *exchanges=cJSON_AddArrayToObject(root,"exchanges");
    static const char *const results[]={"ok","timeout","negative_response","malformed","tx_failed","bus_fault"};
    for(unsigned i=0;i<v->exchange_count && i<MD_ACQ_EXCHANGE_SIZE;i++) {
        const md_acq_exchange_t *e=&v->exchanges[i];cJSON *r=cJSON_CreateObject();
        cJSON_AddNumberToObject(r,"sequence",e->sequence);cJSON_AddNumberToObject(r,"request_ms",e->request_ms);
        cJSON_AddNumberToObject(r,"response_ms",e->response_ms);cJSON_AddNumberToObject(r,"response_time_ms",e->elapsed_ms);
        cJSON_AddNumberToObject(r,"request_id",e->request_id);cJSON_AddNumberToObject(r,"response_id",e->response_id);
        cJSON_AddNumberToObject(r,"service",e->service);cJSON_AddNumberToObject(r,"parameter",e->parameter);
        cJSON_AddNumberToObject(r,"nrc",e->nrc);
        cJSON_AddStringToObject(r,"result",(unsigned)e->result<sizeof(results)/sizeof(results[0])?results[e->result]:"error");
        raw(r,e->raw,e->raw_length);cJSON_AddStringToObject(r,"source",source(e->source));
        bool valid=e->value_available && isfinite(e->value) && e->signal<MD_SIGNAL_RAW_COUNT;
        cJSON_AddStringToObject(r,"validity",valid?"valid":"unavailable");
        if(e->signal<MD_SIGNAL_RAW_COUNT)cJSON_AddStringToObject(r,"signal",md_ui_signal_definitions[e->signal].canonical_name);
        if(valid)cJSON_AddNumberToObject(r,"value",e->value);else cJSON_AddNullToObject(r,"value");
        cJSON_AddItemToArray(exchanges,r);
    }
    cJSON *signals=cJSON_AddArrayToObject(root,"signals");
    for(unsigned i=0;i<MD_SIGNAL_COUNT;i++) {
        const md_ui_signal_snapshot_t *s=&v->status.signals[i];cJSON *r=cJSON_CreateObject();
        cJSON_AddStringToObject(r,"signal",md_ui_signal_definitions[i].canonical_name);
        cJSON_AddStringToObject(r,"unit",md_ui_signal_definitions[i].canonical_unit);
        cJSON_AddStringToObject(r,"validity",quality(s->state));
        cJSON_AddStringToObject(r,"source",md_derived_definition(i)?"derived":source(v->status.sources[i]));
        cJSON_AddNumberToObject(r,"age_ms",s->age_ms);
        if(s->state==MD_UI_SIGNAL_VALID && s->value_available && isfinite(s->value))cJSON_AddNumberToObject(r,"value",s->value);
        else cJSON_AddNullToObject(r,"value");
        cJSON_AddItemToArray(signals,r);
    }
    return root;
}
