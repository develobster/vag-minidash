#include "can_pipeline.h"
#include <string.h>

#if defined(ESP_PLATFORM) && defined(__XTENSA__)
#include "xtensa/config/core-isa.h"
/* IDF's PSRAM workaround deliberately clears the compiler's unconditional
 * lock-free macro. Its stdatomic shim dispatches internal-RAM words to the
 * S32C1I hardware stubs. Keep ring/mailbox metadata in internal DRAM rather
 * than disabling that necessary workaround for the rest of the application. */
_Static_assert(XCHAL_HAVE_S32C1I == 1, "RX requires hardware 32-bit CAS");
#else
_Static_assert(ATOMIC_INT_LOCK_FREE == 2, "RX requires lock-free 32-bit atomics");
#endif
_Static_assert(sizeof(atomic_uint)==sizeof(uint32_t), "32-bit handoff metadata");
_Static_assert((MD_CAN_RING_CAPACITY & (MD_CAN_RING_CAPACITY-1)) == 0, "Power-of-two ring");
#define READY 4U

void md_can_ring_init(md_can_ring_t *r)
{
    atomic_init(&r->head,0); atomic_init(&r->tail,0);
    atomic_init(&r->dropped,0); atomic_init(&r->high_water,0);
}
bool md_can_ring_push(md_can_ring_t *r, const md_can_sample_t *s)
{
    unsigned head=atomic_load_explicit(&r->head,memory_order_relaxed);
    unsigned used=head-atomic_load_explicit(&r->tail,memory_order_acquire);
    if (used==MD_CAN_RING_CAPACITY) { atomic_fetch_add(&r->dropped,1); return false; }
    r->slots[head & (MD_CAN_RING_CAPACITY-1)]=*s;
    atomic_store_explicit(&r->head,head+1,memory_order_release);
    if (++used>atomic_load_explicit(&r->high_water,memory_order_relaxed))
        atomic_store_explicit(&r->high_water,used,memory_order_relaxed);
    return true;
}
bool md_can_ring_pop(md_can_ring_t *r, md_can_sample_t *s)
{
    unsigned tail=atomic_load_explicit(&r->tail,memory_order_relaxed);
    if (tail==atomic_load_explicit(&r->head,memory_order_acquire)) return false;
    *s=r->slots[tail & (MD_CAN_RING_CAPACITY-1)];
    atomic_store_explicit(&r->tail,tail+1,memory_order_release);
    return true;
}
void md_signal_mailbox_init(md_signal_mailbox_t *b)
{
    for(unsigned i=0;i<3;i++) md_ui_signal_store_init(&b->slots[i]);
    b->writer=0; b->reader=1; atomic_init(&b->middle,2);
}
void md_signal_mailbox_publish(md_signal_mailbox_t *b, const md_ui_signal_store_t *s)
{
    b->slots[b->writer]=*s;
    b->writer=atomic_exchange_explicit(&b->middle,b->writer|READY,memory_order_acq_rel)&3U;
}
bool md_signal_mailbox_read(md_signal_mailbox_t *b, md_ui_signal_store_t *s)
{
    if (!(atomic_load_explicit(&b->middle,memory_order_acquire)&READY)) return false;
    b->reader=atomic_exchange_explicit(&b->middle,b->reader,memory_order_acq_rel)&3U;
    *s=b->slots[b->reader];
    return true;
}
void md_can_recovery_fault(md_can_recovery_t *r, uint32_t now)
{
    unsigned shift=r->attempts<5?r->attempts:5;
    uint32_t delay=1000U<<shift;
    if(delay>30000U) delay=30000U;
    if(r->attempts<32) ++r->attempts;
    r->retry_at=now+delay; r->frames=0;
}
void md_can_recovery_listen(md_can_recovery_t *r, uint32_t now, uint32_t errors)
{ r->listen_since=now; r->frames=0; r->errors=errors; }
bool md_can_recovery_valid(const md_can_recovery_t *r, uint32_t now,
                          uint32_t last_frame, uint32_t errors)
{
    return now-r->listen_since>=1000U && now-last_frame<100U &&
        r->frames>=10 && errors==r->errors;
}
bool md_can_silent(uint32_t now, uint32_t last_frame, uint32_t timeout_ms)
{ return timeout_ms && now-last_frame>=timeout_ms; }
