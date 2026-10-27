#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define MD_DIAG_PAYLOAD_MAX 128U
#define MD_TP_N_CR_MS 250U
typedef struct { uint8_t status, block_size; uint32_t stmin_us; } md_tp_flow_control_t;
bool md_tp_parse_flow_control(const uint8_t *data, size_t length, md_tp_flow_control_t *out);
typedef struct {
    uint32_t id;
    uint8_t data[8], dlc;
    bool extended, remote;
} md_can_frame_t;
typedef struct {
    uint8_t data[MD_DIAG_PAYLOAD_MAX], sequence;
    uint16_t length, used;
    uint32_t last_ms;
    bool receiving;
} md_tp_rx_t;
typedef enum { MD_TP_IGNORE, MD_TP_INVALID, MD_TP_FLOW, MD_TP_MORE, MD_TP_COMPLETE } md_tp_result_t;
void md_tp_reset(md_tp_rx_t *rx);
bool md_tp_expired(const md_tp_rx_t *rx, uint32_t now_ms);
md_tp_result_t md_tp_receive(md_tp_rx_t *rx, const md_can_frame_t *frame, uint32_t now_ms);
