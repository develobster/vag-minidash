#include "ui_instrument_profiles.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { printf("FAIL line %d\n",__LINE__); return 1; } } while(0)
int main(void)
{
    char id[48]="unchanged"; uint32_t ttl=0;
    CHECK(md_instrument_frame_decode("MDV1|golf-7-gtd-eu|1000",id,sizeof(id),&ttl));
    CHECK(!strcmp(id,"golf-7-gtd-eu") && ttl==1000);
    const char *invalid[]={"MDV1|unknown|1000","MDV1|default|0","MDV1|default|60001",
        "MDV1|default|-1","MDV1|default|+1","MDV1|default|10|extra","MDV1|default|1\n"};
    for(unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++)
        CHECK(!md_instrument_frame_decode(invalid[i],id,sizeof(id),&ttl));
    CHECK(!md_instrument_frame_decode("MDV1|golf-7-gtd-eu|10",id,4,&ttl));
    md_instrument_selection_t selection={0}; bool matched; float low,high;
    const md_instrument_profile_t *p=md_instrument_active(&selection,0,&matched);
    CHECK(!matched && md_instrument_profile_range(p,MD_SIGNAL_VEHICLE_SPEED,&low,&high) && high==280);
    CHECK(md_instrument_select(&selection,"golf-7-gtd-eu",1000,UINT32_MAX-10));
    p=md_instrument_active(&selection,500,&matched);
    CHECK(matched && md_instrument_profile_range(p,MD_SIGNAL_ENGINE_RPM,&low,&high) && high==6000);
    CHECK(md_instrument_profile_range(p,MD_SIGNAL_VEHICLE_SPEED,&low,&high) && high==280);
    CHECK(!md_instrument_profile_range(p,MD_SIGNAL_ENGINE_OIL_TEMPERATURE,&low,&high));
    CHECK(!md_instrument_profile_range(p,MD_SIGNAL_ENGINE_COOLANT_TEMPERATURE,&low,&high));
    p=md_instrument_active(&selection,1000,&matched);
    CHECK(!matched && md_instrument_profile_range(p,MD_SIGNAL_ENGINE_COOLANT_TEMPERATURE,&low,&high) && low==50 && high==130);
    CHECK(md_instrument_select(&selection,"golf-7-tsi-eu",1000,10));
    CHECK(md_instrument_profile_range(md_instrument_active(&selection,10,NULL),MD_SIGNAL_VEHICLE_SPEED,&low,&high) && high==260);
    CHECK(md_instrument_select(&selection,"default",1000,10));
    CHECK(!md_instrument_select(&selection,"bad",1000,10));
    CHECK(md_instrument_active(&selection,10,&matched)==md_instrument_profile_default() && !matched);
    puts("INSTRUMENT_PROFILE_CONTRACT PASS"); return 0;
}
