#pragma once
#include "acquisition.h"
#include "derived_engine.h"
typedef struct {
    bool configured, ready;
    md_acq_state_t state;
    char fault[32], vin[18];
    uint16_t ecu_id;
    uint8_t ecu_count, dtc_count;
    uint32_t rx, tx, tx_failed, timeouts, malformed, rejected, rx_dropped, bus_offs, trace_overwritten;
    bool dtc_valid, dtc_truncated;
    uint32_t dtc_ms;
    uint16_t dtc[MD_ACQ_DTC_MAX];
    md_ui_signal_snapshot_t signals[MD_SIGNAL_COUNT];
    md_source_t sources[MD_SIGNAL_COUNT];
} md_vehicle_status_t;
/* Boot thread only: prepare before exposing status/trace to UI or HTTP;
 * start the producer only after the display store has been initialized. */
void md_vehicle_app_prepare(void);
void md_vehicle_app_start(void);
bool md_vehicle_app_status(md_vehicle_status_t *out);
void md_vehicle_app_demand(uint32_t visible, uint32_t background);
void md_vehicle_app_scan(void);
size_t md_vehicle_app_trace(md_acq_trace_t *out, size_t capacity);
bool md_vehicle_app_analytics_configure(const md_analytics_config_t *config);
void md_vehicle_app_analytics_reset(bool session);
size_t md_vehicle_app_events(md_analytics_event_t *out,size_t capacity);
