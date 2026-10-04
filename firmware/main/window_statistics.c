#include "window_statistics.h"
#include <math.h>
#include <string.h>

void md_stats_init(md_window_statistics_t *s) {
  if (s)
    memset(s, 0, sizeof(*s));
}
void md_stats_disconnect(md_window_statistics_t *s) {
  if (!s)
    return;
  s->connected = s->has_rate = false;
  memset(s->buckets, 0, sizeof(s->buckets));
}
void md_stats_reset_trip(md_window_statistics_t *s) {
  if (!s)
    return;
  s->trip_count = 0;
  s->trip_sum = 0;
  s->above_ms = s->below_ms = 0;
}
bool md_stats_observe(md_window_statistics_t *s, double v, uint32_t stamp) {
  if (!s || !isfinite(v) || fabs(v) > 1e12)
    return false;
  if (s->connected && stamp == s->stamp)
    return false;
  if (s->connected && (int32_t)(stamp - s->stamp) <= 0)
    return false;
  s->has_rate = s->connected;
  s->previous = s->current;
  s->previous_stamp = s->stamp;
  s->delta = s->has_rate ? v - s->current : 0;
  s->rate = s->has_rate ? s->delta * 1000.0 / (uint32_t)(stamp - s->stamp) : 0;
  s->current = v;
  s->stamp = stamp;
  s->seen = s->connected = true;
  uint32_t sec = stamp / 1000;
  md_stat_bucket_t *b = &s->buckets[sec % MD_STATS_BUCKETS];
  if (!b->used || b->second != sec)
    *b = (md_stat_bucket_t){
        .second = sec, .used = true, .minimum = (float)v, .maximum = (float)v};
  if (b->count < UINT32_MAX) {
    b->sum += (float)v;
    b->count++;
  }
  if (v < b->minimum)
    b->minimum = (float)v;
  if (v > b->maximum)
    b->maximum = (float)v;
  if (!s->session_count)
    s->session_min = s->session_max = (float)v;
  if (!s->trip_count)
    s->trip_min = s->trip_max = (float)v;
  if (v < s->session_min)
    s->session_min = (float)v;
  if (v > s->session_max)
    s->session_max = (float)v;
  if (v < s->trip_min)
    s->trip_min = (float)v;
  if (v > s->trip_max)
    s->trip_max = (float)v;
  s->session_count++;
  s->trip_count++;
  s->session_sum += v;
  s->trip_sum += v;
  return true;
}
void md_stats_threshold_time(md_window_statistics_t *s, double threshold,
                             uint32_t duration) {
  if (!s || !s->connected || !isfinite(threshold))
    return;
  if (s->current > threshold)
    s->above_ms += duration;
  if (s->current < threshold)
    s->below_ms += duration;
}
static void current(const md_window_statistics_t *s, md_stat_result_t *o) {
  o->current = s->current;
  o->delta = s->delta;
  o->rate = s->rate;
  o->has_rate = s->has_rate;
}
bool md_stats_window(const md_window_statistics_t *s, uint32_t now,
                     unsigned seconds, md_stat_result_t *o) {
  if (!s || !o || !s->connected ||
      (seconds != 1 && seconds != 5 && seconds != 10 && seconds != 30 &&
       seconds != 60))
    return false;
  memset(o, 0, sizeof(*o));
  current(s, o);
  double sum = 0;
  for (unsigned i = 0; i < MD_STATS_BUCKETS; i++) {
    const md_stat_bucket_t *b = &s->buckets[i];
    if (!b->used || !b->count ||
        (uint32_t)(now - b->second * 1000U) >= seconds * 1000U)
      continue;
    if (!o->count) {
      o->minimum = b->minimum;
      o->maximum = b->maximum;
    }
    if (b->minimum < o->minimum)
      o->minimum = b->minimum;
    if (b->maximum > o->maximum)
      o->maximum = b->maximum;
    o->count += b->count;
    sum += b->sum;
  }
  if (!o->count)
    return false;
  o->average = sum / o->count;
  return true;
}
bool md_stats_session(const md_window_statistics_t *s, bool trip,
                      md_stat_result_t *o) {
  if (!s || !o || !(trip ? s->trip_count : s->session_count))
    return false;
  memset(o, 0, sizeof(*o));
  current(s, o);
  o->count = trip ? s->trip_count : s->session_count;
  o->minimum = trip ? s->trip_min : s->session_min;
  o->maximum = trip ? s->trip_max : s->session_max;
  o->average = (trip ? s->trip_sum : s->session_sum) / o->count;
  return true;
}
