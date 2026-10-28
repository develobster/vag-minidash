#include "ui_configuration_nvs.h"
#include "ui_settings_nvs.h"
#include "ui_touch_nvs.h"
#include "nvs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"CONFIG NVS FAIL %d: %s\n",__LINE__,#x); abort(); } } while(0)

static uint8_t stored[MD_CONFIG_WIRE_MAX];
static uint8_t pending[MD_CONFIG_WIRE_MAX];
static uint8_t touch[256];
static size_t stored_len;
static size_t pending_len;
static size_t touch_len;
static bool pending_touch;
static bool exists, storage_ok=true;
static int failure,writes,commits,opens,closes;
static md_ui_settings_t legacy;
bool md_ui_settings_storage_init(void) { return storage_ok; }
bool md_ui_settings_storage_load(md_ui_settings_t *out) { *out=legacy; return true; }
esp_err_t nvs_open(const char *name,int mode,nvs_handle_t *handle)
{
    CHECK(!strcmp(name,"minidash_ui")); CHECK(mode==NVS_READONLY || mode==NVS_READWRITE);
    if(failure==1) return ESP_FAIL;
    *handle=7; ++opens; return ESP_OK;
}
esp_err_t nvs_get_blob(nvs_handle_t handle,const char *key,void *out,size_t *length)
{
    CHECK(handle==7);
    bool is_touch=!strcmp(key,"touch_cal"); CHECK(is_touch || !strcmp(key,"configuration"));
    size_t size=is_touch?touch_len:stored_len;
    if(is_touch ? !touch_len : !exists) return ESP_ERR_NVS_NOT_FOUND;
    if(failure==2) return ESP_FAIL;
    if(out) { CHECK(*length>=size); memcpy(out,is_touch?touch:stored,size); }
    *length=size; return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t handle,const char *key,const void *data,size_t length)
{
    CHECK(handle==7 && length<=sizeof(stored));
    pending_touch=!strcmp(key,"touch_cal"); CHECK(pending_touch || !strcmp(key,"configuration"));
    if(pending_touch) CHECK(length<=sizeof(touch));
    ++writes; if(failure==2) return ESP_FAIL;
    memcpy(pending,data,length); pending_len=length; return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t handle) {
    CHECK(handle==7); ++commits;
    if(failure==3) return ESP_FAIL;
    if(pending_touch) { memcpy(touch,pending,pending_len); touch_len=pending_len; }
    else { memcpy(stored,pending,pending_len); stored_len=pending_len; exists=true; }
    return ESP_OK;
}
void nvs_close(nvs_handle_t handle) { CHECK(handle==7); ++closes; }
int main(void)
{
    md_target_profile_t target={.hardware=MD_CAP_COLOR|MD_CAP_WIFI|MD_CAP_DIMMING};
    md_ui_settings_defaults(&legacy); legacy.theme=MD_UI_THEME_PIXEL_ARCADE;
    md_device_configuration_t config,loaded;
    CHECK(md_configuration_storage_load(&loaded,&target));
    CHECK(loaded.common.theme==legacy.theme && writes==0 && commits==0);
    CHECK(md_configuration_from_legacy(&config,&legacy,&target));
    config.common.pages[0].title[0]='X'; config.platform.brightness=63;
    CHECK(md_configuration_storage_save(&config)); CHECK(writes==1 && commits==1);
    CHECK(md_configuration_storage_load(&loaded,&target));
    CHECK(loaded.common.theme==MD_UI_THEME_PIXEL_ARCADE && loaded.common.pages[0].title[0]=='X');
    CHECK(loaded.platform.brightness==63);
    md_touch_map_t map={.x={1,0,4},.y={0,1,5},.width=320,.height=240,.rotation=1},cal={0};
    CHECK(md_touch_map_storage_save(&map));
    CHECK(md_touch_map_storage_load(&cal,320,240,1));
    CHECK(cal.x[2]==4 && cal.y[2]==5);
    CHECK(!md_touch_map_storage_load(&cal,240,320,1));
    CHECK(!md_touch_map_storage_load(&cal,320,240,0));
    CHECK(md_configuration_storage_load(&loaded,&target) && loaded.common.pages[0].title[0]=='X');
    touch[touch_len-1]^=1;
    CHECK(!md_touch_map_storage_load(&cal,320,240,1));
    uint8_t backup[MD_CONFIG_WIRE_MAX]; memcpy(backup,stored,stored_len);
    size_t backup_len=stored_len; int before=writes,commits_before=commits;
    stored[stored_len-1]^=0x55;
    CHECK(!md_configuration_storage_load(&loaded,&target));
    CHECK(loaded.common.theme==legacy.theme && writes==before && commits==commits_before);
    CHECK(stored[stored_len-1]!=(backup[stored_len-1]));
    memcpy(stored,backup,backup_len);
    config.platform.brightness=75;
    for(failure=1;failure<=3;failure++) {
        CHECK(!md_configuration_storage_save(&config));
        CHECK(stored_len==backup_len && !memcmp(stored,backup,backup_len));
    }
    failure=0;storage_ok=false;CHECK(!md_configuration_storage_save(&config));
    CHECK(opens==closes);
    puts("CONFIG NVS PASS: legacy fallback, theme/page, calibration, corruption and failed writes");
    return 0;
}
