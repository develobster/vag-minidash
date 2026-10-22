#include "validation_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"VALIDATION FAIL %d: %s\n",__LINE__,#x);exit(1);} }while(0)
static md_acquisition_t acq;
static md_can_frame_t last;
static unsigned sent;
static bool reject_tx;
static bool send_frame(void *ctx,const md_can_frame_t *f)
{(void)ctx;last=*f;sent++;return !reject_tx;}
static void init(bool active,uint32_t now)
{
    md_acq_config_t c={.allow_standard_obd=active,.car_validation_mode=true,.startup_ms=0};
    sent=0;reject_tx=false;md_acq_init(&acq,&c,send_frame,NULL,now);
    md_acq_set_demand(&acq,UINT32_MAX,UINT32_MAX);
}
static void payload(const uint8_t *data,unsigned n,uint32_t now)
{
    md_can_frame_t f={.id=0x7e8,.dlc=8};CHECK(n<=7);
    f.data[0]=n;memcpy(f.data+1,data,n);md_acq_receive(&acq,&f,now);
}
static void answer(uint32_t now)
{
    CHECK(last.data[0]!=0x30);CHECK(last.data[1]==1 || last.data[1]==9 || last.data[1]==3);
    uint8_t pid=last.data[2];
    if(last.data[1]==9){const uint8_t r[]={0x7f,9,0x11};payload(r,3,now);return;}
    uint8_t r[]={0x41,pid,0xff,0xff,0xff,0xff};unsigned n=6;
    if(pid==0x60)r[5]=0xfe;
    if(pid==0x0c){r[2]=0x32;r[3]=0;n=4;}
    else if(pid==0x0d){r[2]=0;n=3;}
    else if(pid==0x05){r[2]=130;n=3;}
    else if(pid==0x42){r[2]=0x36;r[3]=0xb0;n=4;}
    else CHECK(pid==0 || pid==0x20 || pid==0x40);
    payload(r,n,now);
}
static const md_acq_exchange_t *latest(void)
{CHECK(acq.exchange_count);return &acq.exchanges[(acq.exchange_head+MD_ACQ_EXCHANGE_SIZE-1)%MD_ACQ_EXCHANGE_SIZE];}
static void prepare(uint32_t now)
{
    init(true,now);acq.state=MD_ACQ_RUNNING;acq.request_id=0x7e0;acq.response_id=0x7e8;
    acq.supported[0]=UINT32_C(1)<<(32-0x0c);
}
static void test_passive(void)
{
    init(false,0);
    for(unsigned i=0;i<300;i++) {
        md_can_frame_t f={.id=0x123,.dlc=2,.data={1,2}};
        md_acq_receive(&acq,&f,i);md_acq_tick(&acq,i);
    }
    CHECK(sent==0 && acq.exchange_count==0);
    CHECK(acq.trace_count==96 && acq.trace_overwritten==204 && acq.trace_sequence==300);
    CHECK(md_acq_snapshot(&acq,MD_SIGNAL_ENGINE_RPM,300,NULL).state==MD_UI_SIGNAL_UNAVAILABLE);
}
static void test_reads(void)
{
    init(true,0);unsigned handled=0,requests=0;uint32_t previous=0;
    for(uint32_t now=0;now<15000;now+=10) {
        md_acq_tick(&acq,now);
        if(sent!=handled) {
            CHECK(sent==handled+1);handled=sent;
            if(requests)CHECK(now-previous>=MD_ACQ_VALIDATION_GAP_MS);
            previous=now;requests++;answer(now+7);
        }
    }
    CHECK(acq.state==MD_ACQ_RUNNING && requests<=60 && requests>20);
    CHECK(acq.exchange_overwritten>0 && acq.exchange_count==MD_ACQ_EXCHANGE_SIZE);
    const md_acq_exchange_t *e=latest();CHECK(e->result==MD_EXCHANGE_OK);
    CHECK(e->elapsed_ms==7 && e->value_available && e->source==MD_SOURCE_OBD);
    CHECK(e->raw_length>=3 && e->raw[0]==0x41);
    CHECK(md_acq_snapshot(&acq,MD_SIGNAL_ENGINE_RPM,15000,NULL).value==3200);
    CHECK(md_acq_snapshot(&acq,MD_SIGNAL_ENGINE_COOLANT_TEMPERATURE,15000,NULL).value==90);
    CHECK(!acq.profile_matched && acq.config.profile==NULL);
}
static void test_errors_and_rollover(void)
{
    prepare(UINT32_MAX-100);md_acq_tick(&acq,UINT32_MAX-100);answer(20);
    CHECK(latest()->elapsed_ms==121 && latest()->value==3200);
    prepare(0);md_acq_tick(&acq,0);md_acq_tick(&acq,501);
    CHECK(latest()->result==MD_EXCHANGE_TIMEOUT && latest()->elapsed_ms==501);
    prepare(0);md_acq_tick(&acq,0);const uint8_t nrc[]={0x7f,1,0x31};payload(nrc,3,12);
    CHECK(latest()->result==MD_EXCHANGE_NEGATIVE && latest()->nrc==0x31);
    CHECK(!latest()->value_available && latest()->raw_length==3);
    prepare(0);md_acq_tick(&acq,0);md_acq_bus_available(&acq,false,25);
    CHECK(latest()->result==MD_EXCHANGE_BUS_FAULT && latest()->elapsed_ms==25);
    prepare(0);reject_tx=true;md_acq_tick(&acq,0);
    CHECK(latest()->result==MD_EXCHANGE_TX_FAILED && acq.tx_failed==1);
    prepare(0);md_acq_tick(&acq,0);
    const uint8_t wrong[]={0x41,0x0d,42};payload(wrong,3,1);
    CHECK(acq.rejected_count==1 && acq.exchange_count==0);
    md_acq_tick(&acq,501);CHECK(latest()->result==MD_EXCHANGE_TIMEOUT);
}
static void test_json(void)
{
    static md_vehicle_validation_t v;memset(&v,0,sizeof(v));
    for(unsigned i=0;i<MD_SIGNAL_COUNT;i++){v.status.signals[i].state=MD_UI_SIGNAL_UNAVAILABLE;v.status.sources[i]=MD_SOURCE_COUNT;}
    v.validation_mode=true;v.trace_count=1;v.trace[0]=(md_acq_trace_t){.frame={.id=0x123,.dlc=2,.data={0xab,0x01}},.sequence=3,.ms=4};
    v.exchange_count=1;v.exchanges[0]=*latest();
    v.status.signals[MD_SIGNAL_ENGINE_RPM]=(md_ui_signal_snapshot_t){.state=MD_UI_SIGNAL_VALID,.value=3200,.value_available=true};
    v.status.sources[MD_SIGNAL_ENGINE_RPM]=MD_SOURCE_OBD;
    cJSON *j=md_validation_json(&v,"fixture",UINT64_C(4294967300));CHECK(j);
    CHECK(cJSON_GetObjectItem(j,"uptime_ms")->valuedouble==4294967300.0);
    CHECK(!strcmp(cJSON_GetObjectItem(cJSON_GetArrayItem(cJSON_GetObjectItem(j,"frames"),0),"raw")->valuestring,"ab01"));
    cJSON *s=cJSON_GetArrayItem(cJSON_GetObjectItem(j,"signals"),MD_SIGNAL_ENGINE_RPM);
    CHECK(cJSON_GetObjectItem(s,"value")->valuedouble==3200);
    CHECK(!strcmp(cJSON_GetObjectItem(s,"source")->valuestring,"obd"));
    s=cJSON_GetArrayItem(cJSON_GetObjectItem(j,"signals"),MD_SIGNAL_VEHICLE_SPEED);CHECK(cJSON_IsNull(cJSON_GetObjectItem(s,"value")));
    cJSON_Delete(j);v.status.signals[0].state=MD_UI_SIGNAL_STALE;
    j=md_validation_json(&v,"fixture",10);s=cJSON_GetArrayItem(cJSON_GetObjectItem(j,"signals"),0);
    CHECK(cJSON_IsNull(cJSON_GetObjectItem(s,"value")));cJSON_Delete(j);
    CHECK(md_validation_json(NULL,NULL,0)==NULL);
}
static void doctor_is(md_vehicle_validation_t *v,const char *code)
{
    cJSON *j=md_validation_json(v,"synthetic",0);CHECK(j);
    cJSON *d=cJSON_GetObjectItem(j,"connection_doctor");CHECK(d);
    CHECK(!strcmp(cJSON_GetObjectItem(d,"code")->valuestring,code));
    CHECK(cJSON_IsFalse(cJSON_GetObjectItem(d,"root_cause_confirmed")));
    CHECK(cJSON_IsFalse(cJSON_GetObjectItem(j,"vehicle_validated")));
    CHECK(cJSON_IsTrue(cJSON_GetObjectItem(d,"ack_error_observed"))==(v->status.can_errors.ack>0));
    cJSON_Delete(j);
}
static void test_doctor(void)
{
    static md_vehicle_validation_t v;memset(&v,0,sizeof(v));
    doctor_is(&v,"can_disabled");
    v.status.configured=true;doctor_is(&v,"controller_unavailable");
    v.status.ready=true;doctor_is(&v,"listen_only");
    v.standard_obd=true;doctor_is(&v,"no_bus_evidence");
    v.status.rx=100;doctor_is(&v,"rx_only");
    v.status.tx_failed=3;v.status.can_errors.ack=3;doctor_is(&v,"tx_unconfirmed");
    v.status.tx=1;doctor_is(&v,"tx_confirmed_no_ecu");
    v.status.ecu_count=2;doctor_is(&v,"ecu_response_observed");
    /* Historical errors cannot hide later responses; a current fault can. */
    v.status.bus_offs=1;v.status.rx_dropped=8;doctor_is(&v,"ecu_response_observed");
    v.status.state=MD_ACQ_FAULT;doctor_is(&v,"controller_unavailable");
}
int main(void)
{
    test_passive();test_reads();test_errors_and_rollover();test_json();test_doctor();
    printf("VALIDATION PASS: read allowlist, 4 requests/s, raw/decoded, error/rollover, bounded rings and canonical JSON; acquisition_bytes=%zu exchange_bytes=%zu\n",sizeof(acq),sizeof(md_acq_exchange_t));
    return 0;
}
