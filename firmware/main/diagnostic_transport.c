#include "diagnostic_transport.h"
#include <string.h>

void md_tp_reset(md_tp_rx_t *rx) { memset(rx, 0, sizeof(*rx)); }
bool md_tp_expired(const md_tp_rx_t *rx, uint32_t now_ms)
{ return rx->receiving && (uint32_t)(now_ms - rx->last_ms) >= 250U; }

md_tp_result_t md_tp_receive(md_tp_rx_t *rx, const md_can_frame_t *f, uint32_t now)
{
    if (!rx || !f || f->extended || f->remote || f->dlc != 8) return MD_TP_INVALID;
    uint8_t type = f->data[0] >> 4;
    if (type == 0) {
        uint8_t n = f->data[0] & 15;
        if (n == 0 || n > 7 || rx->receiving) return MD_TP_INVALID;
        memcpy(rx->data, f->data + 1, n);
        rx->used = rx->length = n;
        return MD_TP_COMPLETE;
    }
    if (type == 1) {
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
        memcpy(rx->data + rx->used, f->data + 1, n);
        rx->used += n; rx->sequence = (rx->sequence + 1) & 15;
        rx->last_ms = now;
        if (rx->used == rx->length) { rx->receiving = false; return MD_TP_COMPLETE; }
        return MD_TP_MORE;
    }
    return MD_TP_IGNORE;
}
