#pragma once
#include "diagnostic_transport.h"
#include "ui_core.h"

typedef enum { PLATFORM_PQ35_46=0, PLATFORM_MQB=1 } vag_platform_t;
typedef enum { VAG_LAYOUT_SOURCED=0, VAG_LAYOUT_REQUESTED_EXPERIMENTAL=1 } vag_layout_t;
typedef enum {
    VAG_RPM, VAG_SPEED, VAG_THROTTLE, VAG_COOLANT, VAG_OIL, VAG_IAT,
    VAG_DSG_TEMP, VAG_GEAR, VAG_BOOST, VAG_REGEN, VAG_SOOT,
    VAG_DPF_PRESSURE, VAG_REGEN_REMAINING, VAG_HEADLIGHTS, VAG_TORQUE,
    VAG_STEERING, VAG_EGT, VAG_GEAR_SELECTOR, VAG_CHARGE_PRESSURE,
    VAG_SIGNAL_COUNT
} vag_signal_t;
typedef enum { VAG_UNAVAILABLE, VAG_CANDIDATE, VAG_VALID, VAG_STALE, VAG_ERROR } vag_quality_t;
typedef struct {
    /* Units: rpm, km/h, %, degC, bar gauge (BOOST), hPa differential (DPF),
     * grams (SOOT), Nm, degrees. CHARGE_PRESSURE has unknown reference until
     * independently evidenced, and must never be used as gauge boost. */
    double value[VAG_SIGNAL_COUNT];
    uint32_t stamp_ms[VAG_SIGNAL_COUNT], ttl_ms[VAG_SIGNAL_COUNT];
    uint32_t observed, trusted, errors, passive;
    vag_platform_t platform;
    vag_layout_t layout;
} vehicle_telemetry_t;
typedef struct {
    vag_platform_t platform;
    vag_layout_t layout;
    uint16_t shift_rpm, soot_limit_decigrams; /* 0 disables inferred DPF headroom. */
} vag_db_config_t;
typedef struct vag_can_db {
    vehicle_telemetry_t telemetry;
    uint32_t validation_mask, rejected;
} vag_can_db_t;
typedef struct {
    uint16_t can_id;
    uint8_t platform, layout, signal, dlc, start_bit, bits, little, signed_value;
    int8_t sign_bit, valid_bit, mux_start;
    uint8_t valid_value, mux_bits, mux_value;
    uint32_t all_bits_mask;
    double scale, offset;
    int8_t term_start;
    uint8_t term_bits;
    double term_scale, minimum, maximum;
    uint32_t ttl_ms, invalid_raw;
    bool has_invalid_raw;
    const char *source;
} vag_can_definition_t;
extern const vag_can_definition_t vag_can_definitions[];
extern const size_t vag_can_definition_count;
extern const char *const vag_signal_names[VAG_SIGNAL_COUNT];
void vag_db_config_defaults(vag_db_config_t *config);
bool vag_db_config_valid(const vag_db_config_t *config);
void vag_can_db_init(vag_can_db_t *db, const vag_db_config_t *config);
void vag_can_db_validation(vag_can_db_t *db, uint32_t validated_signals);
bool vag_can_unpack(const uint8_t *data, size_t dlc, unsigned start, unsigned bits,
                    bool little, bool sign, uint32_t *raw, double *scalar);
size_t vag_can_db_decode(vag_can_db_t *db, const md_can_frame_t *frame, uint32_t stamp_ms);
bool vag_can_db_publish(vag_can_db_t *db, vag_signal_t signal, double value,
                        uint32_t stamp_ms, uint32_t ttl_ms, bool trusted, bool passive);
vag_quality_t vag_telemetry_sample(const vehicle_telemetry_t *t, vag_signal_t signal,
                                  uint32_t now_ms, double *value);
void vag_can_db_merge_canonical(vag_can_db_t *db, const md_ui_signal_store_t *store,
                                uint32_t now_ms, uint16_t soot_limit_decigrams);
const char *vag_platform_name(vag_platform_t platform);
