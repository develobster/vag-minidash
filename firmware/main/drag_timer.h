#pragma once
#include "vag_can_db.h"
#include "performance_timer.h"
typedef enum { DRAG_IDLE, DRAG_READY, DRAG_RUNNING, DRAG_FINISHED } drag_state_t;
typedef enum { DRAG_OK, DRAG_NO_SPEED, DRAG_DATA_GAP, DRAG_STOPPED, DRAG_SOURCE_CHANGED, DRAG_SPIKE } drag_reason_t;
typedef struct {
    drag_state_t state;
    drag_reason_t reason;
    double elapsed_s, seconds[3], peak_boost_bar, peak_rpm;
    uint32_t result_mask, peak_mask, sample_interval_ms, completed_runs;
    uint16_t readiness_per_mille;
} drag_result_t;
typedef struct {
    drag_result_t result;
    md_performance_timer_t segments[3]; /* 0-60, 0-100, 60-100 */
    uint32_t stamp_ms, stationary_since, last_zero_ms, launch_ms;
    unsigned stationary_samples;
    double previous_speed, launch_speed;
    bool seen, stationary, launch_pending, passive_source;
} drag_timer_t;
void drag_timer_init(drag_timer_t *timer);
void drag_timer_reset(drag_timer_t *timer);
/* Only trusted, fresh speed can arm or complete a run. Candidate CAN values
 * remain viewable on other screens but never become a measured result. */
void drag_timer_observe(drag_timer_t *timer, const vehicle_telemetry_t *telemetry, uint32_t now_ms);
const char *drag_state_name(drag_state_t state);
const char *drag_reason_name(drag_reason_t reason);
