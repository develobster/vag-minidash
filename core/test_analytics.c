#include "derived_engine.h"
#include "ui_configuration.h"
#include "ui_theme.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static md_derived_engine_t e;
static md_ui_signal_store_t store;
static md_analytics_config_t config;
static unsigned checks, allocations;
static bool watch;
#ifdef MD_TEST_ALLOCATIONS
void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
void *__real_realloc(void *, size_t);
void __real_free(void *);
void *__wrap_malloc(size_t n) {
  if (watch)
    allocations++;
  return __real_malloc(n);
}
void *__wrap_calloc(size_t n, size_t m) {
  if (watch)
    allocations++;
  return __real_calloc(n, m);
}
void *__wrap_realloc(void *p, size_t n) {
  if (watch)
    allocations++;
  return __real_realloc(p, n);
}
void __wrap_free(void *p) {
  if (watch)
    allocations++;
  __real_free(p);
}
#endif
#define CHECK(X)                                                               \
  do {                                                                         \
    checks++;                                                                  \
    if (!(X)) {                                                                \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #X);             \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)
static void init(void) {
  md_analytics_config_defaults(&config);
  md_ui_signal_store_init(&store);
  CHECK(md_derived_init(&e, &config, 0));
}
static void put(unsigned id, double v, unsigned state, uint32_t ms,
                uint32_t ttl) {
  CHECK(md_ui_signal_store_publish_id(&store, id, v, state, ms, ttl, false));
}
static void value(unsigned id, double v, uint32_t ms) {
  put(id, v, MD_UI_SIGNAL_VALID, ms, 5000);
}
static md_ui_signal_snapshot_t snap(unsigned id, uint32_t now) {
  return md_ui_signal_snapshot(&store, id, now);
}
static void expect(unsigned id, double v, uint32_t now) {
  md_ui_signal_snapshot_t s = snap(id, now);
  CHECK(s.state == MD_UI_SIGNAL_VALID && s.value_available);
  CHECK(fabs(s.value - v) < 0.0001);
}
static void retard(double a, double b, double c, double d, uint32_t ms) {
  value(MD_SIGNAL_RETARD_1, a, ms);
  value(MD_SIGNAL_RETARD_2, b, ms);
  value(MD_SIGNAL_RETARD_3, c, ms);
  value(MD_SIGNAL_RETARD_4, d, ms);
}
static void misfire(double a, double b, double c, double d, uint32_t ms) {
  value(MD_SIGNAL_MISFIRE_1, a, ms);
  value(MD_SIGNAL_MISFIRE_2, b, ms);
  value(MD_SIGNAL_MISFIRE_3, c, ms);
  value(MD_SIGNAL_MISFIRE_4, d, ms);
}
static void dependency_tests(void) {
  init();
  value(MD_SIGNAL_ENGINE_MANIFOLD_PRESSURE, 1600, 0);
  value(MD_SIGNAL_ENGINE_AMBIENT_PRESSURE, 1000, 0);
  md_derived_tick(&e, &store, 0);
  expect(MD_SIGNAL_ENGINE_BOOST, 600, 0);
  expect(MD_SIGNAL_BOOST_PEAK, 600, 0);
  value(MD_SIGNAL_ENGINE_MANIFOLD_PRESSURE, 1700, 50);
  md_derived_tick(&e, &store, 50);
  expect(MD_SIGNAL_BOOST_PEAK, 700, 50);
  put(MD_SIGNAL_ENGINE_AMBIENT_PRESSURE, 0, MD_UI_SIGNAL_UNSUPPORTED, 100,
      1000);
  md_derived_tick(&e, &store, 100);
  CHECK(snap(MD_SIGNAL_ENGINE_BOOST, 100).state == MD_UI_SIGNAL_UNSUPPORTED);
  CHECK(!snap(MD_SIGNAL_BOOST_PEAK, 100).value_available);
  put(MD_SIGNAL_ENGINE_AMBIENT_PRESSURE, 1000, MD_UI_SIGNAL_VALID, 200, 100);
  md_derived_tick(&e, &store, 300);
  CHECK(snap(MD_SIGNAL_BOOST_AVG, 300).state == MD_UI_SIGNAL_STALE);
  value(MD_SIGNAL_ENGINE_AMBIENT_PRESSURE, 1000, 400);
  value(MD_SIGNAL_ENGINE_MANIFOLD_PRESSURE, 1800, 400);
  md_derived_tick(&e, &store, 400);
  expect(MD_SIGNAL_BOOST_PEAK, 800, 400);
  value(MD_SIGNAL_STFT, NAN, 400);
  value(MD_SIGNAL_LTFT, 2, 400);
  md_derived_tick(&e, &store, 400);
  CHECK(snap(MD_SIGNAL_TRIM_SUM, 400).state == MD_UI_SIGNAL_ERROR);
  value(MD_SIGNAL_STFT, 3, 500);
  md_derived_tick(&e, &store, 500);
  expect(MD_SIGNAL_TRIM_SUM, 5, 500);
  put(MD_SIGNAL_LTFT, 0, MD_UI_SIGNAL_UNAVAILABLE, 600, 5000);
  md_derived_tick(&e, &store, 600);
  CHECK(snap(MD_SIGNAL_TRIM_AVG, 600).state == MD_UI_SIGNAL_UNAVAILABLE);
  CHECK(md_derived_raw_demand(MD_SIGNAL_BOOST_PEAK) ==
        ((1U << MD_SIGNAL_ENGINE_MANIFOLD_PRESSURE) |
         (1U << MD_SIGNAL_ENGINE_AMBIENT_PRESSURE)));
  CHECK(md_derived_raw_demand(MD_SIGNAL_COUNT) == 0);
  md_derived_node_t cycle[2] = {{.signal = MD_SIGNAL_BOOST_PEAK,
                                 .operation = MD_DERIVED_ABS,
                                 .dependency_count = 1,
                                 .dependencies = {MD_SIGNAL_BOOST_AVG}},
                                {.signal = MD_SIGNAL_BOOST_AVG,
                                 .operation = MD_DERIVED_ABS,
                                 .dependency_count = 1,
                                 .dependencies = {MD_SIGNAL_BOOST_PEAK}}};
  CHECK(!md_derived_graph_validate(cycle, 2));
  cycle[1].dependencies[0] = MD_SIGNAL_COUNT;
  CHECK(!md_derived_graph_validate(cycle, 2));
  cycle[1] = cycle[0];
  CHECK(!md_derived_graph_validate(cycle, 2));
  CHECK(!md_derived_graph_validate(NULL, 0));
}
static void statistics_tests(void) {
  static md_window_statistics_t s;
  md_stat_result_t r;
  md_stats_init(&s);
  for (unsigned i = 0; i <= 70; i++)
    CHECK(md_stats_observe(&s, i, i * 1000));
  CHECK(!md_stats_observe(&s, 99, 70000));
  CHECK(md_stats_window(&s, 70000, 5, &r));
  CHECK(r.count == 5 && r.minimum == 66 && r.maximum == 70 && r.average == 68);
  for (unsigned i = 0; i < 5; i++) {
    const unsigned w[] = {1, 5, 10, 30, 60};
    CHECK(md_stats_window(&s, 70000, w[i], &r));
    CHECK(r.count == w[i]);
  }
  CHECK(!md_stats_window(&s, 70000, 3, &r));
  CHECK(md_stats_session(&s, false, &r));
  CHECK(r.minimum == 0 && r.maximum == 70 && r.count == 71 && r.rate == 1);
  md_stats_threshold_time(&s, 60, 500);
  CHECK(s.above_ms == 500);
  md_stats_threshold_time(&s, 80, 250);
  CHECK(s.below_ms == 250);
  md_stats_reset_trip(&s);
  CHECK(!md_stats_session(&s, true, &r));
  CHECK(md_stats_observe(&s, 10, 71000));
  CHECK(md_stats_session(&s, true, &r) && r.count == 1 && r.maximum == 10);
  md_stats_disconnect(&s);
  CHECK(!md_stats_window(&s, 71000, 60, &r));
  CHECK(md_stats_observe(&s, 20, 72000));
  CHECK(!s.has_rate);
  CHECK(!md_stats_observe(&s, INFINITY, 73000));
  md_stats_init(&s);
  CHECK(md_stats_observe(&s, 1, UINT32_MAX - 100));
  CHECK(md_stats_observe(&s, 2, 100));
  CHECK(s.has_rate && fabs(s.rate - 1000.0 / 201) < 0.001);
  CHECK(md_stats_window(&s, 100, 1, &r) && r.count == 2);
}
static void event_tests(void) {
  init();
  config.rules[6] = (md_analytics_rule_t){.enabled = true,
                                          .low = true,
                                          .threshold = -2,
                                          .hysteresis = .5,
                                          .debounce_ms = 500,
                                          .cooldown_ms = 2000};
  CHECK(md_derived_configure(&e, &config, 0));
  retard(0, 0, 0, 0, 0);
  md_derived_tick(&e, &store, 0);
  expect(MD_SIGNAL_RETARD_CYL, 0, 0);
  retard(-1, -4, 0, -2, 100);
  md_derived_tick(&e, &store, 100);
  expect(MD_SIGNAL_RETARD_WORST, -4, 100);
  expect(MD_SIGNAL_RETARD_AVG, -1.75, 100);
  expect(MD_SIGNAL_RETARD_CYL, 2, 100);
  md_derived_tick(&e, &store, 599);
  expect(MD_SIGNAL_KNOCK_COUNT, 0, 599);
  md_derived_tick(&e, &store, 600);
  expect(MD_SIGNAL_KNOCK_COUNT, 1, 600);
  md_derived_tick(&e, &store, 1000);
  expect(MD_SIGNAL_KNOCK_COUNT, 1, 1000);
  retard(0, 0, 0, 0, 1100);
  md_derived_tick(&e, &store, 1100);
  retard(0, -4, 0, 0, 1200);
  md_derived_tick(&e, &store, 1200);
  md_derived_tick(&e, &store, 1700);
  expect(MD_SIGNAL_KNOCK_COUNT, 1, 1700);
  retard(0, 0, 0, 0, 2700);
  md_derived_tick(&e, &store, 2700);
  retard(0, -5, 0, 0, 2800);
  md_derived_tick(&e, &store, 2800);
  md_derived_tick(&e, &store, 3300);
  expect(MD_SIGNAL_KNOCK_COUNT, 2, 3300);
  expect(MD_SIGNAL_RETARD_TRIP_PEAK, -5, 3300);
  md_analytics_event_t ev[32];
  CHECK(md_derived_events(&e, ev, 32) == 2);
  CHECK(ev[0].type == MD_EVENT_RETARD && ev[0].value == -4);
  for (unsigned i = 0; i < 100; i++)
    md_derived_can_error(&e, &store, 10000 + i * 1000, i);
  CHECK(e.event_count == 32 && e.event_overwritten == 70);
  CHECK(md_derived_events(&e, ev, 3) == 3 && ev[2].value == 99);
  md_derived_reset_trip(&e, 120000);
  CHECK(e.trip_retard_events == 0 && e.rules[6].count == 2);
  md_derived_reset_session(&e, 120000);
  CHECK(e.event_count == 0 && e.rules[6].count == 0);
  md_derived_can_error(&e, &store, 120100, 1);
  md_derived_can_error(&e, &store, 120200, 2);
  CHECK(e.event_count == 1 && e.events_rate_limited == 1);
  md_derived_can_error(&e, &store, 121100, 3);
  CHECK(e.event_count == 2);
}
static void counter_tests(void) {
  init();
  for (unsigned i = 0; i < 4; i++) {
    config.counter_bits[i] = 32;
    config.counter_wrap_allowed[i] = true;
  }
  config.idle_enabled = true;
  config.idle_rpm_min = 500;
  config.idle_rpm_max = 1200;
  config.load_enabled = true;
  config.load_minimum = 70;
  CHECK(md_derived_configure(&e, &config, 0));
  value(MD_SIGNAL_ENGINE_RPM, 800, 0);
  value(MD_SIGNAL_VEHICLE_SPEED, 0, 0);
  value(MD_SIGNAL_ENGINE_LOAD, 80, 0);
  misfire(4294967294.0, 0, 0, 0, 0);
  md_derived_tick(&e, &store, 0);
  expect(MD_SIGNAL_MISFIRE_DELTA, 0, 0);
  expect(MD_SIGNAL_MISFIRE_TOTAL, 4294967294.0, 0);
  misfire(2, 3, 0, 0, 1000);
  md_derived_tick(&e, &store, 1000);
  expect(MD_SIGNAL_MISFIRE_DELTA, 7, 1000);
  expect(MD_SIGNAL_MISFIRE_RATE, 420, 1000);
  CHECK(e.counter_wraps == 1);
  expect(MD_SIGNAL_MISFIRE_CYL, 1, 1000);
  expect(MD_SIGNAL_MISFIRE_IDLE, 7, 1000);
  misfire(1, 0, 0, 0, 2000);
  md_derived_tick(&e, &store, 2000);
  expect(MD_SIGNAL_MISFIRE_DELTA, 0, 2000);
  expect(MD_SIGNAL_MISFIRE_TRIP, 7, 2000);
  CHECK(e.counter_resets == 2);
  misfire(2, 0, 0, 0, 3000);
  md_derived_tick(&e, &store, 3000);
  expect(MD_SIGNAL_MISFIRE_TRIP, 8, 3000);
  md_derived_reset_trip(&e, 3100);
  misfire(3, 0, 0, 0, 4000);
  md_derived_tick(&e, &store, 4000);
  expect(MD_SIGNAL_MISFIRE_TRIP, 1, 4000);
  expect(MD_SIGNAL_MISFIRE_SESSION, 9, 4000);
  md_derived_tick(&e, &store, 9000);
  CHECK(snap(MD_SIGNAL_MISFIRE_RATE, 9000).state == MD_UI_SIGNAL_STALE);
  misfire(100, 50, 0, 0, 10000);
  md_derived_tick(&e, &store, 10000);
  expect(MD_SIGNAL_MISFIRE_DELTA, 0, 10000);
  expect(MD_SIGNAL_MISFIRE_TRIP, 1, 10000);
  CHECK(!snap(MD_SIGNAL_MISFIRE_RATE, 10000).value_available);
  misfire(1.5, 0, 0, 0, 11000);
  md_derived_tick(&e, &store, 11000);
  CHECK(!snap(MD_SIGNAL_MISFIRE_DELTA, 11000).value_available &&
        e.rejected_counters > 0);
  init();
  misfire(10, 0, 0, 0, 0);
  md_derived_tick(&e, &store, 0);
  CHECK(!snap(MD_SIGNAL_MISFIRE_RATE, 0).value_available);
}
static void timer_tests(void) {
  md_performance_timer_t t;
  CHECK(!md_performance_init(&t, 100, 50, 1));
  CHECK(md_performance_init(&t, 80, 120, 1));
  CHECK(!md_performance_observe(&t, true, 78, 0));
  CHECK(t.state == MD_RUN_ARMED);
  CHECK(!md_performance_observe(&t, true, 82, 200));
  CHECK(t.state == MD_RUN_RUNNING);
  CHECK(md_performance_observe(&t, true, 122, 1000));
  CHECK(fabs(t.result_s - .86) < .0001);
  md_performance_observe(&t, false, 130, 1200);
  CHECK(!t.result_valid && t.state == MD_RUN_INVALID);
  CHECK(md_performance_init(&t, 0, 100, 1));
  md_performance_observe(&t, true, 0, 0);
  md_performance_observe(&t, true, 10, 1000);
  md_performance_observe(&t, true, 20, 2001);
  CHECK(t.state == MD_RUN_INVALID);
  md_performance_observe(&t, true, 0, 3000);
  md_performance_observe(&t, true, 30, 3200);
  md_performance_observe(&t, true, 20, 3400);
  CHECK(t.state == MD_RUN_INVALID);
  CHECK(md_performance_init(&t, 0, 100, 1));
  md_performance_observe(&t, true, 0, UINT32_MAX - 399);
  md_performance_observe(&t, true, 50, UINT32_MAX - 199);
  CHECK(md_performance_observe(&t, true, 100, 0));
  CHECK(fabs(t.result_s - .4) < .00001);
  init();
  for (unsigned i = 0; i <= 50; i++) {
    value(MD_SIGNAL_VEHICLE_SPEED, i * 2, i * 200);
    md_derived_tick(&e, &store, i * 200);
  }
  expect(MD_SIGNAL_PERF_0100, 10, 10000);
  expect(MD_SIGNAL_PERF_COUNT, 3, 10000); /* 0-50, 0-100, 60-100 */
  md_derived_tick(&e, &store, 15000);
  CHECK(!snap(MD_SIGNAL_PERF_0100, 15000).value_available);
}
static void reseal(uint8_t *data, size_t n) {
  uint32_t crc = UINT32_MAX;
  for(size_t i=0;i<n-4;i++) {
    crc ^= data[i];
    for(unsigned j=0;j<8;j++) crc=(crc>>1)^(0xedb88320U & (0U-(crc&1)));
  }
  crc=~crc;
  for(unsigned i=0;i<4;i++) data[n-4+i]=crc>>(i*8);
}
static void config_ui_tests(void) {
  static md_device_configuration_t c, d;
  static uint8_t wire[MD_CONFIG_WIRE_MAX];
  size_t n;
  md_ui_settings_t s;
  md_ui_settings_defaults(&s);
  CHECK(md_configuration_from_legacy(&c, &s, NULL));
  c.common.pages[0].widgets[0].signal = MD_SIGNAL_MISFIRE_RATE;
  c.common.gauge_ranges[MD_SIGNAL_RETARD_WORST].minimum = -10;
  CHECK(md_configuration_encode(&c, wire, sizeof(wire), &n));
  CHECK(md_configuration_decode(wire, n, &d));
  CHECK(d.common.pages[0].widgets[0].signal == MD_SIGNAL_MISFIRE_RATE);
  c.common.analytics.rules[0].enabled=true;
  c.common.analytics.rules[0].threshold=123.5;
  c.common.analytics.xy_enabled=true;
  c.common.analytics.xy_from=80;
  c.common.analytics.xy_to=120;
  CHECK(md_configuration_encode(&c, wire, sizeof(wire), &n));
  CHECK(md_configuration_decode(wire, n, &d));
  CHECK(d.common.analytics.rules[0].enabled && d.common.analytics.rules[0].threshold==123.5 && d.common.analytics.xy_to==120);
  c.common.analytics.counter_bits[0]=16;
  CHECK(!md_configuration_validate(&c,NULL));
  c.common.analytics.counter_bits[0]=0;
  for(unsigned p=0;p<MD_ANALYTICS_PRESET_COUNT;p++) {
    CHECK(md_configuration_analytics_preset(&c,0,p));
    CHECK(c.common.pages[0].widget_count==8 && md_configuration_validate(&c,NULL));
  }
  CHECK(!md_configuration_analytics_preset(&c,0,MD_ANALYTICS_PRESET_COUNT));
  /* Historical schema 3 came in BOTH 12-range UI and 14-range standalone
   * variants. Construct real old wire tails, not schema-4 lookalikes. */
  CHECK(md_configuration_from_legacy(&c, &s, NULL));
  c.common.gauge_auto=false;
  c.common.gauge_ranges[0].maximum=10000;
  CHECK(md_configuration_encode(&c,wire,sizeof(wire),&n));
  size_t count_offset=n-4-MD_CONFIG_ANALYTICS_WIRE_SIZE-MD_SIGNAL_COUNT*8-1;
  static uint8_t old[MD_CONFIG_WIRE_MAX];
  for(unsigned count=12;count<=14;count+=2) {
    memcpy(old,wire,count_offset);
    old[4]=3;
    memcpy(old+count_offset,wire+count_offset+1,count*8);
    size_t old_size=count_offset+count*8+4;
    reseal(old,old_size);
    CHECK(md_configuration_decode(old,old_size,&d));
    CHECK(d.common.gauge_ranges[0].maximum==10000 && !d.common.gauge_auto && !d.common.analytics.rules[0].enabled);
    CHECK(d.common.gauge_ranges[MD_SIGNAL_ECU_SUPPLY_VOLTAGE].maximum==18);
  }
  memcpy(old,wire,n);old[count_offset]=MD_SIGNAL_COUNT+1;reseal(old,n);
  CHECK(!md_configuration_decode(old,n,&d));
  CHECK(md_ui_settings_bind_widget_signal(&s, 0, MD_SIGNAL_RETARD_WORST));
  s.warnings_enabled = true;
  s.warnings[0] =
      (md_ui_warning_threshold_t){.enabled = true,
                                  .signal_id = MD_SIGNAL_RETARD_WORST,
                                  .direction = MD_UI_THRESHOLD_LOW,
                                  .warning_milli = -2000,
                                  .critical_milli = -5000};
  CHECK(md_ui_signal_severity(&s, MD_SIGNAL_RETARD_WORST, -6,
                              MD_UI_SIGNAL_VALID) == MD_UI_SEVERITY_CRITICAL);
  CHECK(md_ui_signal_severity(&s, MD_SIGNAL_RETARD_WORST, -6,
                              MD_UI_SIGNAL_STALE) == MD_UI_SEVERITY_NORMAL);
  md_ui_signal_snapshot_t v = {
      .state = MD_UI_SIGNAL_VALID, .value_available = true, .value = 10};
  char val[32], unit[12];
  s.temperature_unit = MD_UI_TEMP_FAHRENHEIT;
  md_ui_format_signal(md_ui_signal_definition(MD_SIGNAL_IAT_DELTA), &v, &s, val,
                      sizeof(val), unit, sizeof(unit));
  CHECK(strcmp(val, "18.0") == 0);
  init();
  config.rules[0].threshold = NAN;
  CHECK(!md_analytics_config_valid(&config));
  config.rules[0].threshold = 0;
  config.counter_bits[0] = 17;
  CHECK(!md_analytics_config_valid(&config));
}
static void bounds_tests(void) {
  init();
  watch = true;
  for (unsigned i = 0; i < 20000; i++) {
    md_ui_signal_store_publish_id(&store, MD_SIGNAL_ENGINE_RPM, i % 6000,
                                  MD_UI_SIGNAL_VALID, i * 100, 1000, false);
    md_derived_tick(&e, &store, i * 100);
  }
  watch = false;
  CHECK(allocations == 0);
  CHECK(e.event_count <= 32);
  CHECK(sizeof(e) < 65000);
  CHECK(e.statistics[MD_SIGNAL_ENGINE_RPM].session_count == 20000);
}
int main(void) {
  dependency_tests();
  statistics_tests();
  event_tests();
  counter_tests();
  timer_tests();
  config_ui_tests();
  bounds_tests();
  printf("ANALYTICS PASS checks=%u engine=%zu stats=%zu event=%zu store=%zu "
         "allocations=%u\n",
         checks, sizeof(e), sizeof(md_window_statistics_t),
         sizeof(md_analytics_event_t), sizeof(store), allocations);
  return 0;
}
