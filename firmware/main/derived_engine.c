#include "derived_engine.h"
#include <math.h>
#include <string.h>

const md_derived_node_t md_derived_nodes[] = {
#include "analytics_nodes.inc"
};
const size_t md_derived_node_count =
    sizeof(md_derived_nodes) / sizeof(md_derived_nodes[0]);
_Static_assert(MD_SIGNAL_RAW_COUNT <= 32,
               "Raw demand must fit acquisition mask");
_Static_assert(MD_SIGNAL_COUNT < 255, "Stable persisted signal ID is uint8");
static bool raw_id(unsigned id) {
  return id < MD_SIGNAL_RAW_COUNT && id != MD_SIGNAL_ENGINE_BOOST;
}
const md_derived_node_t *md_derived_definition(md_ui_signal_id_t id) {
  for (size_t i = 0; i < md_derived_node_count; i++)
    if (md_derived_nodes[i].signal == id)
      return &md_derived_nodes[i];
  return NULL;
}
static int slot(unsigned id) {
  if (id < MD_SIGNAL_RAW_COUNT)
    return (int)id;
  if (id == MD_SIGNAL_RETARD_WORST)
    return MD_SIGNAL_RAW_COUNT;
  if (id == MD_SIGNAL_TRIM_SUM)
    return MD_SIGNAL_RAW_COUNT + 1;
  if (id == MD_SIGNAL_BOOST_ERROR)
    return MD_SIGNAL_RAW_COUNT + 2;
  return -1;
}
const md_window_statistics_t *
md_derived_statistics(const md_derived_engine_t *e, md_ui_signal_id_t id) {
  int n = slot(id);
  return e && n >= 0 ? &e->statistics[n] : NULL;
}
static bool visit(const md_derived_node_t *nodes, size_t count, unsigned id,
                  uint8_t *colour) {
  if (colour[id] == 1)
    return false;
  if (colour[id] == 2)
    return true;
  colour[id] = 1;
  for (size_t i = 0; i < count; i++)
    if (nodes[i].signal == id) {
      for (unsigned j = 0; j < nodes[i].dependency_count; j++)
        if (!visit(nodes, count, nodes[i].dependencies[j], colour))
          return false;
    }
  colour[id] = 2;
  return true;
}
bool md_derived_graph_validate(const md_derived_node_t *nodes, size_t count) {
  if (!nodes || count > MD_SIGNAL_COUNT)
    return false;
  uint8_t colour[MD_SIGNAL_COUNT] = {0};
  for (size_t i = 0; i < count; i++) {
    if (nodes[i].signal >= MD_SIGNAL_COUNT || nodes[i].dependency_count > 4 ||
        nodes[i].operation > MD_DERIVED_WORST4)
      return false;
    for (size_t k = 0; k < i; k++)
      if (nodes[k].signal == nodes[i].signal)
        return false;
    for (unsigned j = 0; j < nodes[i].dependency_count; j++)
      if (nodes[i].dependencies[j] >= MD_SIGNAL_COUNT)
        return false;
  }
  for (size_t i = 0; i < count; i++)
    if (!visit(nodes, count, nodes[i].signal, colour))
      return false;
  return true;
}
static uint32_t demand(unsigned id, unsigned depth) {
  if (id >= MD_SIGNAL_COUNT || depth >= MD_SIGNAL_COUNT)
    return 0;
  const md_derived_node_t *n = md_derived_definition(id);
  if (!n)
    return raw_id(id) ? UINT32_C(1) << id : 0;
  uint32_t mask = 0;
  for (unsigned i = 0; i < n->dependency_count; i++)
    mask |= demand(n->dependencies[i], depth + 1);
  return mask;
}
uint32_t md_derived_raw_demand(md_ui_signal_id_t id) {
  return demand((unsigned)id, 0);
}
const char *md_signal_provenance(md_ui_signal_id_t id) {
  if (md_derived_definition(id))
    return "local embedded / observed data";
  if (id == MD_SIGNAL_ECU_SUPPLY_VOLTAGE)
    return "OBD 01/42 / ECU module, not battery terminals";
  if (id == MD_SIGNAL_IAT || id == MD_SIGNAL_AIR_TEMP || id == MD_SIGNAL_STFT ||
      id == MD_SIGNAL_LTFT)
    return "standard OBD / supported-PID gate";
  if (id == MD_SIGNAL_ENGINE_RPM || id == MD_SIGNAL_VEHICLE_SPEED ||
      id == MD_SIGNAL_ENGINE_COOLANT_TEMPERATURE ||
      id == MD_SIGNAL_ENGINE_OIL_TEMPERATURE || id == MD_SIGNAL_ENGINE_LOAD ||
      id == MD_SIGNAL_ENGINE_MANIFOLD_PRESSURE ||
      id == MD_SIGNAL_ENGINE_AMBIENT_PRESSURE)
    return "canonical acquisition / selected physical source";
  return "candidate / binding disabled pending exact-ECU evidence";
}
void md_analytics_config_defaults(md_analytics_config_t *c) {
  if (!c)
    return;
  memset(c, 0, sizeof(*c));
  for (unsigned i = 0; i < MD_ANALYTICS_RULES; i++) {
    c->rules[i].debounce_ms = 500;
    c->rules[i].cooldown_ms = 5000;
  }
  c->rules[1].low = c->rules[5].low = c->rules[6].low = true;
  c->xy_from = 60;
  c->xy_to = 100;
}
bool md_analytics_config_valid(const md_analytics_config_t *c) {
  if (!c)
    return false;
  for (unsigned i = 0; i < MD_ANALYTICS_RULES; i++) {
    const md_analytics_rule_t *r = &c->rules[i];
    if (!isfinite(r->threshold) || !isfinite(r->hysteresis) ||
        fabs(r->threshold) > 1e6 || r->hysteresis < 0 || r->hysteresis > 1e6 ||
        r->debounce_ms > 60000 || r->cooldown_ms < 100 ||
        r->cooldown_ms > 3600000)
      return false;
  }
  for (unsigned i = 0; i < 4; i++)
    if (c->counter_bits[i] != 0 && c->counter_bits[i] != 8 &&
        c->counter_bits[i] != 16 && c->counter_bits[i] != 24 &&
        c->counter_bits[i] != 32)
      return false;
  if (!isfinite(c->idle_rpm_min) || !isfinite(c->idle_rpm_max) ||
      !isfinite(c->load_minimum) || !isfinite(c->warmup_target) ||
      !isfinite(c->heat_soak_delta) || !isfinite(c->xy_from) ||
      !isfinite(c->xy_to))
    return false;
  if (c->idle_enabled &&
      (c->idle_rpm_min <= 0 || c->idle_rpm_max <= c->idle_rpm_min ||
       c->idle_rpm_max > 10000))
    return false;
  if (c->load_enabled && (c->load_minimum <= 0 || c->load_minimum > 100))
    return false;
  if (c->warmup_enabled && (c->warmup_target < 0 || c->warmup_target > 215))
    return false;
  if (c->heat_soak_enabled &&
      (c->heat_soak_delta <= 0 || c->heat_soak_delta > 200))
    return false;
  return c->xy_from >= 0 && c->xy_to <= 300 && c->xy_to - c->xy_from >= 2;
}
bool md_derived_init(md_derived_engine_t *e, const md_analytics_config_t *c,
                     uint32_t now) {
  if (!e || !md_analytics_config_valid(c))
    return false;
  memset(e, 0, sizeof(*e));
  e->config = *c;
  e->initialized = true;
  e->last_ms = e->trip_start_ms = e->session_start_ms = now;
  e->graph_valid =
      md_derived_graph_validate(md_derived_nodes, md_derived_node_count);
  for (unsigned i = 0; i < MD_SIGNAL_RAW_COUNT; i++)
    e->previous_state[i] = MD_UI_SIGNAL_UNAVAILABLE;
  const double bounds[6][2] = {{0, 50},   {0, 100},   {60, 100},
                               {80, 120}, {100, 200}, {c->xy_from, c->xy_to}};
  for (unsigned i = 0; i < 6; i++)
    if (!md_performance_init(&e->performance[i], bounds[i][0], bounds[i][1], 1))
      return false;
  return e->graph_valid;
}
bool md_derived_configure(
    md_derived_engine_t *e, const md_analytics_config_t *c,
    uint32_t now) { /* Configuration changes reset accumulators: rates never
                       span changed semantics. */
  return md_derived_init(e, c, now);
}
void md_derived_reset_trip(md_derived_engine_t *e, uint32_t now) {
  if (!e || !e->initialized)
    return;
  e->trip_start_ms = now;
  e->misfire_trip = e->idle_misfires = e->load_misfires = e->trip_runs =
      e->trip_retard_events = 0;
  memset(e->cylinder_trip, 0, sizeof(e->cylinder_trip));
  e->worst_misfire_cylinder = 0;
  e->warmup_started = e->warmup_complete = e->warmup_invalid = e->load_event =
      false;
  for (unsigned i = 0; i < MD_DERIVED_STATS_COUNT; i++)
    md_stats_reset_trip(&e->statistics[i]);
  for (unsigned i = 0; i < MD_ANALYTICS_RULES; i++)
    e->rules[i].trip_duration_ms = 0;
  for (unsigned i = 0; i < 6; i++)
    md_performance_invalidate(&e->performance[i]);
}
void md_derived_reset_session(md_derived_engine_t *e, uint32_t now) {
  if (e && e->initialized) {
    md_analytics_config_t c = e->config;
    (void)md_derived_init(e, &c, now);
  }
}
static int rank(md_ui_signal_state_t s) {
  switch (s) {
  case MD_UI_SIGNAL_ERROR:
    return 4;
  case MD_UI_SIGNAL_UNSUPPORTED:
    return 3;
  case MD_UI_SIGNAL_UNAVAILABLE:
    return 2;
  case MD_UI_SIGNAL_STALE:
    return 1;
  default:
    return 0;
  }
}
static md_ui_signal_state_t dependencies(const md_derived_node_t *n,
                                         const md_ui_signal_store_t *s,
                                         uint32_t now, double *v,
                                         uint32_t *stamp, uint32_t *ttl) {
  md_ui_signal_state_t state = MD_UI_SIGNAL_VALID;
  uint32_t remaining = 1000, age = 0;
  for (unsigned i = 0; i < n->dependency_count; i++) {
    unsigned id = n->dependencies[i];
    md_ui_signal_snapshot_t x = md_ui_signal_snapshot(s, id, now);
    if (x.state == MD_UI_SIGNAL_VALID &&
        (!x.value_available || !isfinite(x.value)))
      x.state = MD_UI_SIGNAL_ERROR;
    if (rank(x.state) > rank(state))
      state = x.state;
    v[i] = x.value;
    if (x.age_ms > age)
      age = x.age_ms;
    if (x.state == MD_UI_SIGNAL_VALID) {
      uint32_t left = s->signals[id].ttl_ms - x.age_ms;
      if (left < remaining)
        remaining = left;
    }
  }
  *stamp = now - age;
  *ttl = age + remaining;
  return state;
}
static void event(md_derived_engine_t *e, const md_ui_signal_store_t *s,
                  uint32_t now, md_event_type_t type, unsigned signal,
                  double value, double threshold) {
  const uint32_t gap = type == MD_EVENT_CAN_ERROR ? 1000U : 100U;
  if (signal >= MD_SIGNAL_COUNT)
    return;
  if (e->event_seen[signal] &&
      (uint32_t)(now - e->last_event_ms[signal]) < gap) {
    e->events_rate_limited++;
    return;
  }
  e->event_seen[signal] = true;
  e->last_event_ms[signal] = now;
  md_analytics_event_t x = {.timestamp_ms = now,
                            .type = type,
                            .signal = (uint8_t)signal,
                            .value = value,
                            .threshold = threshold};
  const unsigned ids[] = {MD_SIGNAL_ENGINE_RPM, MD_SIGNAL_ENGINE_LOAD,
                          MD_SIGNAL_VEHICLE_SPEED, MD_SIGNAL_ENGINE_BOOST};
  float *values[] = {&x.rpm, &x.load, &x.speed, &x.map_boost};
  for (unsigned i = 0; i < 4; i++) {
    md_ui_signal_snapshot_t v = md_ui_signal_snapshot(s, ids[i], now);
    if (v.state == MD_UI_SIGNAL_VALID) {
      *values[i] = (float)v.value;
      x.snapshot_valid |= (uint8_t)(1U << i);
    }
  }
  e->events[e->event_head] = x;
  e->event_head = (e->event_head + 1) % MD_EVENT_CAPACITY;
  if (e->event_count < MD_EVENT_CAPACITY)
    e->event_count++;
  else
    e->event_overwritten++;
  e->event_total++;
}
size_t md_derived_events(const md_derived_engine_t *e,
                         md_analytics_event_t *out, size_t capacity) {
  if (!e || !out)
    return 0;
  size_t n = e->event_count;
  if (n > capacity)
    n = capacity;
  size_t first = (e->event_head + MD_EVENT_CAPACITY - n) % MD_EVENT_CAPACITY;
  for (size_t i = 0; i < n; i++)
    out[i] = e->events[(first + i) % MD_EVENT_CAPACITY];
  return n;
}
void md_derived_can_error(md_derived_engine_t *e, const md_ui_signal_store_t *s,
                          uint32_t now, double context) {
  if (e && s && e->initialized) {
    e->can_errors++;
    event(e, s, now, MD_EVENT_CAN_ERROR, MD_SIGNAL_CAN_ERROR_COUNT, context, 0);
  }
}
static void count_add(md_count_bucket_t *b, uint32_t now, uint64_t n) {
  unsigned i = (now / 1000) % 61;
  if (!b[i].used || b[i].second != now / 1000)
    b[i] = (md_count_bucket_t){.second = now / 1000, .used = true};
  b[i].count += n;
}
static uint64_t count_window(const md_count_bucket_t *b, uint32_t now) {
  uint64_t n = 0;
  for (unsigned i = 0; i < 61; i++)
    if (b[i].used && (uint32_t)(now - b[i].second * 1000U) < 60000)
      n += b[i].count;
  return n;
}
static const unsigned rule_signals[MD_ANALYTICS_RULES] = {
    MD_SIGNAL_BOOST_ERROR,
    MD_SIGNAL_BOOST_ERROR,
    MD_SIGNAL_ENGINE_COOLANT_TEMPERATURE,
    MD_SIGNAL_ENGINE_OIL_TEMPERATURE,
    MD_SIGNAL_TRANSMISSION_OIL_TEMPERATURE,
    MD_SIGNAL_ECU_SUPPLY_VOLTAGE,
    MD_SIGNAL_RETARD_WORST,
    MD_SIGNAL_TRIM_SUM};
