#include "acquisition.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);exit(1);} }while(0)
#define BIT(s) (UINT32_C(1)<<(s))
static md_acquisition_t a;
static md_can_frame_t sent[4096];static unsigned count;static bool fail_tx;
static uint8_t speed;static unsigned rpm;
static bool tx(void *ctx,const md_can_frame_t *f)
{(void)ctx;CHECK(count<4096);sent[count++]=*f;return !fail_tx;}
static void init(bool active,const md_vehicle_profile_t *p,uint32_t now)
{
    count=0;fail_tx=false;speed=0;rpm=3200;
    md_acq_config_t c={.allow_standard_obd=active,.startup_ms=1000,.profile=p};
    md_acq_init(&a,&c,tx,NULL,now);
}
static void payload(uint32_t id,const uint8_t *d,size_t n,uint32_t now)
{
    md_can_frame_t f={.id=id,.dlc=8};
    if(n<=7) {f.data[0]=n;memcpy(f.data+1,d,n);md_acq_receive(&a,&f,now);return;}
    f.data[0]=0x10|(n>>8);f.data[1]=n;memcpy(f.data+2,d,6);md_acq_receive(&a,&f,now);
    unsigned seq=1;
    for(size_t i=6;i<n;i+=7){memset(f.data,0,8);f.data[0]=0x20|(seq++&15);
        size_t z=n-i;if(z>7)z=7;memcpy(f.data+1,d+i,z);md_acq_receive(&a,&f,++now);}
}
static void emulate(unsigned i,uint32_t now)
{
    md_can_frame_t *f=&sent[i];uint8_t d[30]={0};
    if(f->data[0]==0x30)return;
    if(f->data[1]==1) {
        d[0]=0x41;d[1]=f->data[2];
        if(d[1]==0){d[2]=0x18;d[3]=0x38;d[5]=1;payload(0x7e8,d,6,now);}
        else if(d[1]==0x20){d[4]=0x20;d[5]=1;payload(0x7e8,d,6,now);}
        else if(d[1]==0x40){d[2]=0x40;d[5]=0x10;payload(0x7e8,d,6,now);}
        else if(d[1]==0x0c){d[2]=(rpm*4)>>8;d[3]=(rpm*4)&255;payload(0x7e8,d,4,now);}
        else if(d[1]==0x0d){d[2]=speed;payload(0x7e8,d,3,now);}
        else if(d[1]==0x05 || d[1]==0x5c){d[2]=130;payload(0x7e8,d,3,now);}
        else if(d[1]==0x04){d[2]=128;payload(0x7e8,d,3,now);}
        else if(d[1]==0x0b){d[2]=160;payload(0x7e8,d,3,now);}
        else if(d[1]==0x33){d[2]=100;payload(0x7e8,d,3,now);}
        else if(d[1]==0x42){d[2]=0x36;d[3]=0xb0;payload(0x7e8,d,4,now);}
    }else if(f->data[1]==9) {
        d[0]=0x49;d[1]=2;d[2]=1;memcpy(d+3,"TESTVAN1234567890",17);payload(0x7e8,d,20,now);
    }else if(f->data[1]==0x22) {
        d[0]=0x62;d[1]=f->data[2];d[2]=f->data[3];d[3]=0x0f;d[4]=0xa0;payload(0x77c,d,5,now);
    }else if(f->data[1]==3) {
        d[0]=0x43;d[1]=3;d[2]=0x01;d[3]=0x33;d[4]=0xc1;d[5]=0x23;d[6]=0x03;d[7]=0;payload(0x7e8,d,8,now);
    }
}
static void run(uint32_t from,uint32_t duration)
{
    unsigned consumed=count;
    for(uint32_t t=0;t<duration;t+=50){md_acq_tick(&a,from+t);
        while(consumed<count){unsigned i=consumed++;emulate(i,from+t+1);}}
}
static void test_end_to_end(void)
{
    init(true,NULL,0);md_acq_set_demand(&a,BIT(MD_SIGNAL_ENGINE_RPM)|BIT(MD_SIGNAL_ENGINE_BOOST)|BIT(MD_SIGNAL_ENGINE_COOLANT_TEMPERATURE),0);
    run(0,10000);CHECK(a.state==MD_ACQ_RUNNING);CHECK(a.ecu_count==1);CHECK(a.response_id==0x7e8);
    CHECK(!strcmp(a.vin,"TESTVAN1234567890"));CHECK(!a.profile_matched);
    CHECK(md_acq_snapshot(&a,MD_SIGNAL_ENGINE_RPM,10000,NULL).value==3200);
    CHECK(md_acq_snapshot(&a,MD_SIGNAL_ENGINE_BOOST,10000,NULL).value==600);
    CHECK(md_acq_snapshot(&a,MD_SIGNAL_ENGINE_COOLANT_TEMPERATURE,10000,NULL).value==90);
    CHECK(md_acq_snapshot(&a,MD_SIGNAL_TRANSMISSION_GEAR,10000,NULL).state==MD_UI_SIGNAL_UNSUPPORTED);
    for(unsigned i=0;i<count;i++) {
        if(sent[i].data[0]==0x30)continue;
        CHECK(sent[i].data[1]==1 || sent[i].data[1]==9);
        if(sent[i].data[1]==1)CHECK(sent[i].data[2]!=0x04 && sent[i].data[2]!=0x42 && sent[i].data[2]!=0x5c);
    }
    CHECK(md_acq_snapshot(&a,MD_SIGNAL_ENGINE_RPM,12000,NULL).state==MD_UI_SIGNAL_STALE);
    md_acq_set_demand(&a,BIT(MD_SIGNAL_ECU_SUPPLY_VOLTAGE),0);run(10000,4000);
    CHECK(md_acq_snapshot(&a,MD_SIGNAL_ECU_SUPPLY_VOLTAGE,14000,NULL).value==14);
    CHECK(md_acq_snapshot(&a,MD_SIGNAL_BATTERY_VOLTAGE,14000,NULL).state==MD_UI_SIGNAL_UNSUPPORTED);
}
static void test_dtc(void)
{
    init(true,NULL,0);run(0,4000);CHECK(md_acq_request_dtc(&a,4000));run(4000,1000);
    CHECK(a.dtc_valid && a.dtc_count==3 && a.dtc[0]==0x0133);
    char code[6];md_acq_format_dtc(a.dtc[0],code);CHECK(!strcmp(code,"P0133"));
    md_acq_format_dtc(a.dtc[1],code);CHECK(!strcmp(code,"U0123"));
    speed=15;run(5000,2000);CHECK(!md_acq_request_dtc(&a,7000));
    speed=0;run(7000,2000);CHECK(md_acq_request_dtc(&a,9000));
    a.active=false;md_acq_tick(&a,9150);CHECK(a.service==3);
    uint8_t many[32]={0x43,15};for(unsigned i=2;i<32;i++)many[i]=1;
    payload(0x7e8,many,32,9160);CHECK(a.dtc_truncated && a.dtc_count==12);
}
static void test_closed_and_timeout(void)
{
    init(false,NULL,0);run(0,10000);CHECK(count==0);
    init(true,NULL,0);for(unsigned t=0;t<10000;t+=100)md_acq_tick(&a,t);
    CHECK(count==3 && a.state==MD_ACQ_QUIET);
    for(unsigned t=10000;t<60000;t+=100)md_acq_tick(&a,t);
    CHECK(count==3);
    md_can_frame_t f={.id=0x7e8,.dlc=8,.data={6,0x41,0,0xff,0xff,0xff,0xff,0}};
    md_acq_receive(&a,&f,60001);CHECK(a.state==MD_ACQ_QUIET);
    f.id=0x123;md_acq_receive(&a,&f,60002);CHECK(a.state==MD_ACQ_LISTEN);
    init(true,NULL,0);fail_tx=true;for(unsigned t=0;t<10000;t+=100)md_acq_tick(&a,t);
    CHECK(count==3 && a.state==MD_ACQ_QUIET && a.tx_failed==3);
}
static void test_fault_reconnect_and_idle(void)
{
    init(true,NULL,0);run(0,4000);md_acq_bus_available(&a,false,4000);
    CHECK(md_acq_snapshot(&a,MD_SIGNAL_ENGINE_RPM,4001,NULL).state==MD_UI_SIGNAL_UNAVAILABLE);
    unsigned old=count;run(4000,4000);CHECK(count==old);
    md_acq_bus_available(&a,true,8000);run(8000,4000);CHECK(a.state==MD_ACQ_RUNNING);
    rpm=0;run(12000,125000);CHECK(a.state==MD_ACQ_QUIET);
}
static const md_passive_binding_t passive={.id=0x123,.start_bit=0,.bit_count=16,.dlc=8,.scale=1,.signal=MD_SIGNAL_ENGINE_RPM,.ttl_ms=300};
static const md_uds_binding_t uds={.did=0x1234,.bytes=2,.scale=1,.signal=MD_SIGNAL_ENGINE_RPM,.period_ms=200,.ttl_ms=500};
static md_vehicle_profile_t profile={.vin="TESTVAN1234567890",.provenance="synthetic fixture, NOT vehicle knowledge",.source_license="project test fixture",
    .addressing_verified=true,.definitions_verified=true,.request_id=0x712,.response_id=0x77c,
    .identity_did=0xf187,.identity_length=2,.identity_expected={0x0f,0xa0},
    .passive=&passive,.passive_count=1,.uds=&uds,.uds_count=1};
