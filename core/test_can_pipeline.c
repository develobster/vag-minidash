#include "can_pipeline.h"
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"PIPELINE FAIL %d: %s\n",__LINE__,#x);abort();}}while(0)
#define ITERATIONS 100000U
static md_can_ring_t ring;
static md_signal_mailbox_t box;
static atomic_bool done;

static void *produce(void *unused)
{
    (void)unused;
    for(unsigned i=1;i<=ITERATIONS;i++) {
        md_can_sample_t s={.ms=i,.epoch=i^0xdeadbeefU,.frame={.id=i}};
        while(!md_can_ring_push(&ring,&s)) sched_yield();
    }
    return NULL;
}
static void *publish(void *unused)
{
    (void)unused;
    md_dashboard_frame_t frame={0};
    md_ui_signal_store_t *store=&frame.signals;md_ui_signal_store_init(store);
    for(unsigned i=1;i<=ITERATIONS;i++) {
        for(unsigned id=0;id<MD_SIGNAL_COUNT;id++)
            CHECK(md_ui_signal_store_publish_id(store,id,i,MD_UI_SIGNAL_VALID,i,300,false));
        for(unsigned id=0;id<VAG_SIGNAL_COUNT;id++) {
            frame.vehicle.value[id]=i+id;frame.vehicle.stamp_ms[id]=i;
        }
        frame.drag.completed_runs=i;frame.drag.elapsed_s=i^0xdeadbeefU;
        frame.config.shift_rpm=(uint16_t)(i%11000+1000);
        md_signal_mailbox_publish_frame(&box,&frame);
        if(i%101==0) sched_yield();
    }
    atomic_store(&done,true);return NULL;
}
int main(void)
{
    md_can_ring_init(&ring);
    md_can_sample_t s={0},out;
    for(unsigned i=0;i<MD_CAN_RING_CAPACITY;i++) {s.ms=i;CHECK(md_can_ring_push(&ring,&s));}
    CHECK(!md_can_ring_push(&ring,&s) && atomic_load(&ring.dropped)==1);
    for(unsigned i=0;i<MD_CAN_RING_CAPACITY;i++) {CHECK(md_can_ring_pop(&ring,&out));CHECK(out.ms==i);}
    CHECK(!md_can_ring_pop(&ring,&out));
    /* Ring indices wrap independently of frame timestamps. */
    atomic_store(&ring.head,UINT32_MAX-100U);atomic_store(&ring.tail,UINT32_MAX-100U);
    pthread_t thread;CHECK(!pthread_create(&thread,NULL,produce,NULL));
    for(unsigned i=1;i<=ITERATIONS;i++) {
        while(!md_can_ring_pop(&ring,&out)) sched_yield();
        CHECK(out.ms==i && out.frame.id==i && out.epoch==(i^0xdeadbeefU));
    }
    CHECK(!pthread_join(thread,NULL));
    CHECK(atomic_load(&ring.high_water)<=MD_CAN_RING_CAPACITY);
    md_signal_mailbox_init(&box);
    md_ui_signal_store_t store;CHECK(!md_signal_mailbox_read(&box,&store));
    CHECK(!pthread_create(&thread,NULL,publish,NULL));
    md_dashboard_frame_t frame;
    unsigned last=0,reads=0;
    while(!atomic_load(&done) || last<ITERATIONS) {
        if(!md_signal_mailbox_read_frame(&box,&frame)) {sched_yield();continue;}
        store=frame.signals;
        unsigned current=store.signals[0].updated_ms;CHECK(current>=last && current<=ITERATIONS);
        for(unsigned id=0;id<MD_SIGNAL_COUNT;id++) {
            md_ui_signal_snapshot_t v=md_ui_signal_snapshot(&store,id,current);
            CHECK(v.value==current && v.state==MD_UI_SIGNAL_VALID && v.age_ms==0);
        }
        for(unsigned id=0;id<VAG_SIGNAL_COUNT;id++)
            CHECK(frame.vehicle.value[id]==current+id && frame.vehicle.stamp_ms[id]==current);
        CHECK(frame.drag.completed_runs==current && frame.drag.elapsed_s==(current^0xdeadbeefU));
        CHECK(frame.config.shift_rpm==(uint16_t)(current%11000+1000));
        last=current;++reads;
    }
    CHECK(!pthread_join(thread,NULL));CHECK(reads>0);
    md_can_recovery_t r={0};
    md_can_recovery_fault(&r,UINT32_MAX-500U);CHECK(r.retry_at==499U);
    md_can_recovery_fault(&r,1000);CHECK(r.retry_at==3000);
    for(unsigned i=0;i<40;i++) md_can_recovery_fault(&r,1000);
    CHECK(r.retry_at==31000 && r.attempts==32);
    md_can_recovery_listen(&r,2000,10);r.frames=10;
    CHECK(!md_can_recovery_valid(&r,2999,2999,10));
    CHECK(md_can_recovery_valid(&r,3000,2999,10));
    CHECK(!md_can_recovery_valid(&r,3000,2999,11));
    CHECK(!md_can_recovery_valid(&r,3000,2900,10));
    CHECK(!md_can_silent(148,UINT32_MAX-100U,250));
    CHECK(md_can_silent(149,UINT32_MAX-100U,250));
    CHECK(!md_can_silent(10000,0,0));
    puts("PIPELINE PASS: ring overflow/wrap, 100k FIFO and triple-buffer snapshots, backoff and sleep deadlines");
    return 0;
}
