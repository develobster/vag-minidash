#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "can_error_probe.h"
typedef struct {
    uint32_t schema,uptime_ms,rx,tx,tx_failed,timeouts,arbitration,bus_errors,peak_tec,peak_rec;
    uint32_t rx_standard,rx_extended,last_id,last_extended;
    md_can_error_counts_t errors;
    char firmware[32];
} md_can_incident_t;
bool md_can_incident_save(const md_can_incident_t *record);
void md_can_incident_dump(void);
#ifdef MINIDASH_USB_CAN_BENCH
bool md_can_incident_selftest(const md_can_incident_t *record);
#endif
