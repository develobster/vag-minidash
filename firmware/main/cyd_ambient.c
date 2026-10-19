#include "cyd_ambient.h"
#include "sdkconfig.h"
#if CONFIG_MINIDASH_CYD
#include "esp_adc/adc_oneshot.h"
static adc_oneshot_unit_handle_t unit;
bool md_cyd_ambient_init(void)
{
    const adc_oneshot_unit_init_cfg_t config={.unit_id=ADC_UNIT_1};
    const adc_oneshot_chan_cfg_t channel={.atten=ADC_ATTEN_DB_0,.bitwidth=ADC_BITWIDTH_12};
    if(adc_oneshot_new_unit(&config,&unit)!=ESP_OK) return false;
    if(adc_oneshot_config_channel(unit,ADC_CHANNEL_6,&channel)==ESP_OK) return true;
    adc_oneshot_del_unit(unit); unit=NULL; return false;
}
bool md_cyd_ambient_read(uint16_t *raw)
{
    if(!unit || !raw) return false;
    unsigned total=0;
    for(unsigned i=0;i<16;i++) {
        int value;
        if(adc_oneshot_read(unit,ADC_CHANNEL_6,&value)!=ESP_OK) return false;
        total+=value;
    }
    *raw=total/16; return true;
}
#else
bool md_cyd_ambient_init(void) { return false; }
bool md_cyd_ambient_read(uint16_t *raw) { (void)raw; return false; }
#endif
