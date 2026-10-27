#include "vag_can_db.h"
#include <math.h>
#include <string.h>
#define BIT(s) (UINT32_C(1)<<(s))
_Static_assert(VAG_SIGNAL_COUNT<=32, "Telemetry mask width");
#include "vag_can_db_generated.inc"

void vag_db_config_defaults(vag_db_config_t *c)
{ if(c) *c=(vag_db_config_t){.platform=PLATFORM_MQB,.layout=VAG_LAYOUT_SOURCED,.shift_rpm=6500}; }
bool vag_db_config_valid(const vag_db_config_t *c)
{ return c && (unsigned)c->platform<=PLATFORM_MQB && (unsigned)c->layout<=VAG_LAYOUT_REQUESTED_EXPERIMENTAL &&
    c->shift_rpm>=1000 && c->shift_rpm<=12000 && c->soot_limit_decigrams<=2000; }
void vag_can_db_init(vag_can_db_t *db,const vag_db_config_t *c)
{
    if(!db) return;
    memset(db,0,sizeof(*db));
    vag_db_config_t defaults;if(!vag_db_config_valid(c)) {vag_db_config_defaults(&defaults);c=&defaults;}
    db->telemetry.platform=c->platform;db->telemetry.layout=c->layout;
}
void vag_can_db_validation(vag_can_db_t *db,uint32_t mask)
{
    if(!db) return;
    db->validation_mask=db->telemetry.layout==VAG_LAYOUT_SOURCED?mask:0;
    db->telemetry.trusted&=db->validation_mask|~db->telemetry.passive;
}
bool vag_can_unpack(const uint8_t *data,size_t dlc,unsigned start,unsigned bits,
                    bool little,bool sign,uint32_t *raw,double *scalar)
{
    if(!data || !raw || !scalar || !dlc || dlc>8 || !bits || bits>32 || start>=dlc*8) return false;
    uint32_t result=0;
    for(unsigned i=0,bit=start;i<bits;i++) {
        if(bit>=dlc*8) return false;
        unsigned value=(data[bit/8]>>(bit%8))&1U;
        if(little) {result|=(uint32_t)value<<i;++bit;}
        else {result=(result<<1)|value;bit=bit%8==0?bit+15:bit-1;}
    }
    *raw=result;*scalar=result;
    if(sign && (result&(UINT32_C(1)<<(bits-1)))) *scalar-=(double)(UINT64_C(1)<<bits);
    return true;
}
bool vag_can_db_publish(vag_can_db_t *db,vag_signal_t signal,double value,uint32_t stamp,
                        uint32_t ttl,bool trusted,bool passive)
{
    if(!db || (unsigned)signal>=VAG_SIGNAL_COUNT || !isfinite(value) || !ttl || ttl>60000) return false;
    vehicle_telemetry_t *t=&db->telemetry;uint32_t bit=BIT(signal);
    t->value[signal]=value;t->stamp_ms[signal]=stamp;t->ttl_ms[signal]=ttl;
    t->observed|=bit;t->errors&=~bit;
    if(trusted) t->trusted|=bit;else t->trusted&=~bit;
    if(passive) t->passive|=bit;else t->passive&=~bit;
    return true;
}
size_t vag_can_db_decode(vag_can_db_t *db,const md_can_frame_t *f,uint32_t stamp)
{
    if(!db || !f) return 0;
    if(f->extended || f->remote || !f->dlc || f->dlc>8 || f->id>0x7ff) {++db->rejected;return 0;}
    vehicle_telemetry_t *t=&db->telemetry;size_t count=0;
    for(size_t i=0;i<vag_can_definition_count;i++) {
        const vag_can_definition_t *d=&vag_can_definitions[i];
        if(d->platform!=t->platform || d->layout!=t->layout || d->can_id!=f->id) continue;
        uint32_t raw;double scalar;
        if(d->mux_start>=0) {
            if(!vag_can_unpack(f->data,f->dlc,(unsigned)d->mux_start,d->mux_bits,true,false,&raw,&scalar) || raw!=d->mux_value) continue;
        }
        bool ok=f->dlc==d->dlc && vag_can_unpack(f->data,f->dlc,d->start_bit,d->bits,d->little,d->signed_value,&raw,&scalar);
        if(ok && d->valid_bit>=0) ok=((f->data[d->valid_bit/8]>>(d->valid_bit%8))&1)==d->valid_value;
        if(ok && d->has_invalid_raw && raw==d->invalid_raw) ok=false;
        if(ok) {
            if(d->all_bits_mask) scalar=(raw&d->all_bits_mask)==d->all_bits_mask;
            scalar=scalar*d->scale+d->offset;
            if(d->sign_bit>=0 && ((f->data[d->sign_bit/8]>>(d->sign_bit%8))&1)) scalar=-scalar;
            if(d->term_start>=0) {
                double term;uint32_t unused;
                ok=vag_can_unpack(f->data,f->dlc,(unsigned)d->term_start,d->term_bits,true,false,&unused,&term);
                if(ok) scalar+=term*d->term_scale;
            }
            ok=ok && scalar>=d->minimum && scalar<=d->maximum;
        }
        if(!ok) {t->observed&=~BIT(d->signal);t->trusted&=~BIT(d->signal);t->errors|=BIT(d->signal);++db->rejected;continue;}
        (void)vag_can_db_publish(db,(vag_signal_t)d->signal,scalar,stamp,d->ttl_ms,
            (db->validation_mask&BIT(d->signal))!=0,true);++count;
    }
    return count;
}
vag_quality_t vag_telemetry_sample(const vehicle_telemetry_t *t,vag_signal_t signal,uint32_t now,double *value)
{
    if(!t || (unsigned)signal>=VAG_SIGNAL_COUNT) return VAG_UNAVAILABLE;
    uint32_t bit=BIT(signal);
    if(t->errors&bit) return VAG_ERROR;
    if(!(t->observed&bit)) return VAG_UNAVAILABLE;
    if(now-t->stamp_ms[signal]>=t->ttl_ms[signal]) return VAG_STALE;
    if(value) *value=t->value[signal];
    return (t->trusted&bit)?VAG_VALID:VAG_CANDIDATE;
}
void vag_can_db_merge_canonical(vag_can_db_t *db,const md_ui_signal_store_t *store,uint32_t now,uint16_t soot_limit)
{
    static const struct {vag_signal_t vag;md_ui_signal_id_t id;double scale;} map[]={
        {VAG_RPM,MD_SIGNAL_ENGINE_RPM,1},{VAG_SPEED,MD_SIGNAL_VEHICLE_SPEED,1},
        {VAG_COOLANT,MD_SIGNAL_ENGINE_COOLANT_TEMPERATURE,1},{VAG_OIL,MD_SIGNAL_ENGINE_OIL_TEMPERATURE,1},
        {VAG_IAT,MD_SIGNAL_IAT,1},{VAG_DSG_TEMP,MD_SIGNAL_TRANSMISSION_OIL_TEMPERATURE,1},
        {VAG_GEAR,MD_SIGNAL_TRANSMISSION_GEAR,1},{VAG_BOOST,MD_SIGNAL_ENGINE_BOOST,.001}};
    if(!db || !store) return;
    for(size_t i=0;i<sizeof(map)/sizeof(map[0]);i++) {
        vag_signal_t id=map[i].vag;
        if(vag_telemetry_sample(&db->telemetry,id,now,NULL)==VAG_VALID && (db->telemetry.passive&BIT(id))) continue;
        md_ui_signal_snapshot_t s=md_ui_signal_snapshot(store,map[i].id,now);
        const md_ui_signal_record_t *record=&store->signals[map[i].id];
        if(s.state==MD_UI_SIGNAL_VALID && !s.demo)
            (void)vag_can_db_publish(db,id,s.value*map[i].scale,now-s.age_ms,record->ttl_ms,true,false);
        else if(!(db->telemetry.passive&BIT(id))) db->telemetry.observed&=~BIT(id);
    }
    double soot;
    vag_quality_t q=vag_telemetry_sample(&db->telemetry,VAG_SOOT,now,&soot);
    if(soot_limit && (q==VAG_VALID || q==VAG_CANDIDATE)) {
        double remaining=fmax(0,fmin(100,100*(1-soot/(soot_limit*.1))));
        (void)vag_can_db_publish(db,VAG_REGEN_REMAINING,remaining,db->telemetry.stamp_ms[VAG_SOOT],
            db->telemetry.ttl_ms[VAG_SOOT],q==VAG_VALID,false);
    } else db->telemetry.observed&=~BIT(VAG_REGEN_REMAINING);
}
const char *vag_platform_name(vag_platform_t p)
{ return p==PLATFORM_MQB?"MQB":p==PLATFORM_PQ35_46?"PQ35/46":"UNKNOWN"; }
