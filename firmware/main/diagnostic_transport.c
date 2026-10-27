#include "diagnostic_transport.h"
#include <string.h>

void md_tp_reset(md_tp_rx_t *rx) { memset(rx, 0, sizeof(*rx)); }
bool md_tp_expired(const md_tp_rx_t *rx, uint32_t now_ms)
{ return rx && rx->receiving && (uint32_t)(now_ms - rx->last_ms) >= MD_TP_N_CR_MS; }

bool md_tp_parse_flow_control(const uint8_t *d,size_t n,md_tp_flow_control_t *out)
{
    if(!d || !out || n<3 || n>8 || d[0]<0x30 || d[0]>0x32) return false;
    uint32_t stmin;
    if(d[2]<=0x7f) stmin=(uint32_t)d[2]*1000U;
    else if(d[2]>=0xf1 && d[2]<=0xf9) stmin=(uint32_t)(d[2]-0xf0)*100U;
    else return false; /* Reserved STmin values must not silently become zero. */
    *out=(md_tp_flow_control_t){.status=d[0]&15,.block_size=d[1],.stmin_us=stmin};
    return true;
}

md_tp_result_t md_tp_receive(md_tp_rx_t *rx, const md_can_frame_t *f, uint32_t now)
{
    if (!rx || !f || f->extended || f->remote || f->dlc == 0 || f->dlc > 8) return MD_TP_INVALID;
    uint8_t type = f->data[0] >> 4;
    if (type == 0) {
        uint8_t n = f->data[0] & 15;
        if (n == 0 || n > 7 || n > f->dlc - 1U || rx->receiving) return MD_TP_INVALID;
        memcpy(rx->data, f->data + 1, n);
        rx->used = rx->length = n;
        return MD_TP_COMPLETE;
    }
    if (type == 1) {
        if (f->dlc != 8) return MD_TP_INVALID;
        uint16_t n = ((uint16_t)(f->data[0] & 15) << 8) | f->data[1];
        if (rx->receiving || n <= 7 || n > MD_DIAG_PAYLOAD_MAX) return MD_TP_INVALID;
        memcpy(rx->data, f->data + 2, 6);
        rx->used = 6; rx->length = n; rx->sequence = 1;
        rx->receiving = true; rx->last_ms = now;
        return MD_TP_FLOW;
    }
    if (type == 2) {
        if (!rx->receiving || md_tp_expired(rx, now) ||
            (f->data[0] & 15) != rx->sequence) return MD_TP_INVALID;
        size_t n = rx->length - rx->used;
        if (n > 7) n = 7;
        /* Intermediate CFs are full; the final CF may omit padding. */
        if (f->dlc < n + 1U) return MD_TP_INVALID;
        memcpy(rx->data + rx->used, f->data + 1, n);
        rx->used += n; rx->sequence = (rx->sequence + 1) & 15;
        rx->last_ms = now;
        if (rx->used == rx->length) { rx->receiving = false; return MD_TP_COMPLETE; }
        return MD_TP_MORE;
    }
    if(type==3) {
        md_tp_flow_control_t fc;
        /* This is a receiver-only transport: valid FC has no sender to resume.
         * Application requests fit SF; never mistake FC for a UDS response. */
        return md_tp_parse_flow_control(f->data,f->dlc,&fc)?MD_TP_IGNORE:MD_TP_INVALID;
    }
    return MD_TP_INVALID; /* Reserved PCI types. */
}