static const md_event_type_t rule_types[MD_ANALYTICS_RULES] = {
    MD_EVENT_OVERBOOST, MD_EVENT_UNDERBOOST, MD_EVENT_HIGH_COOLANT,
    MD_EVENT_HIGH_OIL,  MD_EVENT_HIGH_DSG,   MD_EVENT_LOW_VOLTAGE,
    MD_EVENT_RETARD,    MD_EVENT_TRIM};
static void rules(md_derived_engine_t *e, const md_ui_signal_store_t *s,
                  uint32_t now, uint32_t dt) {
  for (unsigned i = 0; i < MD_ANALYTICS_RULES; i++) {
    const md_analytics_rule_t *c = &e->config.rules[i];
    md_analytics_rule_state_t *r = &e->rules[i];
    md_ui_signal_snapshot_t x = md_ui_signal_snapshot(s, rule_signals[i], now);
    bool valid = c->enabled && x.state == MD_UI_SIGNAL_VALID;
    if (!valid) {
      r->condition = r->active = r->previous_valid = false;
      continue;
    }
    r->observed = true;
    bool condition = c->low ? x.value < c->threshold : x.value > c->threshold;
    if (r->condition)
      condition = c->low ? x.value < c->threshold + c->hysteresis
                         : x.value > c->threshold - c->hysteresis;
    if (r->previous_valid && r->condition && dt <= 1000)
      r->trip_duration_ms += dt;
    if (condition && !r->condition) {
      r->since_ms = now;
      r->active = false;
    }
    if (!condition)
      r->active = false;
    if (condition && !r->active &&
        (uint32_t)(now - r->since_ms) >= c->debounce_ms) {
      r->active = true;
      if (!r->fired || (uint32_t)(now - r->last_event_ms) >= c->cooldown_ms) {
        r->fired = true;
        r->last_event_ms = now;
        r->count++;
        event(e, s, now, rule_types[i], rule_signals[i], x.value, c->threshold);
        if (i == 6) {
          e->trip_retard_events++;
          count_add(e->retard_window, now, 1);
        }
      }
    }
    r->condition = condition;
    r->previous_valid = true;
  }
}
static bool idle(const md_derived_engine_t *e, const md_ui_signal_store_t *s,
                 uint32_t now) {
  md_ui_signal_snapshot_t rpm = md_ui_signal_snapshot(s, MD_SIGNAL_ENGINE_RPM,
                                                      now),
                          speed = md_ui_signal_snapshot(
                              s, MD_SIGNAL_VEHICLE_SPEED, now);
  return e->config.idle_enabled && rpm.state == MD_UI_SIGNAL_VALID &&
         speed.state == MD_UI_SIGNAL_VALID && speed.value == 0 &&
         rpm.value >= e->config.idle_rpm_min &&
         rpm.value <= e->config.idle_rpm_max;
}
static bool load(const md_derived_engine_t *e, const md_ui_signal_store_t *s,
                 uint32_t now) {
  md_ui_signal_snapshot_t x =
      md_ui_signal_snapshot(s, MD_SIGNAL_ENGINE_LOAD, now);
  return e->config.load_enabled && x.state == MD_UI_SIGNAL_VALID &&
         x.value >= e->config.load_minimum;
}
static void counters(md_derived_engine_t *e, const md_ui_signal_store_t *s,
                     uint32_t now) {
  uint64_t delta = 0;
  bool changed = false, all = true;
  for (unsigned i = 0; i < 4; i++) {
    md_ui_signal_snapshot_t x =
        md_ui_signal_snapshot(s, MD_SIGNAL_MISFIRE_1 + i, now);
    md_counter_tracker_t *t = &e->counters[i];
    unsigned bits = e->config.counter_bits[i];
    uint64_t modulus = bits ? UINT64_C(1) << bits : 0;
    if (x.state != MD_UI_SIGNAL_VALID || !bits || x.value < 0 ||
        x.value >= modulus || floor(x.value) != x.value) {
      if (x.state == MD_UI_SIGNAL_VALID && bits)
        e->rejected_counters++;
      t->seen = false;
      all = false;
      continue;
    }
    uint64_t v = (uint64_t)x.value;
    uint32_t stamp = now - x.age_ms;
    if (t->seen && t->stamp == stamp)
      continue;
    uint64_t d = 0;
    changed = true;
    if (t->seen && (int32_t)(stamp - t->stamp) > 0 &&
        (uint32_t)(stamp - t->stamp) <= 5000) {
      if (v >= t->previous)
        d = v - t->previous;
      /* Wrap only across the extreme quarters of the evidenced modulus.
       * Ambiguous decreases are resets; never turn a reset into a huge rate. */
      else if (e->config.counter_wrap_allowed[i] &&
               t->previous >= modulus * 3 / 4 && v < modulus / 4) {
        d = modulus - t->previous + v;
        e->counter_wraps++;
      } else
        e->counter_resets++;
    }
    t->previous = v;
    t->stamp = stamp;
    t->seen = true;
    delta += d;
    e->cylinder_trip[i] += d;
  }
  if (!all) {
    e->misfire_rate_seen = false;
    memset(e->misfire_window, 0, sizeof(e->misfire_window));
  } else if (!e->misfire_rate_seen) {
    e->misfire_rate_seen = true;
    e->misfire_observed_since = now;
  }
  if (!changed)
    return;
  e->last_delta = delta;
  e->misfire_session += delta;
  e->misfire_trip += delta;
  if (idle(e, s, now))
    e->idle_misfires += delta;
  if (load(e, s, now))
    e->load_misfires += delta;
  if (delta) {
    count_add(e->misfire_window, now, delta);
    event(e, s, now, MD_EVENT_MISFIRE, MD_SIGNAL_MISFIRE_DELTA, (double)delta,
          0);
  }
  uint64_t worst = 0;
  e->worst_misfire_cylinder = 0;
  for (unsigned i = 0; i < 4; i++)
    if (e->cylinder_trip[i] > worst) {
      worst = e->cylinder_trip[i];
      e->worst_misfire_cylinder = (uint8_t)(i + 1);
    }
}
static void context(md_derived_engine_t *e, const md_ui_signal_store_t *s,
                    uint32_t now) {
  md_ui_signal_snapshot_t retard =
      md_ui_signal_snapshot(s, MD_SIGNAL_RETARD_WORST, now);
  if (load(e, s, now) && retard.state == MD_UI_SIGNAL_VALID) {
    if (!e->load_event)
      e->load_retard_peak = retard.value;
    else if (retard.value < e->load_retard_peak)
      e->load_retard_peak = retard.value;
    e->load_event = true;
  } else
    e->load_event = false;
  md_ui_signal_snapshot_t temp = md_ui_signal_snapshot(
                              s, MD_SIGNAL_ENGINE_COOLANT_TEMPERATURE, now),
                          rpm = md_ui_signal_snapshot(s, MD_SIGNAL_ENGINE_RPM,
                                                      now);
  if (e->config.warmup_enabled && !e->warmup_complete) {
    if (temp.state != MD_UI_SIGNAL_VALID || rpm.state != MD_UI_SIGNAL_VALID) {
      if (e->warmup_started)
        e->warmup_invalid = true;
    } else if (rpm.value > 0 && !e->warmup_started) {
      e->warmup_started = true;
      e->warmup_start_ms = now;
    }
    if (e->warmup_started && !e->warmup_invalid &&
        temp.state == MD_UI_SIGNAL_VALID &&
        temp.value >= e->config.warmup_target) {
      e->warmup_complete = true;
      e->warmup_result = (uint32_t)(now - e->warmup_start_ms) / 1000.0;
    }
  }
  md_ui_signal_snapshot_t speed =
      md_ui_signal_snapshot(s, MD_SIGNAL_VEHICLE_SPEED, now);
  for (unsigned i = 0; i < 6; i++) {
    if (i == 5 && !e->config.xy_enabled)
      continue;
    if (md_performance_observe(&e->performance[i],
                               speed.state == MD_UI_SIGNAL_VALID, speed.value,
                               now - speed.age_ms)) {
      e->trip_runs++;
      event(e, s, now, MD_EVENT_ACCELERATION_RUN, MD_SIGNAL_PERF_050 + i,
            e->performance[i].result_s, e->performance[i].to);
    }
  }
}
static bool compute(md_derived_engine_t *e, const md_derived_node_t *n,
                    const md_ui_signal_store_t *s, uint32_t now,
                    const double *v, double *out) {
  int k = n->dependency_count ? slot(n->dependencies[0]) : -1;
  md_stat_result_t stats;
  bool ok = k >= 0;
  unsigned p = n->parameter;
  switch (n->operation) {
  case MD_DERIVED_SUB:
    *out = v[0] - v[1];
    break;
  case MD_DERIVED_ADD:
    *out = v[0] + v[1];
    break;
  case MD_DERIVED_ABS:
    *out = fabs(v[0]);
    break;
  case MD_DERIVED_MIN4:
    *out = v[0];
    for (unsigned i = 1; i < 4; i++)
      if (v[i] < *out)
        *out = v[i];
    break;
  case MD_DERIVED_AVG4:
    *out = (v[0] + v[1] + v[2] + v[3]) / 4;
    break;
  case MD_DERIVED_SUM4:
    *out = v[0] + v[1] + v[2] + v[3];
    break;
  case MD_DERIVED_WORST4: {
    double worst = 0;
    *out = 0;
    for (unsigned i = 0; i < 4; i++)
      if (v[i] < worst) {
        worst = v[i];
        *out = i + 1;
      }
    break;
  }
  case MD_DERIVED_TRIP_MIN:
  case MD_DERIVED_TRIP_MAX:
  case MD_DERIVED_SESSION_MIN:
  case MD_DERIVED_SESSION_MAX:
    if (!ok || !md_stats_session(&e->statistics[k],
                                 n->operation == MD_DERIVED_TRIP_MIN ||
                                     n->operation == MD_DERIVED_TRIP_MAX,
                                 &stats))
      return false;
    *out = (n->operation == MD_DERIVED_TRIP_MIN ||
            n->operation == MD_DERIVED_SESSION_MIN)
               ? stats.minimum
               : stats.maximum;
    break;
  case MD_DERIVED_ROLL_AVG:
  case MD_DERIVED_ROLL_MIN:
  case MD_DERIVED_ROLL_MAX:
    if (!ok || !md_stats_window(&e->statistics[k], now, p, &stats))
      return false;
    *out = n->operation == MD_DERIVED_ROLL_AVG   ? stats.average
           : n->operation == MD_DERIVED_ROLL_MIN ? stats.minimum
                                                 : stats.maximum;
    break;
  case MD_DERIVED_RATE:
    if (!ok || !e->statistics[k].has_rate)
      return false;
    *out = e->statistics[k].rate;
    break;
  case MD_DERIVED_RULE_COUNT:
    if (!e->config.rules[p].enabled)
      return false;
    *out = (double)e->rules[p].count;
    break;
  case MD_DERIVED_RULE_TIME:
    if (!e->config.rules[p].enabled || !e->rules[p].observed)
      return false;
    *out = e->rules[p].trip_duration_ms / 1000.0;
    break;
  case MD_DERIVED_KNOCK_COUNT:
    if (!e->config.rules[6].enabled)
      return false;
    *out = (double)e->rules[6].count;
    break;
  case MD_DERIVED_TRIP_KNOCK_COUNT:
    if (!e->config.rules[6].enabled)
      return false;
    *out = (double)e->trip_retard_events;
    break;
  case MD_DERIVED_KNOCK_RATE:
    if (!e->config.rules[6].enabled)
      return false;
    *out = (double)count_window(e->retard_window, now);
    break;
  case MD_DERIVED_LOAD_PEAK:
    if (!e->load_event)
      return false;
    *out = e->load_retard_peak;
    break;
  case MD_DERIVED_MISFIRE_DELTA:
  case MD_DERIVED_MISFIRE_RATE:
  case MD_DERIVED_MISFIRE_TRIP:
  case MD_DERIVED_MISFIRE_SESSION:
  case MD_DERIVED_MISFIRE_CYL:
    for (unsigned i = 0; i < 4; i++)
      if (!e->config.counter_bits[i] || !e->counters[i].seen)
        return false;
    if (n->operation == MD_DERIVED_MISFIRE_DELTA)
      *out = (double)e->last_delta;
    if (n->operation == MD_DERIVED_MISFIRE_TRIP)
      *out = (double)e->misfire_trip;
    if (n->operation == MD_DERIVED_MISFIRE_SESSION)
      *out = (double)e->misfire_session;
    if (n->operation == MD_DERIVED_MISFIRE_CYL)
      *out = e->worst_misfire_cylinder;
    if (n->operation == MD_DERIVED_MISFIRE_RATE) {
      uint32_t ms = now - e->misfire_observed_since;
      if (!e->misfire_rate_seen || ms < 1000)
        return false;
      if (ms > 60000)
        ms = 60000;
      *out = count_window(e->misfire_window, now) * 60000.0 / ms;
    }
    break;
  case MD_DERIVED_MISFIRE_IDLE:
    if (!e->config.idle_enabled || !e->misfire_rate_seen)
      return false;
    *out = (double)e->idle_misfires;
    break;
  case MD_DERIVED_MISFIRE_LOAD:
    if (!e->config.load_enabled || !e->misfire_rate_seen)
      return false;
    *out = (double)e->load_misfires;
    break;
  case MD_DERIVED_IDLE_VALUE:
    if (!idle(e, s, now))
      return false;
    *out = v[0];
    break;
  case MD_DERIVED_LOAD_VALUE:
    if (!load(e, s, now))
      return false;
    *out = v[0];
    break;
  case MD_DERIVED_WARMUP:
    if (!e->config.warmup_enabled || !e->warmup_complete || e->warmup_invalid)
      return false;
    *out = e->warmup_result;
    break;
  case MD_DERIVED_HEAT_SOAK:
    if (!e->config.heat_soak_enabled)
      return false;
    *out = v[0] >= e->config.heat_soak_delta && v[1] == 0 && v[2] > 0;
    break;
  case MD_DERIVED_PERFORMANCE:
    if ((p == 5 && !e->config.xy_enabled) || !e->performance[p].result_valid)
      return false;
    *out = e->performance[p].result_s;
    break;
  case MD_DERIVED_RUN_DURATION:
    if (e->performance[1].state != MD_RUN_RUNNING)
      return false;
    *out = e->performance[1].elapsed_s;
    break;
  case MD_DERIVED_PERF_COUNT:
    *out = (double)e->trip_runs;
    break;
  case MD_DERIVED_EVENT_COUNT:
    *out = (double)e->event_total;
    break;
  case MD_DERIVED_LOST_COUNT:
    *out = (double)e->signal_losses;
    break;
  case MD_DERIVED_CAN_ERROR_COUNT:
    *out = (double)e->can_errors;
    break;
  default:
    return false;
  }
  return isfinite(*out);
}
static void evaluate(md_derived_engine_t *e, md_ui_signal_store_t *s,
                     const md_derived_node_t *n, uint32_t now) {
  double v[4] = {0}, out = 0;
  uint32_t stamp, ttl;
  md_ui_signal_state_t state = dependencies(n, s, now, v, &stamp, &ttl);
  if (!e->graph_valid)
    state = MD_UI_SIGNAL_ERROR;
  if (state == MD_UI_SIGNAL_VALID && !compute(e, n, s, now, v, &out))
    state = MD_UI_SIGNAL_UNAVAILABLE;
  md_ui_signal_store_publish_id(s, n->signal, out, state, stamp, ttl, false);
  int k = slot(n->signal);
  if (k >= 0) {
    if (state == MD_UI_SIGNAL_VALID) {
      /* Freshness follows the oldest dependency; observation follows the newest
       * changed input. A slowly polled BARO must not suppress new MAP peaks. */
      uint32_t newest_age = UINT32_MAX;
      for (unsigned i = 0; i < n->dependency_count; i++) {
        uint32_t age = now - s->signals[n->dependencies[i]].updated_ms;
        if (age < newest_age)
          newest_age = age;
      }
      md_stats_observe(&e->statistics[k], out,
                       n->dependency_count ? now - newest_age : now);
    } else
      md_stats_disconnect(&e->statistics[k]);
  }
}
void md_derived_tick(md_derived_engine_t *e, md_ui_signal_store_t *s,
                     uint32_t now) {
  if (!e || !s || !e->initialized)
    return;
  uint32_t dt = now - e->last_ms;
  if ((int32_t)dt < 0)
    return;
  e->last_ms = now;
  for (unsigned id = 0; id < MD_SIGNAL_RAW_COUNT; id++)
    if (raw_id(id)) {
      md_ui_signal_snapshot_t x = md_ui_signal_snapshot(s, id, now);
      if (x.state == MD_UI_SIGNAL_VALID && id >= MD_SIGNAL_MISFIRE_1 &&
          id <= MD_SIGNAL_MISFIRE_4 &&
          (x.value < 0 || x.value > UINT32_MAX || floor(x.value) != x.value)) {
        s->signals[id].state = MD_UI_SIGNAL_ERROR;
        s->signals[id].value = 0;
        x.state = MD_UI_SIGNAL_ERROR;
        x.value_available = false;
        e->rejected_counters++;
      }
      if (x.state == MD_UI_SIGNAL_VALID && id >= MD_SIGNAL_RETARD_1 &&
          id <= MD_SIGNAL_RETARD_4 && x.value > 0) {
        /* Canonical contract: retard is a non-positive crank-angle correction.
         * A verified source decoder must normalise its actual sign convention.
         */
        s->signals[id].state = MD_UI_SIGNAL_ERROR;
        s->signals[id].value = 0;
        x.state = MD_UI_SIGNAL_ERROR;
        x.value_available = false;
      }
      if (e->previous_state[id] == MD_UI_SIGNAL_VALID &&
          x.state != MD_UI_SIGNAL_VALID) {
        e->signal_losses++;
        event(e, s, now, MD_EVENT_SIGNAL_LOST, id, 0, x.state);
      }
      e->previous_state[id] = x.state;
      if (x.state == MD_UI_SIGNAL_VALID)
        md_stats_observe(&e->statistics[id], x.value, now - x.age_ms);
      else
        md_stats_disconnect(&e->statistics[id]);
    }
  /* Pure graph outputs first; stateful consumers then; refresh outputs once.
   * Catalogue order is topological, graph validation rejects accidental cycles.
   */
  for (size_t i = 0; i < md_derived_node_count; i++) {
    unsigned op = md_derived_nodes[i].operation;
    if (op == MD_DERIVED_SUB || op == MD_DERIVED_ADD || op == MD_DERIVED_ABS ||
        op == MD_DERIVED_MIN4 || op == MD_DERIVED_AVG4 ||
        op == MD_DERIVED_SUM4 || op == MD_DERIVED_WORST4)
      evaluate(e, s, &md_derived_nodes[i], now);
  }
  counters(e, s, now);
  rules(e, s, now, dt);
  context(e, s, now);
  for (size_t i = 0; i < md_derived_node_count; i++)
    evaluate(e, s, &md_derived_nodes[i], now);
}
