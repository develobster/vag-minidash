/* Host-only ABI: reuse the firmware decoder and drag FSM for offline replay. */
#ifdef ESP_PLATFORM
#error "Offline estimation API must never be linked into firmware"
#endif
#include "vag_can_db.h"
#include "drag_timer.h"
#include <stdlib.h>
#include <string.h>
typedef struct {vag_can_db_t db;drag_timer_t drag;bool estimate;} replay_t;
void *vag_native_create(unsigned platform,unsigned layout,int offline_estimate)
{
    vag_db_config_t config;vag_db_config_defaults(&config);config.platform=platform;config.layout=layout;
    if(!vag_db_config_valid(&config)) return NULL;
    replay_t *r=calloc(1,sizeof(*r));if(!r) return NULL;
    vag_can_db_init(&r->db,&config);drag_timer_init(&r->drag);r->estimate=offline_estimate!=0;return r;
}
void vag_native_destroy(void *handle) {free(handle);}
unsigned vag_native_feed(void *handle,uint32_t id,const uint8_t *bytes,unsigned dlc,uint32_t stamp,unsigned flags)
{
    replay_t *r=handle;if(!r || !bytes || dlc>8 || (flags&4)) return 0;
    md_can_frame_t frame={.id=id,.dlc=(uint8_t)dlc,.extended=(flags&1)!=0,.remote=(flags&2)!=0};
    memcpy(frame.data,bytes,dlc);unsigned count=(unsigned)vag_can_db_decode(&r->db,&frame,stamp);
    /* Explicit host-only estimates. Python still exports telemetry as candidate
     * and labels every derived run offline_estimate, never vehicle validation. */
    if(r->estimate) r->db.telemetry.trusted=r->db.telemetry.observed;
    drag_timer_observe(&r->drag,&r->db.telemetry,stamp);return count;
}
unsigned vag_native_signal(void *handle,unsigned signal,uint32_t now,double *value)
{replay_t *r=handle;return r?vag_telemetry_sample(&r->db.telemetry,(vag_signal_t)signal,now,value):VAG_UNAVAILABLE;}
void vag_native_drag(void *handle,double values[6],uint32_t meta[7])
{
    replay_t *r=handle;if(!r || !values || !meta) return;
    const drag_result_t *v=&r->drag.result;
    values[0]=v->elapsed_s;for(unsigned i=0;i<3;i++) values[i+1]=v->seconds[i];
    values[4]=v->peak_boost_bar;values[5]=v->peak_rpm;
    meta[0]=v->state;meta[1]=v->reason;meta[2]=v->result_mask;meta[3]=v->peak_mask;
    meta[4]=v->sample_interval_ms;meta[5]=v->completed_runs;meta[6]=v->readiness_per_mille;
}
