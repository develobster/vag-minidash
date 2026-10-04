#include "performance_timer.h"
#include <math.h>
#include <string.h>
bool md_performance_init(md_performance_timer_t *t, double from, double to,
                         double h) {
  if (!t || !isfinite(from) || !isfinite(to) || !isfinite(h) || from < 0 ||
      to <= from || to > 300 || h <= 0 || h > 10 || to - from < 2 * h)
    return false;
  memset(t, 0, sizeof(*t));
  t->from = from;
  t->to = to;
  t->hysteresis = h;
  return true;
}
void md_performance_invalidate(md_performance_timer_t *t) {
  if (t) {
    t->state = MD_RUN_INVALID;
    t->seen = false;
    t->elapsed_s = 0;
    t->result_valid = false;
  }
}
bool md_performance_observe(md_performance_timer_t *t, bool valid, double v,
                            uint32_t stamp) {
  if (!t)
    return false;
  if (!valid || !isfinite(v) || v < 0 || v > 300) {
    md_performance_invalidate(t);
    return false;
  }
  if (t->seen && stamp == t->stamp)
    return false;
  const uint32_t dt = stamp - t->stamp;
  if (t->seen && ((int32_t)dt <= 0 || dt > 1000)) {
    md_performance_invalidate(t);
  }
  bool complete = false;
  if (t->seen && t->state == MD_RUN_ARMED && v > t->from &&
      t->previous_speed <= t->from) {
    double fraction =
        t->from == 0 ? 0
                     : (t->from - t->previous_speed) / (v - t->previous_speed);
    t->start_ms = (double)t->stamp + fraction * dt;
    t->state = MD_RUN_RUNNING;
    t->result_valid = false;
  }
  if (t->seen && t->state == MD_RUN_RUNNING) {
    /* Unsigned elapsed supports the ESP millis wrap; start fraction <1 s. */
    t->elapsed_s = ((uint32_t)(stamp - (uint32_t)t->start_ms) -
                    (t->start_ms - floor(t->start_ms))) /
                   1000.0;
    if (v < t->previous_speed - t->hysteresis || t->elapsed_s > 120 ||
        v < t->from - t->hysteresis)
      md_performance_invalidate(t);
    else if (v >= t->to && t->previous_speed < t->to && v > t->previous_speed) {
      double crossing =
          (t->to - t->previous_speed) / (v - t->previous_speed) * dt;
      t->result_s = ((uint32_t)(t->stamp - (uint32_t)t->start_ms) + crossing -
                     (t->start_ms - floor(t->start_ms))) /
                    1000.0;
      t->result_valid = true;
      t->state = MD_RUN_COMPLETE;
      t->completed_ms = stamp;
      t->completed_count++;
      complete = true;
    }
  }
  const bool below = t->from == 0 ? v == 0 : v <= t->from - t->hysteresis;
  if (below && t->state != MD_RUN_RUNNING) {
    if (t->state != MD_RUN_ARMED) {
      t->state = MD_RUN_ARMED;
      t->armed_ms = stamp;
    }
  }
  t->previous_speed = v;
  t->stamp = stamp;
  t->seen = true;
  return complete;
}
