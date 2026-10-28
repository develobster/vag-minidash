#pragma once
#include "diagnostic_transport.h"
#include "ui_core.h"
#include "vag_can_db.h"
#include "drag_timer.h"
#include <stdatomic.h>

/* Fixed internal-RAM SPSC queue: one TWAI reader, one protocol consumer.
 * Overflow drops the newest frame and is observable; it never blocks RX. */
#define MD_CAN_RING_CAPACITY 512U
typedef struct { md_can_frame_t frame; uint32_t ms, epoch; } md_can_sample_t;
typedef struct {
    md_can_sample_t slots[MD_CAN_RING_CAPACITY];
    atomic_uint head, tail, dropped, high_water;
} md_can_ring_t;
void md_can_ring_init(md_can_ring_t *ring);
bool md_can_ring_push(md_can_ring_t *ring, const md_can_sample_t *sample);
bool md_can_ring_pop(md_can_ring_t *ring, md_can_sample_t *sample);

/* Ownership, not a seqlock: ordinary payload reads never race with writes.
 * Producer and consumer each own a slot; the atomic middle owns the third.
 * Only the UI controller may consume; only acquisition may publish. */
typedef struct {
    md_ui_signal_store_t signals;
    vehicle_telemetry_t vehicle;
    drag_result_t drag;
    vag_db_config_t config;
    uint32_t source_generation;
} md_dashboard_frame_t;
typedef struct {
    md_dashboard_frame_t slots[3];
    atomic_uint middle;
    unsigned writer, reader;
} md_signal_mailbox_t;
void md_signal_mailbox_init(md_signal_mailbox_t *box);
void md_signal_mailbox_publish(md_signal_mailbox_t *box, const md_ui_signal_store_t *store);
bool md_signal_mailbox_read(md_signal_mailbox_t *box, md_ui_signal_store_t *store);
void md_signal_mailbox_publish_frame(md_signal_mailbox_t *box, const md_dashboard_frame_t *frame);
bool md_signal_mailbox_read_frame(md_signal_mailbox_t *box, md_dashboard_frame_t *frame);

typedef struct { unsigned attempts; uint32_t retry_at, listen_since, frames, errors; } md_can_recovery_t;
void md_can_recovery_fault(md_can_recovery_t *r, uint32_t now);
void md_can_recovery_listen(md_can_recovery_t *r, uint32_t now, uint32_t errors);
bool md_can_recovery_valid(const md_can_recovery_t *r, uint32_t now,
                          uint32_t last_frame, uint32_t errors);
bool md_can_silent(uint32_t now, uint32_t last_frame, uint32_t timeout_ms);
