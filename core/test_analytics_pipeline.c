#include "acquisition.h"
#include "derived_engine.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"PIPELINE FAIL line %d: %s\n",__LINE__,#x);exit(1);} }while(0)
static md_acquisition_t acq;
static md_derived_engine_t analytics;
static md_ui_signal_store_t canonical;
static md_can_frame_t pending;
static bool awaiting;
static unsigned requests;
static uint32_t last_request_ms;
static bool request_seen;
static unsigned pid_requests[96];
static unsigned response_delay_ms;
static bool send_frame(void *ctx,const md_can_frame_t *f) {
    (void)ctx;
    CHECK(!awaiting);pending=*f;awaiting=true;requests++;return true;
}
static void response(const uint8_t *bytes,unsigned length,uint32_t now) {
    md_can_frame_t f={.id=0x7e8,.dlc=8};
    CHECK(length<=7);f.data[0]=length;memcpy(f.data+1,bytes,length);
    md_acq_receive(&acq,&f,now);
}
static void emulate(uint32_t now) {
    if(!awaiting)return;
    if(response_delay_ms>=500 && !acq.active) {awaiting=false;return;} /* ECU missed its deadline. */
    if((uint32_t)(now-acq.last_tx_ms)<response_delay_ms)return;
    md_can_frame_t f=pending;awaiting=false;
    if(f.data[0]==0x30)return;
    CHECK(f.data[1]==1 || f.data[1]==9);
    if(request_seen)CHECK((uint32_t)(acq.last_tx_ms-last_request_ms)>=200);
    request_seen=true;last_request_ms=acq.last_tx_ms;
    uint8_t d[7]={0x41,f.data[2],0,0,0,0,0};
    if(f.data[1]==9) {uint8_t n[]={0x7f,9,0x12};response(n,3,now);return;}
    unsigned pid=d[1];
    if(pid<96)pid_requests[pid]++;
    if(pid==0 || pid==0x20 || pid==0x40) {
        const unsigned supported[]={4,5,6,7,11,12,13,15,32,51,64,66,70,92};
        uint32_t bitmap=0;
        for(unsigned i=0;i<sizeof(supported)/sizeof(*supported);i++)
            if(supported[i]>pid && supported[i]<=pid+32)bitmap|=UINT32_C(1)<<(pid+32-supported[i]);
        for(unsigned i=0;i<4;i++)d[i+2]=bitmap>>(24-8*i);
        response(d,6,now);return;
    }
    switch(pid) {
    case 0x0c:d[2]=0x2e;d[3]=0xe0;response(d,4,now);break; /* 3000 rpm */
    case 0x0d:d[2]=now<=6000?0:now>=16000?100:(now-6000)/100;response(d,3,now);break;
    case 0x04:d[2]=128;response(d,3,now);break;
    case 0x05:d[2]=130;response(d,3,now);break;
    case 0x5c:d[2]=140;response(d,3,now);break;
    case 0x0b:d[2]=160;response(d,3,now);break;
    case 0x33:d[2]=100;response(d,3,now);break;
    case 0x42:d[2]=0x36;d[3]=0xb0;response(d,4,now);break;
    case 0x0f:d[2]=70;response(d,3,now);break;
    case 0x46:d[2]=60;response(d,3,now);break;
    case 0x06:d[2]=144;response(d,3,now);break;
    case 0x07:d[2]=112;response(d,3,now);break;
    default:CHECK(false);
    }
}
static void publish(uint32_t now) {
    md_acq_publish_canonical(&acq,&canonical,NULL,now);
    md_derived_tick(&analytics,&canonical,now);
}
static void scenario(bool all_visible,unsigned delay) {
    md_acq_config_t cfg={.allow_standard_obd=true,.startup_ms=100};
    md_acq_init(&acq,&cfg,send_frame,NULL,0);
    md_analytics_config_t settings;md_analytics_config_defaults(&settings);
    CHECK(md_derived_init(&analytics,&settings,0));md_ui_signal_store_init(&canonical);
    uint32_t all=0;
    for(unsigned id=0;id<MD_SIGNAL_RAW_COUNT;id++)all|=md_derived_raw_demand(id);
    requests=0;awaiting=false;request_seen=false;response_delay_ms=delay;memset(pid_requests,0,sizeof(pid_requests));
    md_acq_set_demand(&acq,all_visible?all:(1U<<MD_SIGNAL_VEHICLE_SPEED)|(1U<<MD_SIGNAL_ENGINE_RPM),all);
    unsigned max_gap=0,last=0,rpm_gap=0,rpm_last=0;bool seen=false,rpm_seen=false;
    for(unsigned now=0;now<=20000;now+=10) {
        emulate(now);md_acq_tick(&acq,now);
        const md_acq_sample_t *s=&acq.samples[MD_SIGNAL_VEHICLE_SPEED][MD_SOURCE_OBD];
        if(s->valid && (!seen || s->updated_ms!=last)) {
            if(seen && s->updated_ms-last>max_gap)max_gap=s->updated_ms-last;
            last=s->updated_ms;seen=true;
        }
        s=&acq.samples[MD_SIGNAL_ENGINE_RPM][MD_SOURCE_OBD];
        if(s->valid && (!rpm_seen || s->updated_ms!=rpm_last)) {
            if(rpm_seen && s->updated_ms-rpm_last>rpm_gap)rpm_gap=s->updated_ms-rpm_last;
            rpm_last=s->updated_ms;rpm_seen=true;
        }
        if(now%100==0)publish(now);
    }
    printf("PIPELINE all_visible=%u response_ms=%u speed_max_gap_ms=%u rpm_max_gap_ms=%u requests=%u\n",all_visible,delay,max_gap,rpm_gap,requests);
    md_ui_signal_snapshot_t run=md_ui_signal_snapshot(&canonical,MD_SIGNAL_PERF_0100,20000);
    if(delay>=500) {
        CHECK(acq.timeout_count>0 || acq.ecu_count==0);
        CHECK(!run.value_available);
        puts("PIPELINE overload PASS: impossible sampling budget cannot publish a timing result");
        return;
    }
    if(delay>100) {
        CHECK(max_gap<=1000);
        CHECK(run.value_available && run.value>8 && run.value<12);
        puts("PIPELINE slow ECU PASS: critical speed preserved, result remains an interpolated estimate");
        return;
    }
    CHECK(max_gap<=1000);CHECK(rpm_gap<1500);
    const unsigned standard[]={MD_SIGNAL_ENGINE_RPM,MD_SIGNAL_VEHICLE_SPEED,MD_SIGNAL_ENGINE_COOLANT_TEMPERATURE,MD_SIGNAL_ENGINE_OIL_TEMPERATURE,MD_SIGNAL_ENGINE_LOAD,MD_SIGNAL_ENGINE_MANIFOLD_PRESSURE,MD_SIGNAL_ENGINE_AMBIENT_PRESSURE,MD_SIGNAL_ECU_SUPPLY_VOLTAGE,MD_SIGNAL_IAT,MD_SIGNAL_AIR_TEMP,MD_SIGNAL_STFT,MD_SIGNAL_LTFT};
    for(unsigned i=0;i<sizeof(standard)/sizeof(*standard);i++) {
        unsigned id=standard[i];
        CHECK(md_ui_signal_snapshot(&canonical,id,20000).state==MD_UI_SIGNAL_VALID);
    }
    CHECK(pid_requests[6]>0 && pid_requests[7]>0 && pid_requests[15]>0 && pid_requests[70]>0);
    md_ui_signal_snapshot_t boost=md_ui_signal_snapshot(&canonical,MD_SIGNAL_ENGINE_BOOST,20000);
    CHECK(boost.value_available && boost.value==600);
    md_ui_signal_snapshot_t trim=md_ui_signal_snapshot(&canonical,MD_SIGNAL_TRIM_SUM,20000);
    CHECK(trim.value_available && trim.value==0);
    CHECK(run.state==MD_UI_SIGNAL_VALID && run.value_available && run.value>8.5 && run.value<11.5);
    printf("PIPELINE estimated_0_100=%.3f s\n",run.value);
    md_acq_bus_available(&acq,false,20010);publish(20100);
    CHECK(!md_ui_signal_snapshot(&canonical,MD_SIGNAL_PERF_0100,20100).value_available);
    CHECK(!md_ui_signal_snapshot(&canonical,MD_SIGNAL_BOOST_PEAK,20100).value_available);
    md_acq_bus_available(&acq,true,20200);
    for(unsigned now=20200;now<=30000;now+=10) {
        emulate(now);md_acq_tick(&acq,now);if(now%100==0)publish(now);
    }
    CHECK(acq.state==MD_ACQ_RUNNING && !acq.profile_matched);
    CHECK(md_ui_signal_snapshot(&canonical,MD_SIGNAL_ENGINE_BOOST,30000).state==MD_UI_SIGNAL_VALID);
    CHECK(!md_ui_signal_snapshot(&canonical,MD_SIGNAL_PERF_0100,30000).value_available);
    CHECK(md_ui_signal_snapshot(&canonical,MD_SIGNAL_RETARD_WORST,30000).state==MD_UI_SIGNAL_UNSUPPORTED);
    md_acq_publish_canonical(NULL,&canonical,NULL,30000);
    md_acq_publish_canonical(&acq,NULL,NULL,30000);
}
int main(void) {
    scenario(false,10);scenario(true,10);scenario(true,300);scenario(true,600);
    puts("PIPELINE PASS: supported-PID discovery → codecs → canonical → analytics, loss invalidation");
    return 0;
}
