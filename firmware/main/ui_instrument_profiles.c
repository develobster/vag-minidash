#include "ui_instrument_profiles.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Generated numeric table; provenance and variant scope live in data/ui/. */
#include "ui_instrument_profiles.inc"

const md_instrument_profile_t *md_instrument_profile_find(const char *id)
{
    if (!id) return NULL;
    for (unsigned i=0;i<sizeof(profiles)/sizeof(profiles[0]);i++)
        if (strcmp(id,profiles[i].id)==0) return &profiles[i];
    return NULL;
}
const md_instrument_profile_t *md_instrument_profile_default(void) { return &profiles[0]; }
bool md_instrument_profile_range(const md_instrument_profile_t *p, md_ui_signal_id_t signal,
    float *low, float *high)
{
    if (!p || !low || !high) return false;
    if (signal==MD_SIGNAL_ENGINE_RPM) { *low=0; *high=p->rpm_max; }
    else if (signal==MD_SIGNAL_VEHICLE_SPEED) { *low=0; *high=p->speed_max; }
    else if (signal==MD_SIGNAL_ENGINE_COOLANT_TEMPERATURE && p->coolant_max>p->coolant_min)
        { *low=p->coolant_min; *high=p->coolant_max; }
    else return false;
    return true;
}
bool md_instrument_frame_decode(const char *frame, char *id, size_t capacity, uint32_t *ttl_ms)
{
    if (!frame || !id || !ttl_ms) return false;
    char name[48], ttl_text[12]; int consumed=0;
    if (sscanf(frame,"MDV1|%47[^|]|%11[0-9]%n",name,ttl_text,&consumed)!=2 ||
        frame[consumed]!='\0' || strlen(name)>=capacity ||
        (strcmp(name,"default")!=0 && !md_instrument_profile_find(name))) return false;
    char *end; errno=0; unsigned long ttl=strtoul(ttl_text,&end,10);
    if (errno || *end || ttl<1 || ttl>60000) return false;
    memcpy(id,name,strlen(name)+1); *ttl_ms=(uint32_t)ttl; return true;
}
bool md_instrument_select(md_instrument_selection_t *s, const char *id, uint32_t ttl, uint32_t now)
{
    if (!s || !id || !ttl || ttl>60000) return false;
    const md_instrument_profile_t *p=md_instrument_profile_find(id);
    if (!p && strcmp(id,"default")!=0) return false;
    *s=(md_instrument_selection_t){p,now,ttl}; return true;
}
const md_instrument_profile_t *md_instrument_active(const md_instrument_selection_t *s,
    uint32_t now, bool *matched)
{
    const bool fresh=s && s->profile && s->ttl_ms && now-s->updated_ms<s->ttl_ms;
    if (matched) *matched=fresh;
    return fresh ? s->profile : md_instrument_profile_default();
}
