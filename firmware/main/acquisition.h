#pragma once
#include "diagnostic_transport.h"
#include "ui_core.h" /* Existing portable canonical catalogue; no LVGL/RTOS. */

#define MD_ACQ_MAX_BINDINGS 16U
#define MD_ACQ_TRACE_SIZE 96U
#define MD_ACQ_DTC_MAX 12U
typedef enum { MD_SOURCE_PASSIVE, MD_SOURCE_UDS, MD_SOURCE_OBD, MD_SOURCE_COUNT } md_source_t;
typedef enum { MD_ACQ_LISTEN, MD_ACQ_DISCOVERY, MD_ACQ_CAPABILITIES,
               MD_ACQ_VIN, MD_ACQ_VERIFY_ECU, MD_ACQ_RUNNING, MD_ACQ_QUIET, MD_ACQ_FAULT } md_acq_state_t;
typedef struct {
    uint32_t id;
    uint8_t start_bit, bit_count, dlc;
    bool signed_value;
    float scale, offset;
    md_ui_signal_id_t signal;
    uint32_t ttl_ms;
} md_passive_binding_t; /* Intel bit order only. */
typedef struct {
    uint16_t did;
    uint8_t bytes;
    float scale, offset;
    md_ui_signal_id_t signal;
    uint32_t period_ms, ttl_ms;
    bool signed_value;
} md_uds_binding_t; /* ReadDataByIdentifier, evidence-defined big endian scalars. */
typedef struct {
    /* Compiled evidence, not a user checkbox. Review licence, exact VIN, ECU
     * and addressing before provisioning. CAN IDs alone never identify a car. */
    const char *vin, *provenance, *source_license;
    bool addressing_verified, definitions_verified;
    uint16_t request_id, response_id, identity_did;
    uint8_t identity_length, identity_expected[32];
    const md_passive_binding_t *passive;
    size_t passive_count;
    const md_uds_binding_t *uds;
    size_t uds_count;
} md_vehicle_profile_t;
typedef struct {
    bool allow_standard_obd; /* ISO 15765-4 11-bit/500 kbit preset ONLY. */
    uint32_t startup_ms;
    const md_vehicle_profile_t *profile;
} md_acq_config_t;
typedef struct { double value; uint32_t updated_ms, ttl_ms; bool valid; } md_acq_sample_t;
typedef struct { md_can_frame_t frame; uint32_t ms; bool tx; } md_acq_trace_t;
typedef bool (*md_acq_send_t)(void *context, const md_can_frame_t *frame);
typedef struct {
    md_acq_config_t config;
    md_acq_send_t send;
    void *send_context;
    md_acq_state_t state;
    md_tp_rx_t tp;
    md_acq_sample_t samples[MD_SIGNAL_RAW_COUNT][MD_SOURCE_COUNT];
    uint32_t supported[3], responders[8], visible, background;
    uint32_t due[MD_ACQ_MAX_BINDINGS], uds_due[MD_ACQ_MAX_BINDINGS];
    uint8_t failures[MD_ACQ_MAX_BINDINGS], uds_failures[MD_ACQ_MAX_BINDINGS];
    uint32_t started_ms, last_tx_ms, request_ms, progress_ms, quiet_ms, idle_since_ms;
    uint32_t rx_count, tx_count, timeout_count, malformed_count, rejected_count;
    uint32_t tx_failed, trace_overwritten;
    uint16_t request_id, response_id, active_rx, active_tx;
    uint8_t ecu_count, attempts, consecutive_failures, cursor, pending_count;
    uint8_t service, pid, job, cap_block;
    bool active, did_request, vehicle_matched, profile_matched, dtc_requested, dtc_valid, dtc_truncated;
    bool bus_available, vin_attempted, sent_any, engine_idle;
    char vin[18];
    uint16_t dtc[MD_ACQ_DTC_MAX];
    uint8_t dtc_count;
    uint32_t dtc_ms;
    md_acq_trace_t trace[MD_ACQ_TRACE_SIZE];
    uint16_t trace_head, trace_count;
} md_acquisition_t;

void md_acq_init(md_acquisition_t *a, const md_acq_config_t *config,
                 md_acq_send_t send, void *context, uint32_t now_ms);
void md_acq_tick(md_acquisition_t *a, uint32_t now_ms);
void md_acq_receive(md_acquisition_t *a, const md_can_frame_t *f, uint32_t now_ms);
void md_acq_bus_available(md_acquisition_t *a, bool available, uint32_t now_ms);
void md_acq_restart(md_acquisition_t *a, uint32_t now_ms);
void md_acq_set_demand(md_acquisition_t *a, uint32_t visible, uint32_t background);
bool md_acq_request_dtc(md_acquisition_t *a, uint32_t now_ms);
md_ui_signal_snapshot_t md_acq_snapshot(const md_acquisition_t *a,
    md_ui_signal_id_t signal, uint32_t now_ms, md_source_t *source);
const char *md_acq_state_name(md_acq_state_t state);
/* Shared production/replay source bridge. Derived values never enter here.
 * sources may be NULL or an array of at least MD_SIGNAL_RAW_COUNT entries. */
void md_acq_publish_canonical(const md_acquisition_t *a,
    md_ui_signal_store_t *store, md_source_t *sources, uint32_t now_ms);
void md_acq_format_dtc(uint16_t code, char text[6]);
bool md_acq_profile_valid(const md_vehicle_profile_t *profile);
