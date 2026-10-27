#include "drag_timer.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"DRAG FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static vag_can_db_t db;
static drag_timer_t drag;
static void init(void) {vag_db_config_t c;vag_db_config_defaults(&c);vag_can_db_init(&db,&c);drag_timer_init(&drag);}
static void sample(uint32_t stamp,double speed,bool trusted,bool passive)
{
    CHECK(vag_can_db_publish(&db,VAG_SPEED,speed,stamp,500,trusted,passive));
    CHECK(vag_can_db_publish(&db,VAG_RPM,3000+speed*20,stamp,500,trusted,passive));
    CHECK(vag_can_db_publish(&db,VAG_BOOST,1+speed*.01,stamp,500,trusted,passive));
    drag_timer_observe(&drag,&db.telemetry,stamp);
}
static void arm(uint32_t base)
{sample(base,0,true,true);sample(base+100,.1,true,true);sample(base+600,0,true,true);CHECK(drag.result.state==DRAG_READY);}
static void run(uint32_t base)
{
    arm(base);sample(base+700,1,true,true);CHECK(drag.result.state==DRAG_READY);
    sample(base+800,2,true,true);CHECK(drag.result.state==DRAG_RUNNING);
    drag_timer_observe(&drag,&db.telemetry,base+850);CHECK(fabs(drag.result.elapsed_s-.25)<.001);
    for(unsigned i=3;i<=100;i++) sample(base+600+i*100,i,true,true);
    CHECK(drag.result.state==DRAG_FINISHED && drag.result.result_mask==7);
    CHECK(fabs(drag.result.seconds[0]-6)<.001 && fabs(drag.result.seconds[1]-10)<.001 && fabs(drag.result.seconds[2]-4)<.001);
    CHECK(drag.result.sample_interval_ms==100 && drag.result.completed_runs==1);
    CHECK(drag.result.peak_mask==3 && fabs(drag.result.peak_rpm-5000)<.001 && fabs(drag.result.peak_boost_bar-2)<.001);
}
int main(void)
{
    init();run(0);init();run(UINT32_MAX-1000U); /* millis wrap */
    init();for(unsigned i=0;i<20;i++)sample(i*100,0,false,true);CHECK(drag.result.state==DRAG_IDLE);
    init();arm(0);sample(700,.6,true,true);sample(800,0,true,true);CHECK(drag.result.state==DRAG_READY);
    sample(900,1,true,true);sample(1000,2,true,true);CHECK(drag.result.state==DRAG_RUNNING);
    sample(1100,0,true,true);CHECK(drag.result.state==DRAG_IDLE && !drag.result.result_mask);
    init();arm(0);sample(700,1,true,true);sample(800,2,true,true);
    sample(900,120,true,true);CHECK(drag.result.state==DRAG_IDLE && drag.result.reason==DRAG_SPIKE);
    init();arm(0);sample(700,1,true,true);sample(800,2,true,true);
    sample(900,3,true,false);CHECK(drag.result.state==DRAG_IDLE && drag.result.reason==DRAG_SOURCE_CHANGED);
    init();arm(0);sample(700,1,true,true);sample(800,2,true,true);
    drag_timer_observe(&drag,&db.telemetry,1400);CHECK(drag.result.state==DRAG_IDLE && !drag.result.result_mask);
    init();arm(0);sample(700,1,true,true);sample(800,2,true,true);
    sample(2000,3,true,true);CHECK(drag.result.state==DRAG_IDLE && drag.result.reason==DRAG_DATA_GAP);
    puts("DRAG PASS: debounce, 0-60/0-100/60-100, peaks, wrap, candidate/gap/source/spike/stop rejection");
    return 0;
}
