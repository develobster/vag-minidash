#include "derived_engine.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static md_derived_engine_t engine;
static md_ui_signal_store_t store;
static bool integer(const char *s, uint32_t *v) {
  if (!s || !s[0] || s[0] == '-')
    return false;
  char *end;
  errno = 0;
  unsigned long n = strtoul(s, &end, 10);
  if (errno || *end || n > UINT32_MAX)
    return false;
  *v = (uint32_t)n;
  return true;
}
static void synthetic_config(md_analytics_config_t *c) {
  /* Test choices ONLY: no engine/vehicle safety recommendation. */
  for (unsigned i = 0; i < 4; i++) {
    c->counter_bits[i] = 16;
    c->counter_wrap_allowed[i] = true;
  }
  const double thresholds[] = {100, -100, 100, 120, 110, 12, -2, 10};
  for (unsigned i = 0; i < 8; i++) {
    c->rules[i].enabled = true;
    c->rules[i].threshold = thresholds[i];
    c->rules[i].hysteresis = i == 5 ? .2 : 0;
    c->rules[i].debounce_ms = 500;
    c->rules[i].cooldown_ms = 2000;
  }
  c->idle_enabled = true;
  c->idle_rpm_min = 500;
  c->idle_rpm_max = 1200;
  c->load_enabled = true;
  c->load_minimum = 70;
  c->warmup_enabled = true;
  c->warmup_target = 80;
  c->heat_soak_enabled = true;
  c->heat_soak_delta = 20;
  c->xy_enabled = true;
  c->xy_from = 60;
  c->xy_to = 100;
}
int main(int argc, char **argv) {
  md_analytics_config_t config;
  md_analytics_config_defaults(&config);
  if (argc == 2 && !strcmp(argv[1], "--synthetic"))
    synthetic_config(&config);
  else if (argc != 1)
    return 2;
  md_ui_signal_store_init(&store);
  if (!md_derived_init(&engine, &config, 0))
    return 2;
  char line[256];
  while (fgets(line, sizeof(line), stdin)) {
    if (!strchr(line, '\n') && !feof(stdin))
      return 2;
    char cmd[8], a[48], b[48], c[48], d[48], f[48], extra;
    int n = sscanf(line, "%7s %47s %47s %47s %47s %47s %c", cmd, a, b, c, d, f,
                   &extra);
    uint32_t ms, ttl, state;
    if (n < 2 || !integer(a, &ms))
      return 2;
    if (!strcmp(cmd, "S") && n == 6) {
      if (!integer(b, &ttl) || !ttl || ttl > 60000 || !integer(c, &state) ||
          state > MD_UI_SIGNAL_ERROR)
        return 2;
      int id = md_ui_signal_id_from_name(d);
      if (id < 0 || id >= MD_SIGNAL_RAW_COUNT || id == MD_SIGNAL_ENGINE_BOOST)
        return 2;
      char *end;
      errno = 0;
      double value = strtod(f, &end);
      if (errno || *end)
        return 2;
      if (!md_ui_signal_store_publish_id(&store, id, value, state, ms, ttl,
                                         false))
        return 2;
    } else if (!strcmp(cmd, "T") && n == 2) {
      md_derived_tick(&engine, &store, ms);
      for (unsigned id = 0; id < MD_SIGNAL_COUNT; id++) {
        md_ui_signal_snapshot_t s = md_ui_signal_snapshot(&store, id, ms);
        printf("S %u %s %u %.17g %u\n", ms,
               md_ui_signal_definitions[id].canonical_name, s.state, s.value,
               s.value_available);
      }
      printf("E %u %llu %u %u %u\n", ms, (unsigned long long)engine.event_total,
             engine.event_count, engine.event_overwritten,
             engine.events_rate_limited);
    } else if (!strcmp(cmd, "R") && n == 3) {
      if (!strcmp(b, "trip"))
        md_derived_reset_trip(&engine, ms);
      else if (!strcmp(b, "session"))
        md_derived_reset_session(&engine, ms);
      else
        return 2;
    } else if (!strcmp(cmd, "C") && n == 2)
      md_derived_can_error(&engine, &store, ms, 1);
    else
      return 2;
  }
  return ferror(stdin) ? 2 : 0;
}
