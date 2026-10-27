#include "vag_can_db_nvs.h"
#include "nvs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"VAG NVS FAIL %d: %s\n",__LINE__,#x);abort();}}while(0)
static uint8_t blob[12];
static size_t length;
static bool exists,storage=true;
static int failure,opens,closes,commits;
bool md_ui_settings_storage_init(void) {return storage;}
esp_err_t nvs_open(const char *name,int mode,nvs_handle_t *h)
{ CHECK(!strcmp(name,"minidash_vag"));CHECK(mode==NVS_READONLY || mode==NVS_READWRITE);if(failure==1)return ESP_FAIL;*h=7;++opens;return ESP_OK; }
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *size)
{CHECK(h==7 && !strcmp(key,"profile"));if(!exists)return ESP_ERR_NVS_NOT_FOUND;if(failure==2)return ESP_FAIL;CHECK(*size>=length);memcpy(out,blob,length);*size=length;return ESP_OK;}
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *data,size_t size)
{CHECK(h==7 && !strcmp(key,"profile") && size==12);if(failure==2)return ESP_FAIL;memcpy(blob,data,size);length=size;exists=true;return ESP_OK;}
esp_err_t nvs_commit(nvs_handle_t h) {CHECK(h==7);++commits;return failure==3?ESP_FAIL:ESP_OK;}
void nvs_close(nvs_handle_t h) {CHECK(h==7);++closes;}
int main(void)
{
    vag_db_config_t config,read;vag_db_config_defaults(&config);
    CHECK(vag_db_config_load(&read) && read.platform==PLATFORM_MQB && read.shift_rpm==6500);
    config.platform=PLATFORM_PQ35_46;config.layout=VAG_LAYOUT_REQUESTED_EXPERIMENTAL;
    config.shift_rpm=5100;config.soot_limit_decigrams=240;
    CHECK(vag_db_config_save(&config) && commits==1);
    CHECK(vag_db_config_load(&read) && read.platform==config.platform && read.layout==config.layout && read.shift_rpm==5100 && read.soot_limit_decigrams==240);
    uint8_t backup[12];memcpy(backup,blob,12);
    for(unsigned i=0;i<12;i++) {blob[i]^=1;CHECK(!vag_db_config_load(&read) && read.platform==PLATFORM_MQB);memcpy(blob,backup,12);}
    length=11;CHECK(!vag_db_config_load(&read));length=12;
    for(failure=1;failure<=3;failure++) CHECK(!vag_db_config_save(&config));
    failure=0;storage=false;CHECK(!vag_db_config_save(&config) && !vag_db_config_load(&read));storage=true;
    config.shift_rpm=0;CHECK(!vag_db_config_save(&config));CHECK(opens==closes);
    puts("VAG NVS PASS: defaults, roundtrip, corruption, bounds and write/commit failures");
    return 0;
}
