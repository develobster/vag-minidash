#include "acquisition.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define BIT(s) (UINT32_C(1) << (s))
#define RESPONSE_MS 500U
#define REQUEST_GAP_MS 100U
#define RETRY_LIMIT 3U
#define IDLE_QUIET_MS 120000U

typedef struct { uint8_t pid, bytes; md_ui_signal_id_t signal;
    uint32_t fast_ms, ttl_ms; } obd_signal_t;
static const obd_signal_t obd[] = {
    {0x0c, 2, MD_SIGNAL_ENGINE_RPM, 200, 1500},
    {0x0d, 1, MD_SIGNAL_VEHICLE_SPEED, 200, 1500},
    {0x05, 1, MD_SIGNAL_ENGINE_COOLANT_TEMPERATURE, 2000, 10000},
    {0x04, 1, MD_SIGNAL_ENGINE_LOAD, 1000, 5000},
    {0x0b, 1, MD_SIGNAL_ENGINE_MANIFOLD_PRESSURE, 500, 3000},
    {0x33, 1, MD_SIGNAL_ENGINE_AMBIENT_PRESSURE, 2000, 15000},
    {0x42, 2, MD_SIGNAL_ECU_SUPPLY_VOLTAGE, 2000, 10000},
    {0x5c, 1, MD_SIGNAL_ENGINE_OIL_TEMPERATURE, 2000, 10000},
};
#define OBD_COUNT (sizeof(obd) / sizeof(obd[0]))
_Static_assert(MD_SIGNAL_COUNT<=32, "Demand mask capacity");

static bool elapsed(uint32_t now, uint32_t since, uint32_t duration)
{ return (uint32_t)(now - since) >= duration; }
static bool due(uint32_t now, uint32_t deadline)
{ return (int32_t)(now - deadline) >= 0; }
static bool signal_valid(int id) { return id >= 0 && id < MD_SIGNAL_COUNT; }
static bool fresh(const md_acq_sample_t *s, uint32_t now)
{ return s->valid && !elapsed(now, s->updated_ms, s->ttl_ms); }
static uint32_t be32(const uint8_t *b)
{ return ((uint32_t)b[0]<<24)|((uint32_t)b[1]<<16)|((uint32_t)b[2]<<8)|b[3]; }
static bool has_pid(const md_acquisition_t *a, unsigned pid)
{ return pid > 0 && pid <= 96 && (a->supported[(pid-1)/32] & (UINT32_C(1) << (31-(pid-1)%32))); }

