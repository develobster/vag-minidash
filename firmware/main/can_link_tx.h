#pragma once
#include "driver/twai.h"
/* Exactly one single-shot frame in flight. Other alerts remain visible to the
 * owner. Timeout stops the controller and requires reinstall before reuse. */
esp_err_t md_can_transmit_confirmed(twai_handle_t can,const twai_message_t *frame,uint32_t *deferred_alerts);
