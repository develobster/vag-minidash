#include "vag_can_db.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"VAG DB FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static vag_can_db_t db;
static void init(vag_platform_t platform,vag_layout_t layout)
{vag_db_config_t c;vag_db_config_defaults(&c);c.platform=platform;c.layout=layout;vag_can_db_init(&db,&c);}
int main(void)
{
    vag_db_config_t c;vag_db_config_defaults(&c);CHECK(vag_db_config_valid(&c));
    c.platform=(vag_platform_t)-1;CHECK(!vag_db_config_valid(&c));
    uint32_t raw;double value;uint8_t bytes[]={0xab,0xc0,0,0};
    CHECK(vag_can_unpack(bytes,4,7,12,false,false,&raw,&value) && raw==0xabc && value==2748);
    CHECK(vag_can_unpack(bytes,4,7,12,false,true,&raw,&value) && value==-1348);
    CHECK(!vag_can_unpack(bytes,1,7,12,false,false,&raw,&value));
    CHECK(!vag_can_unpack(bytes,4,32,1,true,false,&raw,&value));
    init(PLATFORM_MQB,VAG_LAYOUT_SOURCED);
    md_can_frame_t frame={.id=0xa8,.dlc=8,.data={0,0,0,0,0,0,0,0x32}};
    CHECK(vag_can_db_decode(&db,&frame,10)==1);
    CHECK(vag_telemetry_sample(&db.telemetry,VAG_RPM,10,&value)==VAG_CANDIDATE && value==3200);
    vag_can_db_validation(&db,1U<<VAG_RPM);CHECK(vag_can_db_decode(&db,&frame,20)==1);
    CHECK(vag_telemetry_sample(&db.telemetry,VAG_RPM,20,&value)==VAG_VALID);
    frame.data[5]=0x80;CHECK(!vag_can_db_decode(&db,&frame,30));
    CHECK(vag_telemetry_sample(&db.telemetry,VAG_RPM,30,&value)==VAG_ERROR);
    frame=(md_can_frame_t){.id=0xfd,.dlc=8,.data={0,0,0,0,0x70,0x17,0,0}};
    CHECK(vag_can_db_decode(&db,&frame,40)==1);
    CHECK(vag_telemetry_sample(&db.telemetry,VAG_SPEED,40,&value)==VAG_CANDIDATE && value==60);
    CHECK(vag_telemetry_sample(&db.telemetry,VAG_SPEED,540,&value)==VAG_STALE);
    frame=(md_can_frame_t){.id=0x126,.dlc=8,.data={0,180,140,0,0,0,0,0}};
    CHECK(!vag_can_db_decode(&db,&frame,50)); /* HCA_01 is not a thermal frame. */
    init(PLATFORM_MQB,VAG_LAYOUT_REQUESTED_EXPERIMENTAL);
    vag_can_db_validation(&db,UINT32_MAX);CHECK(vag_can_db_decode(&db,&frame,50)==2);
    CHECK(vag_telemetry_sample(&db.telemetry,VAG_COOLANT,50,&value)==VAG_CANDIDATE && value==87);
    CHECK(vag_telemetry_sample(&db.telemetry,VAG_OIL,50,&value)==VAG_CANDIDATE && value==100);
    init(PLATFORM_PQ35_46,VAG_LAYOUT_SOURCED);
    frame=(md_can_frame_t){.id=0x480,.dlc=8,.data={0xe0}};
    CHECK(vag_can_db_decode(&db,&frame,100)==1);
    CHECK(vag_telemetry_sample(&db.telemetry,VAG_REGEN,100,&value)==VAG_CANDIDATE && value==1);
    frame.data[0]=0x21;CHECK(!vag_can_db_decode(&db,&frame,110)); /* wrong multiplexer */
    init(PLATFORM_PQ35_46,VAG_LAYOUT_REQUESTED_EXPERIMENTAL);
    CHECK(vag_can_db_decode(&db,&frame,110)==1);
    CHECK(vag_telemetry_sample(&db.telemetry,VAG_REGEN,110,&value)==VAG_CANDIDATE && value==1);
    frame.extended=true;CHECK(!vag_can_db_decode(&db,&frame,120));frame.extended=false;frame.remote=true;
    CHECK(!vag_can_db_decode(&db,&frame,120));
    init(PLATFORM_MQB,VAG_LAYOUT_SOURCED);
    CHECK(vag_can_db_publish(&db,VAG_SOOT,10,100,1000,true,false));
    md_ui_signal_store_t store;md_ui_signal_store_init(&store);
    vag_can_db_merge_canonical(&db,&store,100,200);
    CHECK(vag_telemetry_sample(&db.telemetry,VAG_REGEN_REMAINING,100,&value)==VAG_VALID && value==50);
    vag_can_db_merge_canonical(&db,&store,100,0);
    CHECK(vag_telemetry_sample(&db.telemetry,VAG_REGEN_REMAINING,100,NULL)==VAG_UNAVAILABLE);
    puts("VAG DB PASS: sourced/experimental isolation, Motorola, quality, multiplexer, TTL, configured soot headroom");
    return 0;
}
