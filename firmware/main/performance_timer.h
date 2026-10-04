#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef enum {
  MD_RUN_WAIT,
  MD_RUN_ARMED,
  MD_RUN_RUNNING,
  MD_RUN_COMPLETE,
  MD_RUN_INVALID
} md_run_state_t;
typedef struct {
  double from, to, hysteresis, previous_speed, start_ms, elapsed_s, result_s;
  uint32_t stamp, armed_ms, completed_ms, completed_count;
  md_run_state_t state;
  bool seen, result_valid;
} md_performance_timer_t;
bool md_performance_init(md_performance_timer_t *t, double from, double to,
                         double hysteresis);
bool md_performance_observe(md_performance_timer_t *t, bool valid, double speed,
                            uint32_t stamp);
void md_performance_invalidate(md_performance_timer_t *t);
