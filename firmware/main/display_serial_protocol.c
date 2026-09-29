#include "display_serial_protocol.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DISPLAY_SERIAL_MAX_TTL_MS 60000UL

static bool parse_state(const char *text, md_ui_signal_state_t *state)
{
    if (strcmp(text, "VALID") == 0) *state = MD_UI_SIGNAL_VALID;
    else if (strcmp(text, "STALE") == 0) *state = MD_UI_SIGNAL_STALE;
    else if (strcmp(text, "UNAVAILABLE") == 0) *state = MD_UI_SIGNAL_UNAVAILABLE;
    else if (strcmp(text, "UNSUPPORTED") == 0) *state = MD_UI_SIGNAL_UNSUPPORTED;
    else if (strcmp(text, "ERROR") == 0) *state = MD_UI_SIGNAL_ERROR;
    else return false;
    return true;
}

bool md_display_serial_sample_decode(const char *frame,
                                     md_display_serial_sample_t *sample)
{
    if (!frame || !sample) return false;

    char canonical[MD_UI_SIGNAL_NAME_LEN] = {0};
    char state_text[16] = {0};
    char ttl_text[12] = {0};
    char value_text[32] = {0};
    int consumed = 0;
    if (sscanf(frame, "MDS1|%47[^|]|%15[^|]|%11[^|]|%31[^|]%n",
               canonical, state_text, ttl_text, value_text, &consumed) != 4 ||
        consumed <= 0 || frame[consumed] != '\0' ||
        md_ui_signal_id_from_name(canonical) < 0) return false;

    md_display_serial_sample_t decoded = {0};
    if (!parse_state(state_text, &decoded.state)) return false;

    errno = 0;
    char *end = NULL;
    const unsigned long ttl = strtoul(ttl_text, &end, 10);
    if (errno != 0 || end == ttl_text || *end != '\0' || ttl == 0 ||
        ttl > DISPLAY_SERIAL_MAX_TTL_MS) return false;
    decoded.ttl_ms = (uint32_t)ttl;

    if (decoded.state == MD_UI_SIGNAL_VALID) {
        errno = 0;
        end = NULL;
        decoded.value = strtof(value_text, &end);
        if (errno == ERANGE || end == value_text || *end != '\0' ||
            !isfinite(decoded.value)) return false;
    } else if (strcmp(value_text, "-") != 0) {
        return false;
    }

    (void)snprintf(decoded.canonical_signal, sizeof(decoded.canonical_signal),
                   "%s", canonical);
    *sample = decoded;
    return true;
}
