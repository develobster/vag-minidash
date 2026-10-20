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
    uint32_t arbitration_lost, bus_errors, tx_error_counter, rx_error_counter;
    uint32_t peak_tx_error_counter, peak_rx_error_counter;
    bool dtc_valid, dtc_truncated;
    uint32_t dtc_ms;
    uint16_t dtc[MD_ACQ_DTC_MAX];
    md_ui_signal_snapshot_t signals[MD_SIGNAL_COUNT];
    md_source_t sources[MD_SIGNAL_COUNT];
} md_vehicle_status_t;
typedef struct {
    md_vehicle_status_t status;
    uint32_t timestamp_ms, trace_sequence, exchange_sequence, exchange_overwritten;
    uint32_t supported[3];
    bool validation_mode, standard_obd;
    uint16_t trace_count, exchange_count;
    md_acq_trace_t trace[MD_ACQ_TRACE_SIZE];
    md_acq_exchange_t exchanges[MD_ACQ_EXCHANGE_SIZE];
} md_vehicle_validation_t;
/* Boot thread only: prepare before exposing status/trace to UI or HTTP;
 * start the producer only after the display store has been initialized. */
void md_vehicle_app_prepare(void);
void md_vehicle_app_start(void);
#if CONFIG_MINIDASH_CYD
bool md_vehicle_app_demo(void);
bool md_vehicle_app_tx_armed(void);
bool md_vehicle_app_set_demo(bool enabled);
bool md_vehicle_app_confirm_wiring(bool confirmed);
void md_vehicle_app_disarm(void);
void md_vehicle_app_mode_dump(void);
#endif
bool md_vehicle_app_status(md_vehicle_status_t *out);
void md_vehicle_app_demand(uint32_t visible, uint32_t background);
void md_vehicle_app_scan(void);
size_t md_vehicle_app_trace(md_acq_trace_t *out, size_t capacity);
/* One locked snapshot; export consumers never issue CAN requests. */
bool md_vehicle_app_validation(md_vehicle_validation_t *out);
bool md_vehicle_app_analytics_configure(const md_analytics_config_t *config);
void md_vehicle_app_analytics_reset(bool session);
size_t md_vehicle_app_events(md_analytics_event_t *out,size_t capacity);
