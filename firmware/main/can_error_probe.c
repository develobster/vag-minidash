#include "can_error_probe.h"
#include "esp_attr.h"
#include "esp_idf_version.h"
#include "freertos/FreeRTOS.h"
#include "hal/twai_hal.h"
#if ESP_IDF_VERSION != ESP_IDF_VERSION_VAL(5,5,5) || !CONFIG_IDF_TARGET_ESP32S3
#error "Review the read-only CAN error observer for this SDK/target before enabling it"
#endif

/* The pinned legacy driver hides error types. Observe the HAL result without
 * reading/rearming registers, changing events, or changing the SDK sources. */
static DRAM_ATTR md_can_error_counts_t counts;
static portMUX_TYPE probe_lock=portMUX_INITIALIZER_UNLOCKED;
uint32_t __real_twai_hal_get_events(twai_hal_context_t *context);
uint32_t IRAM_ATTR __wrap_twai_hal_get_events(twai_hal_context_t *context)
{
    uint32_t events=__real_twai_hal_get_events(context);
    if(events&TWAI_HAL_EVENT_BUS_ERR) {
        twai_error_flags_t flags=context->errors;
        portENTER_CRITICAL_ISR(&probe_lock);
        if(flags.ack_err)counts.ack++;
        else if(flags.bit_err)counts.bit++;
        else if(flags.form_err)counts.form++;
        else if(flags.stuff_err)counts.stuff++;
        else counts.other++;
        portEXIT_CRITICAL_ISR(&probe_lock);
    }
    return events;
}
void md_can_error_counts(md_can_error_counts_t *out)
{
    portENTER_CRITICAL(&probe_lock);*out=counts;portEXIT_CRITICAL(&probe_lock);
}