static void test_profiles_arbitration(void)
{
    CHECK(md_acq_profile_valid(&profile));init(true,&profile,0);
    md_acq_set_demand(&a,BIT(MD_SIGNAL_ENGINE_RPM),0);run(0,4000);CHECK(a.profile_matched);
    md_can_frame_t f={.id=0x123,.dlc=8,.data={0x88,0x13}};md_acq_receive(&a,&f,4000);
    md_source_t source=MD_SOURCE_COUNT;
    CHECK(md_acq_snapshot(&a,MD_SIGNAL_ENGINE_RPM,4001,&source).value==5000 && source==MD_SOURCE_PASSIVE);
    run(4000,1000);CHECK(md_acq_snapshot(&a,MD_SIGNAL_ENGINE_RPM,5000,&source).value==4000 && source==MD_SOURCE_UDS);
    /* Loss of UDS restores standard OBD after UDS freshness expires. */
    a.uds_failures[0]=3;run(5000,2000);
    CHECK(md_acq_snapshot(&a,MD_SIGNAL_ENGINE_RPM,7000,&source).value==3200 && source==MD_SOURCE_OBD);
    profile.definitions_verified=false;CHECK(!md_acq_profile_valid(&profile));init(true,&profile,0);run(0,4000);
    CHECK(!a.profile_matched);for(unsigned i=0;i<count;i++)CHECK(sent[i].data[1]!=0x22);
    profile.definitions_verified=true;profile.vin="OTHER123456789012";init(true,&profile,0);run(0,4000);CHECK(!a.profile_matched);
    profile.vin="TESTVAN1234567890";
    profile.identity_expected[0]=0xff;init(true,&profile,0);run(0,4000);CHECK(a.vehicle_matched && !a.profile_matched);
    for(unsigned i=0;i<count;i++)if(sent[i].data[1]==0x22)CHECK(sent[i].data[2]==0xf1 && sent[i].data[3]==0x87);
    profile.identity_expected[0]=0x0f;
}
static void test_isotp(void)
{
    md_tp_rx_t r;md_tp_reset(&r);md_can_frame_t f={.id=0x7e8,.dlc=8,.data={0x10,20,0x49,2,1,'T','E','S'}};
    CHECK(md_tp_receive(&r,&f,UINT32_MAX-100)==MD_TP_FLOW);
    CHECK(md_tp_expired(&r,200));f.data[0]=0x22;CHECK(md_tp_receive(&r,&f,201)==MD_TP_INVALID);
    md_tp_reset(&r);f.data[0]=0x10;f.data[1]=129;CHECK(md_tp_receive(&r,&f,0)==MD_TP_INVALID);
    f.data[1]=20;CHECK(md_tp_receive(&r,&f,0)==MD_TP_FLOW);
    f.data[0]=0x22;CHECK(md_tp_receive(&r,&f,1)==MD_TP_INVALID);
    md_tp_reset(&r);f.data[0]=0;CHECK(md_tp_receive(&r,&f,0)==MD_TP_INVALID);
    f.data[0]=8;CHECK(md_tp_receive(&r,&f,0)==MD_TP_INVALID);
    f.data[0]=2;f.extended=true;CHECK(md_tp_receive(&r,&f,0)==MD_TP_INVALID);
    f.extended=false;f.data[0]=0x10;f.data[1]=128;md_tp_reset(&r);CHECK(md_tp_receive(&r,&f,0)==MD_TP_FLOW);
    for(unsigned i=1;i<=18;i++){f.data[0]=0x20|(i&15);CHECK(md_tp_receive(&r,&f,i)==(i==18?MD_TP_COMPLETE:MD_TP_MORE));}
    CHECK(r.used==128);
}
static void request_rpm(uint32_t now)
{a.active=false;a.cursor=0;a.due[0]=now;md_acq_tick(&a,now);CHECK(a.active && a.pid==0x0c);}
static void test_pending_and_validation(void)
{
    init(true,NULL,0);run(0,4000);request_rpm(4100);
    uint8_t neg[]={0x7f,1,0x78};
    for(unsigned t=4200;t<=4400;t+=100){payload(0x7e8,neg,3,t);CHECK(a.active);}
    payload(0x7e8,neg,3,4500);CHECK(!a.active && a.failures[0]==1);
    request_rpm(4600);unsigned before=count;
    md_can_frame_t f={.id=0x7e8,.dlc=8,.data={0x10,20,0x62,0x12,0x34,0,0,0}};
    md_acq_receive(&a,&f,4610);CHECK(count==before && a.rejected_count==1);
    /* A matching service/PID with impossible total length must never elicit FC. */
    f.data[2]=0x41;f.data[3]=0x0c;
    md_acq_receive(&a,&f,4611);CHECK(count==before && a.rejected_count==2 && !a.tp.receiving);
    uint8_t wrong[]={0x41,0x0c,1,2};payload(0x7e9,wrong,4,4620);CHECK(a.active);
    neg[2]=0x12;payload(0x7e8,neg,3,4630);CHECK(a.failures[0]==3 && a.consecutive_failures==0);
    CHECK(!md_acq_request_dtc(&a,10000)); /* stale speed */
}
static void test_fuzz_and_wrap(void)
{
    init(true,NULL,0);uint32_t rnd=0x12345;md_can_frame_t f;
    for(unsigned i=0;i<50000;i++){
        rnd=rnd*1664525+1013904223;f.id=rnd&0xfff;f.dlc=(rnd>>12)%10;
        f.extended=(rnd&0x10000)!=0;f.remote=(rnd&0x20000)!=0;
        for(unsigned j=0;j<8;j++){rnd=rnd*1664525+1013904223;f.data[j]=rnd>>24;}
        md_acq_receive(&a,&f,i);md_acq_tick(&a,i);
    }
    CHECK(a.trace_count==MD_ACQ_TRACE_SIZE && a.trace_overwritten>0);
    for(unsigned i=0;i<count;i++)CHECK(sent[i].id==0x7df && sent[i].data[1]==1 && sent[i].data[2]==0);
    uint32_t start=UINT32_MAX-2000;init(true,NULL,start);run(start,6000);CHECK(a.state==MD_ACQ_RUNNING);
}
static void test_analytics_codecs(void)
{
    init(true,NULL,0);run(0,4000);
    const unsigned pids[]={0x0f,0x46,0x06,0x07};
    const unsigned ids[]={MD_SIGNAL_IAT,MD_SIGNAL_AIR_TEMP,MD_SIGNAL_STFT,MD_SIGNAL_LTFT};
    const uint8_t bytes[]={70,60,144,112};
    const double expected[]={30,20,12.5,-12.5};
    for(unsigned i=0;i<4;i++) {
        a.active=true;a.service=1;a.pid=pids[i];a.active_rx=0x7e8;a.job=8+i;a.did_request=false;a.request_ms=5000+i;
        uint8_t d[]={0x41,pids[i],bytes[i]};payload(0x7e8,d,3,5001+i);
        md_ui_signal_snapshot_t s=md_acq_snapshot(&a,ids[i],5001+i,NULL);
        CHECK(s.state==MD_UI_SIGNAL_VALID && s.value==expected[i]);
    }
    /* Signed generic codec is evidence infrastructure, exercised using ONLY
     * the existing clearly synthetic profile, never an inferred CZPA DID. */
    md_uds_binding_t signed_binding=uds;signed_binding.signed_value=true;
    signed_binding.signal=MD_SIGNAL_RETARD_1;signed_binding.scale=.25;
    md_vehicle_profile_t signed_profile=profile;signed_profile.uds=&signed_binding;
    init(true,&signed_profile,0);run(0,4000);
    a.active=true;a.did_request=true;a.service=0x22;a.pid=(uint8_t)uds.did;a.active_rx=0x77c;a.job=0;a.request_ms=5000;
    uint8_t d[]={0x62,0x12,0x34,0xff,0xf0};payload(0x77c,d,5,5001);
    CHECK(md_acq_snapshot(&a,MD_SIGNAL_RETARD_1,5001,NULL).value==-4);
    signed_binding.signal=MD_SIGNAL_BOOST_PEAK;
    CHECK(!md_acq_profile_valid(&signed_profile));
}
int main(void)
{
    test_end_to_end();test_dtc();test_closed_and_timeout();test_fault_reconnect_and_idle();
    test_profiles_arbitration();test_isotp();test_pending_and_validation();test_fuzz_and_wrap();test_analytics_codecs();
    printf("PASS: 9 acquisition scenarios + 50,000 malformed frames; core=%zu bytes, trace=%zu bytes\n",sizeof(a),sizeof(a.trace));return 0;
}
