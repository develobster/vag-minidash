#include "vag_can_db_nvs.h"
#include "ui_settings_nvs.h"
#include "nvs.h"
bool vag_db_config_load(vag_db_config_t *c)
{
    if(!c) return false;
    vag_db_config_defaults(c);
    if(!md_ui_settings_storage_init()) return false;
    nvs_handle_t h;esp_err_t err=nvs_open("minidash_vag",NVS_READONLY,&h);
    if(err==ESP_ERR_NVS_NOT_FOUND) return true;
    if(err!=ESP_OK) return false;
    uint8_t wire[12];size_t size=sizeof(wire);
    err=nvs_get_blob(h,"profile",wire,&size);nvs_close(h);
    if(err==ESP_ERR_NVS_NOT_FOUND) return true;
    if(err!=ESP_OK || size!=sizeof(wire) || wire[0]!='V' || wire[1]!='G' || wire[2]!=1) return false;
    unsigned checksum=0;for(unsigned i=0;i<11;i++) checksum+=wire[i];
    if((checksum&255U)!=wire[11]) return false;
    vag_db_config_t decoded={.platform=wire[3],.layout=wire[4],
        .shift_rpm=(uint16_t)(wire[5]|((uint16_t)wire[6]<<8)),
        .soot_limit_decigrams=(uint16_t)(wire[7]|((uint16_t)wire[8]<<8))};
    if(!vag_db_config_valid(&decoded) || wire[9] || wire[10]) return false;
    *c=decoded;return true;
}
bool vag_db_config_save(const vag_db_config_t *c)
{
    if(!vag_db_config_valid(c) || !md_ui_settings_storage_init()) return false;
    uint8_t wire[12]={'V','G',1,(uint8_t)c->platform,(uint8_t)c->layout,
        c->shift_rpm&255,c->shift_rpm>>8,c->soot_limit_decigrams&255,c->soot_limit_decigrams>>8,0,0,0};
    unsigned checksum=0;for(unsigned i=0;i<11;i++) checksum+=wire[i];wire[11]=checksum&255;
    nvs_handle_t h;esp_err_t err=nvs_open("minidash_vag",NVS_READWRITE,&h);
    if(err!=ESP_OK) return false;
    err=nvs_set_blob(h,"profile",wire,sizeof(wire));
    if(err==ESP_OK) err=nvs_commit(h);
    nvs_close(h);return err==ESP_OK;
}
