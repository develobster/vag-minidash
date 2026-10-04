#pragma once
#include "performance_timer.h"
#include "ui_core.h"
#include "window_statistics.h"

#define MD_DERIVED_STATS_COUNT (MD_SIGNAL_RAW_COUNT + 3U)
#define MD_EVENT_CAPACITY 32U
#define MD_ANALYTICS_RULES 8U
typedef enum {
#include "analytics_operations.inc"
} md_derived_operation_t;
typedef struct {
  uint8_t signal, operation, dependency_count, dependencies[4], parameter;
} md_derived_node_t;
extern const md_derived_node_t md_derived_nodes[];
extern const size_t md_derived_node_count;
typedef enum {
  MD_EVENT_RETARD,
  MD_EVENT_MISFIRE,
  MD_EVENT_OVERBOOST,
  MD_EVENT_UNDERBOOST,
  MD_EVENT_HIGH_COOLANT,
  MD_EVENT_HIGH_OIL,
  MD_EVENT_HIGH_DSG,
  MD_EVENT_LOW_VOLTAGE,
  MD_EVENT_SIGNAL_LOST,
  MD_EVENT_CAN_ERROR,
  MD_EVENT_ACCELERATION_RUN,
  MD_EVENT_TRIM
} md_event_type_t;
typedef struct {
  uint32_t timestamp_ms;
  md_event_type_t type;
  uint8_t signal, snapshot_valid;
  double value, threshold;
  float rpm, load, speed, map_boost;
} md_analytics_event_t;
typedef struct {
  bool enabled, low;
  double threshold, hysteresis;
  uint32_t debounce_ms, cooldown_ms;
} md_analytics_rule_t;
typedef struct {
  md_analytics_rule_t rules[MD_ANALYTICS_RULES];
  uint8_t counter_bits[4]; /* 0=encoding unverified; 8/16/24/32 after evidence
                              only. */
  bool counter_wrap_allowed[4]; /* Evidence must distinguish wrapping/reset
                                   semantics. */
  bool idle_enabled, load_enabled, warmup_enabled, heat_soak_enabled,
      xy_enabled;
  double idle_rpm_min, idle_rpm_max, load_minimum, warmup_target,
      heat_soak_delta, xy_from, xy_to;
} md_analytics_config_t;
typedef struct {
  bool condition, active, fired, observed, previous_valid;
  uint32_t since_ms, last_event_ms;
  uint64_t count, trip_duration_ms;
} md_analytics_rule_state_t;
typedef struct {
  uint64_t previous;
  uint32_t stamp;
  bool seen;
} md_counter_tracker_t;
typedef struct {
  uint32_t start_ms;
  uint64_t count;
  bool used;
} md_count_bucket_t;
typedef struct {
  md_analytics_config_t config;
  md_window_statistics_t statistics[MD_DERIVED_STATS_COUNT];
  md_analytics_rule_state_t rules[MD_ANALYTICS_RULES];
  md_counter_tracker_t counters[4];
  md_count_bucket_t misfire_window[61], retard_window[61];
  md_performance_timer_t performance[6];
  md_analytics_event_t events[MD_EVENT_CAPACITY];
  uint16_t event_head, event_count;
  uint32_t event_overwritten, last_ms, trip_start_ms, session_start_ms;
  uint32_t last_event_ms[MD_SIGNAL_COUNT], events_rate_limited;
  bool event_seen[MD_SIGNAL_COUNT];
  uint32_t counter_resets, counter_wraps, rejected_counters;
  uint32_t warmup_start_ms, misfire_observed_since;
  uint64_t misfire_session, misfire_trip, idle_misfires, load_misfires;
  uint64_t event_total, signal_losses, can_errors, trip_runs,
      trip_retard_events;
  uint64_t cylinder_trip[4], last_delta;
  uint8_t worst_misfire_cylinder;
  double load_retard_peak, warmup_result;
  bool initialized, graph_valid, load_event, warmup_started, warmup_complete,
      warmup_invalid, misfire_rate_seen;
  md_ui_signal_state_t previous_state[MD_SIGNAL_RAW_COUNT];
} md_derived_engine_t;

void md_analytics_config_defaults(md_analytics_config_t *c);
bool md_analytics_config_valid(const md_analytics_config_t *c);
bool md_derived_graph_validate(const md_derived_node_t *nodes, size_t count);
bool md_derived_init(md_derived_engine_t *e, const md_analytics_config_t *c,
                     uint32_t now);
bool md_derived_configure(md_derived_engine_t *e,
                          const md_analytics_config_t *c, uint32_t now);
void md_derived_tick(md_derived_engine_t *e, md_ui_signal_store_t *canonical,
                     uint32_t now);
void md_derived_reset_trip(md_derived_engine_t *e, uint32_t now);
void md_derived_reset_session(md_derived_engine_t *e, uint32_t now);
void md_derived_can_error(md_derived_engine_t *e,
                          const md_ui_signal_store_t *store, uint32_t now,
                          double context);
size_t md_derived_events(const md_derived_engine_t *e,
                         md_analytics_event_t *out, size_t capacity);
uint32_t md_derived_raw_demand(md_ui_signal_id_t signal);
const char *md_signal_provenance(md_ui_signal_id_t signal);
const md_derived_node_t *md_derived_definition(md_ui_signal_id_t signal);
const md_window_statistics_t *
md_derived_statistics(const md_derived_engine_t *e, md_ui_signal_id_t signal);
