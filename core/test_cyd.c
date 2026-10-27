#include "simulation.h"
#include "captive_dns.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void)
{
    double v;
    for(unsigned time=0;time<60000;time+=37) {
        for(unsigned id=0;id<14;id++) {
            assert(md_simulation_value(id,time,&v)); assert(isfinite(v));
        }
        assert(md_simulation_value(MD_SIGNAL_TRANSMISSION_GEAR,time,&v) && v>=1 && v<=6 && floor(v)==v);
        assert(md_simulation_value(MD_SIGNAL_BATTERY_VOLTAGE,time,&v) && v>=13.8 && v<=14.4+1e-6);
    }
    assert(md_simulation_value(MD_SIGNAL_ENGINE_RPM,0,&v) && v==850);
    assert(md_simulation_value(MD_SIGNAL_ENGINE_RPM,15000,&v) && v==5200);
    assert(md_simulation_value(MD_SIGNAL_ENGINE_BOOST,15000,&v) && v==1400);
    assert(md_simulation_value(MD_SIGNAL_ENGINE_AMBIENT_PRESSURE,0,&v) && v==1000);
    assert(!md_simulation_value(MD_SIGNAL_COUNT,0,&v));
    assert(!md_simulation_value(0,0,NULL));
    uint8_t query[512]={0x12,0x34,1,0,0,1,0,0,0,0,0,0,3,'a','p','i',0,0,1,0,1};
    uint8_t original[512]; memcpy(original,query,sizeof(query));
    assert(md_captive_dns_reply(query,21,sizeof(query))==37);
    assert(query[0]==0x12 && query[1]==0x34 && query[7]==1);
    assert(query[33]==192 && query[34]==168 && query[35]==4 && query[36]==1);
    for(size_t n=0;n<21;n++) {memcpy(query,original,512);assert(!md_captive_dns_reply(query,n,512));}
    memcpy(query,original,512); assert(!md_captive_dns_reply(query,21,36));
    memcpy(query,original,512);query[12]=0xc0;assert(!md_captive_dns_reply(query,21,512));
    memcpy(query,original,512);query[18]=28;assert(!md_captive_dns_reply(query,21,512));
    memcpy(query,original,512);query[5]=2;assert(!md_captive_dns_reply(query,21,512));
    puts("CYD simulation and captive DNS contract passed");
    return 0;
}
