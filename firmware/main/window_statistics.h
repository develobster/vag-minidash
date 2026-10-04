#pragma once
#include <stdbool.h>
#include <stdint.h>

/* One-second aggregation buckets, bounded to the current + 60 past seconds.
 * Mean is sample-weighted. Window edges have <1 s quantisation; every sample
 * contributes to min/max/sum, including bursts. No allocation, no lost peaks.
 */
#define MD_STATS_BUCKETS 61U
typedef struct {
  uint32_t second, count;
  float sum, minimum, maximum;
  bool used;
} md_stat_bucket_t;
typedef struct {
  md_stat_bucket_t buckets[MD_STATS_BUCKETS];
  double current, previous, delta, rate, session_sum, trip_sum;
  float session_min, session_max, trip_min, trip_max;
  uint32_t stamp, previous_stamp;
  uint64_t session_count, trip_count, above_ms, below_ms;
  bool seen, connected, has_rate;
} md_window_statistics_t;
typedef struct {
  double current, minimum, maximum, average, delta, rate;
  uint64_t count;
  bool has_rate;
} md_stat_result_t;
void md_stats_init(md_window_statistics_t *s);
bool md_stats_observe(md_window_statistics_t *s, double value, uint32_t stamp);
void md_stats_disconnect(md_window_statistics_t *s);
void md_stats_reset_trip(md_window_statistics_t *s);
void md_stats_threshold_time(md_window_statistics_t *s, double threshold,
                             uint32_t duration_ms);
bool md_stats_window(const md_window_statistics_t *s, uint32_t now,
                     unsigned seconds, md_stat_result_t *out);
bool md_stats_session(const md_window_statistics_t *s, bool trip,
                      md_stat_result_t *out);
