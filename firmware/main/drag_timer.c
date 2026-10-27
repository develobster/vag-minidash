#include "drag_timer.h"
#include <math.h>
#include <string.h>
#define SPEED_BIT (UINT32_C(1)<<VAG_SPEED)
#define MAX_GAP_MS 1000U
static void segments_init(drag_timer_t *t)
{
    (void)md_performance_init(&t->segments[0],0,60,2);
    (void)md_performance_init(&t->segments[1],0,100,2);
    (void)md_performance_init(&t->segments[2],60,100,2);
}
void drag_timer_init(drag_timer_t *t)
{ if(t) {memset(t,0,sizeof(*t));segments_init(t);} }
void drag_timer_reset(drag_timer_t *t) {drag_timer_init(t);}
static void abort_run(drag_timer_t *t,drag_reason_t reason)
{
    if(t->result.state==DRAG_RUNNING) {
        t->result.result_mask=0;t->result.elapsed_s=0;t->result.peak_mask=0;
    }
    t->result.state=DRAG_IDLE;t->result.reason=reason;
    t->result.readiness_per_mille=0;
    t->seen=t->stationary=t->launch_pending=false;t->stationary_samples=0;
    segments_init(t);
}
static void feed_segments(drag_timer_t *t,double speed,uint32_t stamp)
{
    for(unsigned i=0;i<3;i++) {
        if(md_performance_observe(&t->segments[i],true,speed,stamp)) {
            t->result.seconds[i]=t->segments[i].result_s;t->result.result_mask|=1U<<i;
        }
    }
    t->result.elapsed_s=t->segments[1].elapsed_s;
}
/* RPM/boost can update between two speed frames; keep those peaks too. */
static void observe_peaks(drag_timer_t *t,const vehicle_telemetry_t *v,uint32_t now)
{
    double value;
    if(vag_telemetry_sample(v,VAG_RPM,now,&value)==VAG_VALID &&
        (!(t->result.peak_mask&1) || value>t->result.peak_rpm)) {
        t->result.peak_rpm=value;t->result.peak_mask|=1;
    }
    if(vag_telemetry_sample(v,VAG_BOOST,now,&value)==VAG_VALID &&
        (!(t->result.peak_mask&2) || value>t->result.peak_boost_bar)) {
        t->result.peak_boost_bar=value;t->result.peak_mask|=2;
    }
}
void drag_timer_observe(drag_timer_t *t,const vehicle_telemetry_t *v,uint32_t now)
{
    if(!t || !v) return;
    double speed;
    if(vag_telemetry_sample(v,VAG_SPEED,now,&speed)!=VAG_VALID || speed<0 || speed>300) {
        if(t->result.state!=DRAG_FINISHED) abort_run(t,DRAG_NO_SPEED);
        return;
    }
    uint32_t stamp=v->stamp_ms[VAG_SPEED];bool passive=(v->passive&SPEED_BIT)!=0;
    if(now-stamp>MAX_GAP_MS) {if(t->result.state!=DRAG_FINISHED) abort_run(t,DRAG_DATA_GAP);return;}
    if(t->seen && stamp==t->stamp_ms) {
        if(t->result.state==DRAG_RUNNING) {
            observe_peaks(t,v,now);
            double start=t->segments[1].start_ms;
            t->result.elapsed_s=((uint32_t)(now-(uint32_t)start)-(start-floor(start)))/1000.0;
        }
        return;
    }
    uint32_t dt=stamp-t->stamp_ms;
    if(t->seen && (dt>MAX_GAP_MS || (int32_t)dt<=0)) abort_run(t,DRAG_DATA_GAP);
    if(t->seen && passive!=t->passive_source) abort_run(t,DRAG_SOURCE_CHANGED);
    if(t->seen && dt && fabs(speed-t->previous_speed)*1000.0/dt>100) {
        abort_run(t,DRAG_SPIKE);return;
    }
    if(speed<.5) {
        if(t->result.state==DRAG_RUNNING) abort_run(t,DRAG_STOPPED);
        if(!t->stationary) {t->stationary=true;t->stationary_since=stamp;t->stationary_samples=0;}
        if(t->stationary_samples<3) ++t->stationary_samples;
        t->last_zero_ms=stamp;t->launch_pending=false;
        unsigned stationary_ms=stamp-t->stationary_since;
        unsigned progress=stationary_ms>=500U?1000U:stationary_ms*2U;
        unsigned samples=t->stationary_samples*1000U/3U;
        t->result.readiness_per_mille=(uint16_t)(progress<samples?progress:samples);
        if(t->stationary_samples>=3 && stamp-t->stationary_since>=500) {
            t->result.state=DRAG_READY;t->result.reason=DRAG_OK;
        }
    } else if(t->result.state==DRAG_READY) {
        if(stamp-t->last_zero_ms>MAX_GAP_MS) {abort_run(t,DRAG_STOPPED);return;}
        if(!t->launch_pending && speed>=.8) {
            t->launch_pending=true;t->launch_ms=stamp;t->launch_speed=speed;
        } else if(t->launch_pending && speed>=1 && speed>=t->launch_speed+.1) {
            memset(t->result.seconds,0,sizeof(t->result.seconds));
            t->result.result_mask=t->result.peak_mask=t->result.sample_interval_ms=0;
            t->result.elapsed_s=0;t->result.state=DRAG_RUNNING;t->result.reason=DRAG_OK;
            segments_init(t);feed_segments(t,0,t->last_zero_ms);
            feed_segments(t,t->launch_speed,t->launch_ms);feed_segments(t,speed,stamp);
            t->stationary=false;t->launch_pending=false;
        } else if(t->launch_pending && speed<t->launch_speed-.3) t->launch_pending=false;
    } else if(t->result.state==DRAG_RUNNING) {
        feed_segments(t,speed,stamp);
    } else t->stationary=false;
    if(t->result.state==DRAG_RUNNING) {
        if(dt>t->result.sample_interval_ms) t->result.sample_interval_ms=dt;
        observe_peaks(t,v,now);
        if(t->segments[1].state==MD_RUN_INVALID) abort_run(t,DRAG_STOPPED);
        else if(t->result.result_mask&2) {
            t->result.state=DRAG_FINISHED;++t->result.completed_runs;
            t->result.elapsed_s=t->result.seconds[1];
        }
    }
    t->stamp_ms=stamp;t->previous_speed=speed;t->passive_source=passive;t->seen=true;
}
const char *drag_state_name(drag_state_t state)
{
    static const char *const names[]={"IDLE","READY","RUNNING","FINISHED"};
    return (unsigned)state<4?names[state]:"IDLE";
}
const char *drag_reason_name(drag_reason_t reason)
{
    static const char *const names[]={"OK","NO VERIFIED SPEED","SPEED GAP","RUN ABORTED","SOURCE CHANGED","SPEED SPIKE"};
    return (unsigned)reason<6?names[reason]:"UNKNOWN";
}
