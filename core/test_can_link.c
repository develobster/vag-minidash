#include "can_link_tx.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"CAN LINK FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static TickType_t ticks;
static uint32_t stale,events[8];
static unsigned event_count,event_index,sends,stops;
static bool submitted;
static esp_err_t queue_result;
TickType_t xTaskGetTickCount(void){return ticks;}
esp_err_t twai_transmit_v2(twai_handle_t h,const twai_message_t *f,TickType_t timeout)
{(void)h;(void)f;CHECK(timeout==0);sends++;submitted=queue_result==ESP_OK;return queue_result;}
esp_err_t twai_stop_v2(twai_handle_t h){(void)h;stops++;submitted=false;return ESP_OK;}
esp_err_t twai_read_alerts_v2(twai_handle_t h,uint32_t *out,TickType_t timeout)
{
    (void)h;
    if(!submitted){*out=stale;stale=0;return *out?ESP_OK:ESP_ERR_TIMEOUT;}
    if(event_index<event_count){ticks++;*out=events[event_index++];return ESP_OK;}
    ticks+=timeout;*out=0;return ESP_ERR_TIMEOUT;
}
static void init(void)
{ticks=stale=event_count=event_index=sends=stops=0;submitted=false;queue_result=ESP_OK;}
int main(void)
{
    twai_message_t frame={0};uint32_t deferred;
    init();deferred=0;CHECK(md_can_transmit_confirmed(NULL,&frame,&deferred)==ESP_ERR_TIMEOUT);
    CHECK(ticks==20 && sends==1 && stops==1);
    init();stale=TWAI_ALERT_TX_SUCCESS;deferred=0;
    CHECK(md_can_transmit_confirmed(NULL,&frame,&deferred)==ESP_ERR_TIMEOUT && stops==1);
    init();events[0]=TWAI_ALERT_TX_FAILED;event_count=1;deferred=0;
    CHECK(md_can_transmit_confirmed(NULL,&frame,&deferred)==ESP_FAIL && sends==1 && stops==0);
    CHECK(deferred==TWAI_ALERT_TX_FAILED);
    init();events[0]=TWAI_ALERT_TX_SUCCESS;event_count=1;deferred=0;
    CHECK(md_can_transmit_confirmed(NULL,&frame,&deferred)==ESP_OK && sends==1 && stops==0);
    init();stale=TWAI_ALERT_RX_QUEUE_FULL;events[0]=TWAI_ALERT_BUS_OFF|TWAI_ALERT_TX_SUCCESS;
    event_count=1;deferred=0;
    CHECK(md_can_transmit_confirmed(NULL,&frame,&deferred)==ESP_FAIL);
    CHECK(deferred==(TWAI_ALERT_RX_QUEUE_FULL|TWAI_ALERT_BUS_OFF));
    init();stale=TWAI_ALERT_BUS_OFF;deferred=0;
    CHECK(md_can_transmit_confirmed(NULL,&frame,&deferred)==ESP_FAIL && sends==0);
    CHECK(deferred==TWAI_ALERT_BUS_OFF);
    init();events[0]=TWAI_ALERT_RX_QUEUE_FULL;events[1]=TWAI_ALERT_TX_SUCCESS;
    event_count=2;deferred=0;
    CHECK(md_can_transmit_confirmed(NULL,&frame,&deferred)==ESP_OK && deferred==TWAI_ALERT_RX_QUEUE_FULL);
    init();queue_result=ESP_FAIL;deferred=0;
    CHECK(md_can_transmit_confirmed(NULL,&frame,&deferred)==ESP_FAIL && sends==1 && stops==0 && ticks==0);
    init();ticks=UINT32_MAX-7;deferred=0;
    CHECK(md_can_transmit_confirmed(NULL,&frame,&deferred)==ESP_ERR_TIMEOUT && stops==1 && ticks==12);
    puts("CAN LINK PASS: confirmed completion, stale/error alerts, bounded abort, no hidden retries, clock wrap");
    return 0;
}