bool md_acq_profile_valid(const md_vehicle_profile_t *p)
{
    if (!p || !p->vin || strlen(p->vin)!=17 || !p->provenance || !p->provenance[0] ||
        !p->source_license || !p->source_license[0] || !p->identity_did ||
        !p->identity_length || p->identity_length>sizeof(p->identity_expected) ||
        !p->addressing_verified || !p->definitions_verified ||
        p->request_id<0x700 || p->request_id>0x7e7 || p->request_id==0x7df ||
        p->response_id<0x700 || p->response_id>0x7ff || p->response_id==p->request_id ||
        p->passive_count>MD_ACQ_MAX_BINDINGS || p->uds_count>MD_ACQ_MAX_BINDINGS ||
        (p->passive_count && !p->passive) || (p->uds_count && !p->uds)) return false;
    for (size_t i=0;i<p->passive_count;i++) {
        const md_passive_binding_t *b=&p->passive[i];
        if (b->id>0x7ff || b->dlc<1 || b->dlc>8 || !b->bit_count || b->bit_count>32 ||
            (unsigned)b->start_bit+b->bit_count>b->dlc*8U || !signal_valid(b->signal) ||
            !isfinite(b->scale) || !isfinite(b->offset) || !b->ttl_ms || b->ttl_ms>60000) return false;
        for(size_t j=0;j<i;j++) if(p->passive[j].signal==b->signal) return false;
    }
    for (size_t i=0;i<p->uds_count;i++) {
        const md_uds_binding_t *b=&p->uds[i];
        if (!b->bytes || b->bytes>4 || !signal_valid(b->signal) ||
            !isfinite(b->scale) || !isfinite(b->offset) || b->period_ms<100 ||
            b->period_ms>60000 || b->ttl_ms<b->period_ms || b->ttl_ms>60000) return false;
        for(size_t j=0;j<i;j++) if(p->uds[j].signal==b->signal) return false;
    }
    return true;
}
static void trace(md_acquisition_t *a, const md_can_frame_t *f, uint32_t now, bool tx)
{
    a->trace[a->trace_head]=(md_acq_trace_t){.frame=*f,.ms=now,.tx=tx};
    a->trace_head=(a->trace_head+1)%MD_ACQ_TRACE_SIZE;
    if (a->trace_count<MD_ACQ_TRACE_SIZE) a->trace_count++;
    else a->trace_overwritten++;
}
static bool send_frame(md_acquisition_t *a, const md_can_frame_t *f, uint32_t now)
{
    if (!a->bus_available || !a->config.allow_standard_obd || !a->send ||
        a->state==MD_ACQ_QUIET || a->state==MD_ACQ_FAULT) return false;
    /* Application requests and authenticated ISO-TP FC share one TX gate. */
    a->last_tx_ms=now; a->sent_any=true;
    if (!a->send(a->send_context,f)) { a->tx_failed++; return false; }
    trace(a,f,now,true); a->tx_count++; return true;
}
static void quiet(md_acquisition_t *a, uint32_t now)
{
    a->active=false; md_tp_reset(&a->tp); a->state=MD_ACQ_QUIET;
    a->quiet_ms=now; a->dtc_requested=false;
}
static void invalidate(md_acquisition_t *a)
{
    memset(a->samples,0,sizeof(a->samples));
    a->vehicle_matched=a->profile_matched=false; a->vin[0]=0; a->dtc_valid=false;
}
void md_acq_restart(md_acquisition_t *a, uint32_t now)
{
    invalidate(a); a->active=false; a->state=MD_ACQ_LISTEN; a->started_ms=now;
    a->attempts=a->ecu_count=a->consecutive_failures=a->cap_block=0;
    a->request_id=a->response_id=0; a->vin_attempted=false; a->dtc_requested=false;
    a->engine_idle=false;
    memset(a->responders,0,sizeof(a->responders)); memset(a->supported,0,sizeof(a->supported));
    memset(a->failures,0,sizeof(a->failures)); memset(a->uds_failures,0,sizeof(a->uds_failures));
    for (unsigned i=0;i<MD_ACQ_MAX_BINDINGS;i++) a->due[i]=a->uds_due[i]=now;
    md_tp_reset(&a->tp);
}
void md_acq_init(md_acquisition_t *a, const md_acq_config_t *c,
                 md_acq_send_t send, void *ctx, uint32_t now)
{
    memset(a,0,sizeof(*a)); a->config=*c; a->send=send; a->send_context=ctx;
    a->bus_available=true; md_acq_restart(a,now);
    a->background=BIT(MD_SIGNAL_ENGINE_RPM)|BIT(MD_SIGNAL_VEHICLE_SPEED);
}
void md_acq_bus_available(md_acquisition_t *a, bool available, uint32_t now)
{
    if (a->bus_available==available) return;
    a->bus_available=available;
    if (available) md_acq_restart(a,now);
    else { a->active=false; a->state=MD_ACQ_FAULT; invalidate(a); md_tp_reset(&a->tp); }
}
void md_acq_set_demand(md_acquisition_t *a, uint32_t visible, uint32_t background)
{
    /* RPM/speed maintain vehicle context even on non-telemetry pages. */
    a->visible=visible; a->background=background|BIT(MD_SIGNAL_ENGINE_RPM)|BIT(MD_SIGNAL_VEHICLE_SPEED);
    if ((visible|background)&BIT(MD_SIGNAL_ENGINE_BOOST)) {
        if (visible&BIT(MD_SIGNAL_ENGINE_BOOST)) a->visible|=BIT(MD_SIGNAL_ENGINE_MANIFOLD_PRESSURE)|BIT(MD_SIGNAL_ENGINE_AMBIENT_PRESSURE);
        a->background|=BIT(MD_SIGNAL_ENGINE_MANIFOLD_PRESSURE)|BIT(MD_SIGNAL_ENGINE_AMBIENT_PRESSURE);
    }
}
static void publish(md_acquisition_t *a, md_ui_signal_id_t id, md_source_t source,
                    float value, uint32_t ttl, uint32_t now)
{
    if (signal_valid(id) && isfinite(value))
        a->samples[id][source]=(md_acq_sample_t){value,now,ttl,true};
}
md_ui_signal_snapshot_t md_acq_snapshot(const md_acquisition_t *a,
    md_ui_signal_id_t id, uint32_t now, md_source_t *source)
{
    md_ui_signal_snapshot_t r={.state=MD_UI_SIGNAL_UNAVAILABLE};
    if(source) *source=MD_SOURCE_COUNT;
    if (!signal_valid(id)) return r;
    if (id==MD_SIGNAL_ENGINE_BOOST) {
        md_ui_signal_snapshot_t map=md_acq_snapshot(a,MD_SIGNAL_ENGINE_MANIFOLD_PRESSURE,now,NULL);
        md_ui_signal_snapshot_t bar=md_acq_snapshot(a,MD_SIGNAL_ENGINE_AMBIENT_PRESSURE,now,NULL);
        if (map.state==MD_UI_SIGNAL_VALID && bar.state==MD_UI_SIGNAL_VALID && bar.age_ms<=5000) {
            r=map; r.value=map.value-bar.value;
            if (bar.age_ms>r.age_ms) r.age_ms=bar.age_ms;
        }
        return r;
    }
    for (int s=0;s<MD_SOURCE_COUNT;s++) {
        const md_acq_sample_t *v=&a->samples[id][s];
        if (fresh(v,now)) {
            r.state=MD_UI_SIGNAL_VALID; r.value=v->value; r.value_available=true;
            r.age_ms=now-v->updated_ms; if (source) *source=(md_source_t)s; return r;
        }
        if (v->valid) r.state=MD_UI_SIGNAL_STALE;
    }
    if (r.state==MD_UI_SIGNAL_UNAVAILABLE && a->state==MD_ACQ_RUNNING) {
        bool supported=false;
        for (unsigned i=0;i<OBD_COUNT;i++) if (obd[i].signal==id && has_pid(a,obd[i].pid) && a->failures[i]<RETRY_LIMIT) supported=true;
        if (a->profile_matched) {
            const md_vehicle_profile_t *p=a->config.profile;
            for(size_t i=0;i<p->passive_count;i++) if(p->passive[i].signal==id) supported=true;
            for(size_t i=0;i<p->uds_count;i++) if(p->uds[i].signal==id && a->uds_failures[i]<RETRY_LIMIT) supported=true;
        }
        if (!supported) r.state=MD_UI_SIGNAL_UNSUPPORTED;
    }
    return r;
}
static bool begin(md_acquisition_t *a, uint8_t service, uint16_t pid,
                  uint16_t tx, uint16_t rx, uint32_t now)
{
    /* No write/clear/session/security/routine/tester-present services. */
    bool standard=(service==1 || service==3 || service==9) &&
        ((tx==0x7df && service==1 && pid==0) ||
         (a->response_id>=0x7e8 && a->response_id<=0x7ef && tx==a->request_id && rx==a->response_id));
    const md_vehicle_profile_t *p=a->config.profile;
    bool uds=service==0x22 && md_acq_profile_valid(p) &&
        (a->profile_matched || (a->vehicle_matched && a->state==MD_ACQ_VERIFY_ECU && pid==p->identity_did)) &&
        tx==p->request_id && rx==p->response_id;
    if (!standard && !uds) { a->rejected_count++; return false; }
    md_can_frame_t f={.id=tx,.dlc=8};
    f.data[0]=service==3?1:uds?3:2; f.data[1]=service;
    f.data[2]=uds?(pid>>8):(uint8_t)pid; if(uds) f.data[3]=(uint8_t)pid;
    a->service=service; a->pid=(uint8_t)pid; a->active_rx=rx; a->active_tx=tx;
    a->did_request=uds; a->request_ms=a->progress_ms=now; a->pending_count=0;
    md_tp_reset(&a->tp); a->active=send_frame(a,&f,now); return a->active;
}
static bool response_matches(const md_acquisition_t *a,const uint8_t *d,size_t n)
{
    if (n==3 && d[0]==0x7f && d[1]==a->service) return true;
    if (n<1 || d[0]!=(uint8_t)(a->service+0x40)) return false;
    if (a->service==3) return true;
    if (a->did_request) {
        uint16_t did=a->state==MD_ACQ_VERIFY_ECU?a->config.profile->identity_did:a->config.profile->uds[a->job].did;
        return n>=3 && d[1]==(did>>8) && d[2]==(did&255);
    }
    return n>=2 && d[1]==a->pid;
}
static void fail(md_acquisition_t *a, uint32_t now, bool unsupported)
{
    a->active=false; md_tp_reset(&a->tp);
    if (a->state==MD_ACQ_RUNNING && a->service!=3) {
        uint8_t *f=a->did_request?&a->uds_failures[a->job]:&a->failures[a->job];
        if (*f<RETRY_LIMIT) (*f)++;
        if (unsupported) { *f=RETRY_LIMIT; a->consecutive_failures=0; }
        else if (++a->consecutive_failures>=RETRY_LIMIT) quiet(a,now);
    } else if (a->state==MD_ACQ_VIN || a->state==MD_ACQ_VERIFY_ECU) a->state=MD_ACQ_RUNNING;
    else if (a->state==MD_ACQ_CAPABILITIES) {
        if (++a->consecutive_failures>=RETRY_LIMIT) { a->state=MD_ACQ_VIN; a->consecutive_failures=0; }
    }
}
static void complete(md_acquisition_t *a, uint32_t now)
{
    const uint8_t *d=a->tp.data; size_t n=a->tp.length;
    if (!response_matches(a,d,n)) { a->rejected_count++; return; }
    if (d[0]==0x7f) {
        if (d[2]==0x78 && ++a->pending_count<=3 && !elapsed(now,a->request_ms,2000)) {
            a->progress_ms=now; md_tp_reset(&a->tp); return;
        }
        fail(a,now,d[2]==0x11 || d[2]==0x12 || d[2]==0x31); return;
    }
    bool ok=false;
    if (a->state==MD_ACQ_CAPABILITIES && n==6) {
        a->supported[a->cap_block]=be32(d+2); a->cap_block++;
        if (a->cap_block>=3 || !(a->supported[a->cap_block-1]&1)) a->state=MD_ACQ_VIN;
        ok=true;
    } else if (a->state==MD_ACQ_VIN) {
        if (n==20 && d[2]==1) {
            ok=true;
            for(unsigned i=0;i<17;i++) {
                uint8_t c=d[3+i];
                if (!((c>='0' && c<='9') || (c>='A' && c<='Z' && c!='I' && c!='O' && c!='Q'))) ok=false;
            }
            if(ok) { memcpy(a->vin,d+3,17); a->vin[17]=0;
                a->vehicle_matched=md_acq_profile_valid(a->config.profile) && !strcmp(a->vin,a->config.profile->vin); }
        }
        a->state=a->vehicle_matched?MD_ACQ_VERIFY_ECU:MD_ACQ_RUNNING;
    } else if (a->state==MD_ACQ_VERIFY_ECU) {
        const md_vehicle_profile_t *p=a->config.profile;
        a->profile_matched=n==3U+p->identity_length && !memcmp(d+3,p->identity_expected,p->identity_length);
        a->state=MD_ACQ_RUNNING;ok=true;
    } else if (a->service==3 && n>=2 && n==2U+(size_t)d[1]*2U) {
        a->dtc_count=d[1]>MD_ACQ_DTC_MAX?MD_ACQ_DTC_MAX:d[1];
        a->dtc_truncated=d[1]>MD_ACQ_DTC_MAX;
        for(unsigned i=0;i<a->dtc_count;i++) a->dtc[i]=((uint16_t)d[2+i*2]<<8)|d[3+i*2];
        a->dtc_valid=true; a->dtc_ms=now; ok=true;
    } else if (a->did_request) {
        const md_uds_binding_t *b=&a->config.profile->uds[a->job];
        if(n==3U+b->bytes) { uint32_t v=0; for(unsigned i=0;i<b->bytes;i++) v=(v<<8)|d[3+i];
            publish(a,b->signal,MD_SOURCE_UDS,v*b->scale+b->offset,b->ttl_ms,now);
            a->uds_failures[a->job]=0; ok=true; }
    } else if (a->service==1 && a->job<OBD_COUNT && n==2U+obd[a->job].bytes) {
        const obd_signal_t *b=&obd[a->job]; float v=d[2];
        switch(b->pid) {
        case 0x0c: v=(((unsigned)d[2]<<8)|d[3])/4.0f;break;
        case 0x05: case 0x5c: v-=40;break;
        case 0x04: v=v*100.0f/255.0f;break;
        case 0x0b: case 0x33: v*=10;break; /* kPa -> canonical hPa */
        case 0x42: v=(((unsigned)d[2]<<8)|d[3])/1000.0f;break;
        default: break;
        }
        publish(a,b->signal,MD_SOURCE_OBD,v,b->ttl_ms,now);a->failures[a->job]=0;ok=true;
    }
    if(ok) { a->active=false;a->consecutive_failures=0;md_tp_reset(&a->tp); }
    else { a->malformed_count++;fail(a,now,false); }
}
void md_acq_receive(md_acquisition_t *a,const md_can_frame_t *f,uint32_t now)
{
    if (!f || f->dlc>8 || f->id>(f->extended?0x1fffffff:0x7ff)) { a->malformed_count++;return; }
    a->rx_count++;trace(a,f,now,false);
    /* Bus availability after a genuine quiet gap is NOT ignition evidence. */
    if (f->id<0x700 && !f->remote && !f->extended) {
        if (a->state==MD_ACQ_QUIET && elapsed(now,a->quiet_ms,5000)) md_acq_restart(a,now);
        a->quiet_ms=now;
    }
    if (a->profile_matched && !f->remote && !f->extended) {
        const md_vehicle_profile_t *p=a->config.profile;
        for(size_t i=0;i<p->passive_count;i++) {
            const md_passive_binding_t *b=&p->passive[i];
            if(f->id!=b->id || f->dlc!=b->dlc) continue;
            uint64_t raw=0;for(unsigned k=0;k<f->dlc;k++) raw|=(uint64_t)f->data[k]<<(8*k);
            uint64_t mask=(UINT64_C(1)<<b->bit_count)-1; raw=(raw>>b->start_bit)&mask;
            int64_t value=(int64_t)raw;
            if(b->signed_value && (raw&(UINT64_C(1)<<(b->bit_count-1)))) value-=(INT64_C(1)<<b->bit_count);
            publish(a,b->signal,MD_SOURCE_PASSIVE,value*b->scale+b->offset,b->ttl_ms,now);
        }
    }
    if (!a->active || f->extended || f->remote) return;
    if(a->state==MD_ACQ_DISCOVERY) {
        if (f->id>=0x7e8 && f->id<=0x7ef && f->dlc==8 && f->data[0]==6 && f->data[1]==0x41 && f->data[2]==0) {
            unsigned i=f->id-0x7e8;uint32_t bits=be32(f->data+3);
            if (!a->responders[i] && bits) a->ecu_count++;
            a->responders[i]=bits;
        }
        return;
    }
    if(f->id!=a->active_rx) return;
    /* Authenticate service/PID/DID AND address before sending flow control. */
    uint8_t type=f->dlc?f->data[0]>>4:255;
    if ((type==0 || type==1) && f->dlc==8 &&
        !response_matches(a,f->data+(type==0?1:2),type==0?((f->data[0]&15)<=7?(f->data[0]&15):0):6)) {
        a->rejected_count++;return;
    }
    md_tp_result_t r=md_tp_receive(&a->tp,f,now);
    if(r==MD_TP_FLOW) {
        md_can_frame_t fc={.id=a->active_tx,.dlc=8,.data={0x30,0,10,0,0,0,0,0}};
        if(!send_frame(a,&fc,now)) fail(a,now,false);
    } else if(r==MD_TP_COMPLETE) complete(a,now);
    else if(r==MD_TP_INVALID) {a->malformed_count++;fail(a,now,false);}
}
bool md_acq_request_dtc(md_acquisition_t *a,uint32_t now)
{
    md_ui_signal_snapshot_t speed=md_acq_snapshot(a,MD_SIGNAL_VEHICLE_SPEED,now,NULL);
    if(a->state!=MD_ACQ_RUNNING || a->dtc_requested || speed.state!=MD_UI_SIGNAL_VALID || speed.value!=0) return false;
    a->dtc_valid=false;a->dtc_requested=true;return true;
}
void md_acq_format_dtc(uint16_t code,char text[6])
{ (void)snprintf(text,6,"%c%04X","PCBU"[code>>14],code&0x3fff); }
void md_acq_tick(md_acquisition_t *a,uint32_t now)
{
    if(!a->bus_available || a->state==MD_ACQ_FAULT || a->state==MD_ACQ_QUIET || !a->config.allow_standard_obd) return;
    if(a->state==MD_ACQ_RUNNING) {
        md_ui_signal_snapshot_t rpm=md_acq_snapshot(a,MD_SIGNAL_ENGINE_RPM,now,NULL);
        md_ui_signal_snapshot_t speed=md_acq_snapshot(a,MD_SIGNAL_VEHICLE_SPEED,now,NULL);
        if(rpm.state==MD_UI_SIGNAL_VALID && rpm.value==0 && speed.state==MD_UI_SIGNAL_VALID && speed.value==0) {
            if(!a->engine_idle) {a->engine_idle=true;a->idle_since_ms=now;}
            else if(elapsed(now,a->idle_since_ms,IDLE_QUIET_MS)) {quiet(a,now);return;}
        } else a->engine_idle=false;
    }
    if(a->active) {
        if(a->state==MD_ACQ_DISCOVERY && elapsed(now,a->request_ms,RESPONSE_MS)) {
            a->active=false;
            for(unsigned i=0;i<8;i++) if(a->responders[i] & (UINT32_C(1)<<(32-0x0c))) {
                a->response_id=0x7e8+i; a->request_id=0x7e0+i;
                a->supported[0]=a->responders[i];a->cap_block=1;
                a->state=(a->supported[0]&1)?MD_ACQ_CAPABILITIES:MD_ACQ_VIN;return;
            }
            if(a->attempts>=RETRY_LIMIT) quiet(a,now);
        } else if(a->state!=MD_ACQ_DISCOVERY &&
                  (md_tp_expired(&a->tp,now) || elapsed(now,a->progress_ms,RESPONSE_MS) || elapsed(now,a->request_ms,2000))) {
            a->timeout_count++;fail(a,now,false);
        }
        return;
    }
    if(a->sent_any && !elapsed(now,a->last_tx_ms,REQUEST_GAP_MS)) return;
    if(a->state==MD_ACQ_LISTEN) {
        if(!elapsed(now,a->started_ms,a->config.startup_ms)) return;
        a->state=MD_ACQ_DISCOVERY;
    }
    if(a->state==MD_ACQ_DISCOVERY) {
        if(a->attempts>=RETRY_LIMIT) {quiet(a,now);return;}
        a->attempts++;(void)begin(a,1,0,0x7df,0,now);return;
    }
    if(a->state==MD_ACQ_CAPABILITIES) {
        if(!begin(a,1,a->cap_block*32,a->request_id,a->response_id,now) && ++a->consecutive_failures>=RETRY_LIMIT) quiet(a,now);
        return;
    }
    if(a->state==MD_ACQ_VIN) {
        if(a->vin_attempted) {a->state=MD_ACQ_RUNNING;return;}
        a->vin_attempted=true;
        if(!begin(a,9,2,a->request_id,a->response_id,now)) a->state=MD_ACQ_RUNNING;
        return;
    }
    if(a->state==MD_ACQ_VERIFY_ECU) {
        const md_vehicle_profile_t *p=a->config.profile;
        if(!begin(a,0x22,p->identity_did,p->request_id,p->response_id,now)) a->state=MD_ACQ_RUNNING;
        return;
    }
    if(a->dtc_requested) {
        a->dtc_requested=false;
        md_ui_signal_snapshot_t speed=md_acq_snapshot(a,MD_SIGNAL_VEHICLE_SPEED,now,NULL);
        if(speed.state==MD_UI_SIGNAL_VALID && speed.value==0) {(void)begin(a,3,0,a->request_id,a->response_id,now);return;}
    }
    /* Round-robin among due jobs. Visible > background rate; fresh passive > UDS > OBD. */
    unsigned uds_count=a->profile_matched?(unsigned)a->config.profile->uds_count:0;
    unsigned total=(unsigned)OBD_COUNT+uds_count;
    for(unsigned n=0;n<total;n++) {
        unsigned i=a->cursor%total; a->cursor=(a->cursor+1)%total; a->job=i<OBD_COUNT?i:i-OBD_COUNT;
        bool is_uds=i>=OBD_COUNT;
        md_ui_signal_id_t signal=is_uds?a->config.profile->uds[a->job].signal:obd[i].signal;
        if(!((a->visible|a->background)&BIT(signal))) continue;
        if(!is_uds && (!has_pid(a,obd[i].pid) || a->failures[i]>=RETRY_LIMIT)) continue;
        if(is_uds && a->uds_failures[a->job]>=RETRY_LIMIT) continue;
        if(fresh(&a->samples[signal][MD_SOURCE_PASSIVE],now) || (!is_uds && fresh(&a->samples[signal][MD_SOURCE_UDS],now))) continue;
        uint32_t *when=is_uds?&a->uds_due[a->job]:&a->due[i];
        if(!due(now,*when)) continue;
        uint32_t period=is_uds?a->config.profile->uds[a->job].period_ms:obd[i].fast_ms;
        if(!(a->visible&BIT(signal)) && period<1000) period=1000;
        *when=now+period;
        bool sent=is_uds?begin(a,0x22,a->config.profile->uds[a->job].did,
            a->config.profile->request_id,a->config.profile->response_id,now):begin(a,1,obd[i].pid,a->request_id,a->response_id,now);
        if(!sent && ++a->consecutive_failures>=RETRY_LIMIT) quiet(a,now);
        return;
    }
}
const char *md_acq_state_name(md_acq_state_t s)
{
    static const char *const names[]={"LISTEN","DISCOVERY","CAPABILITIES","IDENTIFY","VERIFY ECU","LIVE","QUIET","CAN FAULT"};
    return (unsigned)s<sizeof(names)/sizeof(names[0])?names[s]:"UNKNOWN";
}
