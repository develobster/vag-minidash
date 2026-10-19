#include "ui_web.h"
#include "captive_dns.h"
#include "lwip/inet.h"
#include "ui_i18n.h"
#include "display_app.h"
#include "vehicle_app.h"
#include "validation_json.h"
#include "esp_app_desc.h"
#include "ui_configuration_nvs.h"
#include "ui_touch_nvs.h"
#include "sdkconfig.h"
#include "esp_http_server.h"
#include "esp_lvgl_port.h"
#include "esp_http_client.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdatomic.h>

#define BODY_MAX 16384
static httpd_handle_t s_server;
static esp_netif_t *s_netif;
static atomic_bool s_active, s_starting, s_stop;
static char s_ssid[32], s_password[24], s_token[33];
extern const unsigned char webui_start[] asm("_binary_webui_html_start");
extern const unsigned char webui_end[] asm("_binary_webui_html_end");
static const char *TAG = "md_web";
#if CONFIG_MINIDASH_CYD
static unsigned s_http_stack_min=8192;
#endif

bool md_ui_web_active(void) { return atomic_load(&s_active) || atomic_load(&s_starting); }
const char *md_ui_web_ssid(void) { return atomic_load(&s_active) ? s_ssid : "Starting..."; }
const char *md_ui_web_password(void) { return atomic_load(&s_active) ? s_password : ""; }
void md_ui_web_stop(void) { atomic_store(&s_stop, true); }

#if CONFIG_MINIDASH_CYD
static bool http_json_chunk(void *context,const char *bytes,size_t length)
{
    return httpd_resp_send_chunk(context,bytes,length)==ESP_OK;
}
#endif
static esp_err_t respond(httpd_req_t *req, cJSON *object)
{
#if CONFIG_MINIDASH_CYD
    if(!object)return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"allocation failed");
    httpd_resp_set_type(req,"application/json; charset=utf-8");
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    httpd_resp_set_hdr(req,"X-Content-Type-Options","nosniff");
    esp_err_t result=md_json_stream(object,http_json_chunk,req) ? ESP_OK : ESP_FAIL;
    cJSON_Delete(object);
    if(result==ESP_OK)result=httpd_resp_send_chunk(req,NULL,0);
    return result;
#else
    char *json = object ? cJSON_PrintUnformatted(object) : NULL;
    cJSON_Delete(object);
    if (!json) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "allocation failed");
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");
    esp_err_t result = httpd_resp_send(req,json,HTTPD_RESP_USE_STRLEN); free(json); return result;
#endif
}
static cJSON *signal_json(unsigned i)
{
            cJSON *o=cJSON_CreateObject(); const md_ui_signal_definition_t *d=&md_ui_signal_definitions[i];
            cJSON_AddStringToObject(o,"id",d->canonical_name); cJSON_AddStringToObject(o,"label",d->label);
            cJSON_AddStringToObject(o,"label_ru",md_ui_i18n_signal_label(MD_UI_LANGUAGE_RUSSIAN,i));
            cJSON_AddStringToObject(o,"unit",d->canonical_unit); cJSON_AddNumberToObject(o,"precision",d->decimals);
            cJSON_AddStringToObject(o,"provenance",md_signal_provenance(i));
            const md_derived_node_t *node=md_derived_definition(i);cJSON_AddBoolToObject(o,"derived",node!=NULL);
            cJSON *deps=cJSON_AddArrayToObject(o,"dependencies");if(node)for(unsigned j=0;j<node->dependency_count;j++)cJSON_AddItemToArray(deps,cJSON_CreateString(md_ui_signal_definitions[node->dependencies[j]].canonical_name));
    return o;
}
static esp_err_t signal_registry(httpd_req_t *req)
{
#if CONFIG_MINIDASH_CYD
    /* Stream the existing catalogue one record at a time. A 92-object cJSON
     * tree plus its printed copy cannot coexist with Wi-Fi in ESP32 SRAM. */
    httpd_resp_set_type(req,"application/json; charset=utf-8");
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    httpd_resp_set_hdr(req,"X-Content-Type-Options","nosniff");
    esp_err_t result=httpd_resp_send_chunk(req,"[",1);
    for(unsigned i=0;result==ESP_OK && i<MD_SIGNAL_COUNT;i++) {
        cJSON *object=signal_json(i);
        char *row=object ? cJSON_PrintUnformatted(object) : NULL;
        cJSON_Delete(object);
        if(!row) return ESP_ERR_NO_MEM;
        if(i) result=httpd_resp_send_chunk(req,",",1);
        if(result==ESP_OK) result=httpd_resp_send_chunk(req,row,strlen(row));
        free(row);
    }
    if(result==ESP_OK) result=httpd_resp_send_chunk(req,"]",1);
    if(result==ESP_OK) result=httpd_resp_send_chunk(req,NULL,0);
    return result;
#else
    cJSON *root=cJSON_CreateArray();
    for(unsigned i=0;i<MD_SIGNAL_COUNT;i++) cJSON_AddItemToArray(root,signal_json(i));
    return respond(req,root);
#endif
}
static esp_err_t get_handler(httpd_req_t *req)
{
    if (!strcmp(req->uri,"/")) {
        httpd_resp_set_type(req,"text/html; charset=utf-8");
        httpd_resp_set_hdr(req,"Cache-Control","no-store");
        httpd_resp_set_hdr(req,"Content-Security-Policy","default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; connect-src 'self'; img-src 'none'; frame-ancestors 'none'; base-uri 'none'");
        return httpd_resp_send(req,(const char *)webui_start,webui_end-webui_start);
    }
    cJSON *root = NULL;
    if (!strcmp(req->uri,"/api/session")) {
        root = cJSON_CreateObject(); cJSON_AddStringToObject(root,"token",s_token);
    } else if (!strcmp(req->uri,"/api/validation")) {
        md_vehicle_validation_t *snapshot=malloc(sizeof(*snapshot));
        if(!snapshot || !md_vehicle_app_validation(snapshot)) {
            free(snapshot);return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"vehicle snapshot unavailable");
        }
        root=md_validation_json(snapshot,esp_app_get_description()->version,(uint64_t)esp_timer_get_time()/1000);
        free(snapshot);
    } else if (!strcmp(req->uri,"/api/presets")) {
        root=md_analytics_presets_json();
    } else if (!strcmp(req->uri,"/api/analytics")) {
        root=cJSON_CreateObject();md_analytics_event_t *events=malloc(MD_EVENT_CAPACITY*sizeof(*events));
        if(!events){cJSON_Delete(root);return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"allocation failed");}
        size_t n=md_vehicle_app_events(events,MD_EVENT_CAPACITY);cJSON *rows=cJSON_AddArrayToObject(root,"events");
        cJSON_AddNumberToObject(root,"capacity",MD_EVENT_CAPACITY);
        for(size_t i=0;i<n;i++) {
            const md_analytics_event_t *v=&events[i];cJSON *row=cJSON_CreateObject();cJSON_AddNumberToObject(row,"timestamp_ms",v->timestamp_ms);cJSON_AddNumberToObject(row,"type",v->type);
            cJSON_AddStringToObject(row,"signal",md_ui_signal_definitions[v->signal].canonical_name);cJSON_AddNumberToObject(row,"value",v->value);cJSON_AddNumberToObject(row,"threshold",v->threshold);
            const char *names[]={"rpm","load","speed","map_boost"};const double values[]={v->rpm,v->load,v->speed,v->map_boost};cJSON *snapshot=cJSON_AddObjectToObject(row,"snapshot");
            for(unsigned j=0;j<4;j++){if(v->snapshot_valid&(1U<<j))cJSON_AddNumberToObject(snapshot,names[j],values[j]);else cJSON_AddNullToObject(snapshot,names[j]);}
            cJSON_AddItemToArray(rows,row);
        }
        free(events);
    } else if (!strcmp(req->uri,"/api/signals")) {
        return signal_registry(req);
    } else {
        md_device_configuration_t *config = malloc(sizeof(*config)); uint32_t revision; md_target_profile_t target;
        if (!config || !md_display_configuration_read(config,&revision,&target)) { free(config); return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"UI not ready"); }
        if (!strcmp(req->uri,"/api/config")) root = md_configuration_json(config,revision);
        else if (!strcmp(req->uri,"/api/defaults")) { md_configuration_reset_ui(config,&target); root=md_configuration_json(config,revision); }
        else if (!strcmp(req->uri,"/api/device")) {
            root=cJSON_CreateObject(); cJSON_AddStringToObject(root,"target",target.name);
            cJSON_AddNumberToObject(root,"hardware",target.hardware);
            cJSON_AddNumberToObject(root,"features",target.implemented_features);
            cJSON_AddNumberToObject(root,"width",target.display_width); cJSON_AddNumberToObject(root,"height",target.display_height);
            cJSON_AddBoolToObject(root,"dimming",md_target_has(&target,MD_CAP_DIMMING));
            cJSON_AddBoolToObject(root,"auto_brightness",md_target_has(&target,MD_CAP_AMBIENT_SOURCE));
            cJSON_AddNumberToObject(root,"maximum_pages",MD_CONFIG_MAX_PAGES);
#if CONFIG_MINIDASH_CYD
            cJSON_AddBoolToObject(root,"simulation",true);
            cJSON_AddBoolToObject(root,"derived_analytics",false);
            cJSON_AddBoolToObject(root,"psram",false);
#endif
        } else if (!strcmp(req->uri,"/api/layouts")) {
            root=cJSON_CreateArray();
            for (unsigned i=MD_UI_LAYOUT_SINGLE;i<MD_UI_LAYOUT_COUNT;++i) {
                md_ui_dashboard_page_t page={0}; md_configuration_set_layout(&page,i);
                cJSON *o=cJSON_CreateObject(); cJSON_AddNumberToObject(o,"id",i);
                cJSON *slots=cJSON_AddArrayToObject(o,"slots");
                for (unsigned j=0;j<page.widget_count;++j) {
                    cJSON *slot=cJSON_CreateObject(); md_ui_page_widget_t *w=&page.widgets[j];
                    cJSON_AddNumberToObject(slot,"column",w->column); cJSON_AddNumberToObject(slot,"row",w->row);
                    cJSON_AddNumberToObject(slot,"columns",w->columns); cJSON_AddNumberToObject(slot,"rows",w->rows);
                    cJSON_AddItemToArray(slots,slot);
                }
                cJSON_AddItemToArray(root,o);
            }
        } else if (!strcmp(req->uri,"/api/themes")) {
            root=cJSON_CreateArray();
            for (unsigned i=0;i<MD_UI_THEME_COUNT;++i) {
                config->common.theme=i; config->common.palette=md_ui_theme_palette(i,false);
                cJSON *o=md_configuration_json(config,revision);
                md_ui_theme_json_metadata(o,i);
                cJSON_AddItemToArray(root,o);
            }
        }
        free(config);
    }
    if (!root) return httpd_resp_send_err(req,HTTPD_404_NOT_FOUND,"not found");
    return respond(req,root);
}
static bool bounded_json(const char *text, size_t length)
{
    unsigned depth=0; bool quoted=false,escaped=false;
    for (size_t i=0;i<length;++i) {
        unsigned char c=(unsigned char)text[i];
        if (!c) return false;
        if (quoted) {
            if (escaped) {
                if (c=='u' && i+4<length && !memcmp(text+i+1,"0000",4)) return false;
                escaped=false;
            } else if (c=='\\') escaped=true;
            else if (c=='"') quoted=false;
        } else if (c=='"') quoted=true;
        else if (c=='{' || c=='[') { if (++depth>8) return false; }
        else if (c=='}' || c==']') { if (!depth) return false; --depth; }
    }
    return !quoted && !escaped && !depth;
}
static esp_err_t mutation_handler(httpd_req_t *req)
{
    char header[80];
    if (httpd_req_get_hdr_value_str(req,"X-MiniDash-Session",header,sizeof(header)) != ESP_OK || strcmp(header,s_token))
        return httpd_resp_send_err(req,HTTPD_403_FORBIDDEN,"session required");
    if (httpd_req_get_hdr_value_len(req,"Origin") &&
        (httpd_req_get_hdr_value_str(req,"Origin",header,sizeof(header)) != ESP_OK || strcmp(header,"http://192.168.4.1")))
        return httpd_resp_send_err(req,HTTPD_403_FORBIDDEN,"origin rejected");
    if (httpd_req_get_hdr_value_str(req,"Content-Type",header,sizeof(header)) != ESP_OK || strcmp(header,"application/json"))
        return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"JSON required");
    if (!req->content_len || req->content_len > BODY_MAX) return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"body too large");
    char *body=malloc(req->content_len+1);
    md_device_configuration_t *config=malloc(sizeof(*config));
    if (!body || !config) { free(body); free(config); return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"allocation failed"); }
    size_t received=0;
    while (received<req->content_len) {
        int n=httpd_req_recv(req,body+received,req->content_len-received);
        if (n<=0) { free(body); free(config); return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"incomplete request"); }
        received+=n;
    }
    body[received]=0;
    const char *end=NULL; cJSON *root=bounded_json(body,received) ? cJSON_ParseWithLengthOpts(body,received+1,&end,true) : NULL;
    uint32_t revision=0; bool parsed=md_configuration_parse_json(root,config,&revision);
    free(body); cJSON_Delete(root);
    if (!parsed) { free(config); return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"invalid configuration"); }
    const bool reset_trip=!strcmp(req->uri,"/api/reset-trip"),reset_session=!strcmp(req->uri,"/api/reset-session");
    int status=(reset_trip || reset_session)?md_display_analytics_reset(revision,reset_session):md_display_configuration_apply(config,revision,!strcmp(req->uri,"/api/config"),!strcmp(req->uri,"/api/cancel"));
    free(config);
    if (status!=200) {
        httpd_resp_set_status(req,status==409 ? "409 Conflict" : status==403 ? "403 Forbidden" : status==400 ? "400 Bad Request" : "500 Internal Server Error");
        return httpd_resp_send(req,"configuration rejected",HTTPD_RESP_USE_STRLEN);
    }
    md_device_configuration_t *current=malloc(sizeof(*current));
    if (!current || !md_display_configuration_read(current,&revision,NULL)) { free(current); return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"read failed"); }
    cJSON *result=md_configuration_json(current,revision); free(current); return respond(req,result);
}
/* On internal-SRAM CYD, do not overlap the configuration JSON tree with
 * LVGL scene construction/draw-task allocations. The signal source is independent.
 * S3 keeps its original concurrency and PSRAM allocation behaviour. */
static esp_err_t guarded_get_handler(httpd_req_t *req)
{
#if CONFIG_MINIDASH_CYD
    if(!lvgl_port_lock(1000)) {
        httpd_resp_set_status(req,"503 Service Unavailable");
        return httpd_resp_send(req,"display busy; retry",HTTPD_RESP_USE_STRLEN);
    }
#endif
    esp_err_t result=get_handler(req);
#if CONFIG_MINIDASH_CYD
    unsigned remaining=uxTaskGetStackHighWaterMark(NULL);
    if(remaining<s_http_stack_min)s_http_stack_min=remaining;
    lvgl_port_unlock();
#endif
    return result;
}
static esp_err_t guarded_mutation_handler(httpd_req_t *req)
{
#if CONFIG_MINIDASH_CYD
    if(!lvgl_port_lock(1000)) {
        httpd_resp_set_status(req,"503 Service Unavailable");
        return httpd_resp_send(req,"display busy; retry",HTTPD_RESP_USE_STRLEN);
    }
#endif
    esp_err_t result=mutation_handler(req);
#if CONFIG_MINIDASH_CYD
    unsigned remaining=uxTaskGetStackHighWaterMark(NULL);
    if(remaining<s_http_stack_min)s_http_stack_min=remaining;
    lvgl_port_unlock();
#endif
    return result;
}
#if CONFIG_MINIDASH_CYD
static esp_err_t portal_redirect(httpd_req_t *req, httpd_err_code_t error)
{
    (void)error;
    if(!strncmp(req->uri,"/api/",5)) return httpd_resp_send_err(req,HTTPD_404_NOT_FOUND,"unknown API");
    httpd_resp_set_status(req,"302 Found");
    httpd_resp_set_hdr(req,"Location","http://192.168.4.1/");
    return httpd_resp_send(req,"Open MiniDash: http://192.168.4.1/",HTTPD_RESP_USE_STRLEN);
}
#endif
static void web_task(void *argument)
{
    (void)argument;
    bool wifi_initialized=false, wifi_started=false;
#if CONFIG_MINIDASH_CYD
    int dns_fd=-1;
#endif
    ESP_LOGI(TAG,"AP start internal_free=%u largest=%u",(unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),(unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
    ESP_LOGI(TAG,"AP step: initialize network stack");
    esp_err_t error=esp_netif_init();
    if (error!=ESP_OK && error!=ESP_ERR_INVALID_STATE) goto cleanup;
    ESP_LOGI(TAG,"AP step: initialize event loop");
    error=esp_event_loop_create_default();
    if (error!=ESP_OK && error!=ESP_ERR_INVALID_STATE) goto cleanup;
    ESP_LOGI(TAG,"AP step: create access point interface");
    s_netif=esp_netif_create_default_wifi_ap(); if (!s_netif) goto cleanup;
#if CONFIG_MINIDASH_CYD
    esp_netif_dns_info_t dns={0};
    dns.ip.type=ESP_IPADDR_TYPE_V4;
    dns.ip.u_addr.ip4.addr=inet_addr("192.168.4.1");
    esp_netif_dhcps_stop(s_netif);
    error=esp_netif_set_dns_info(s_netif,ESP_NETIF_DNS_MAIN,&dns); if(error!=ESP_OK)goto cleanup;
    uint8_t offer_dns=1;
    error=esp_netif_dhcps_option(s_netif,ESP_NETIF_OP_SET,ESP_NETIF_DOMAIN_NAME_SERVER,&offer_dns,sizeof(offer_dns)); if(error!=ESP_OK)goto cleanup;
    error=esp_netif_dhcps_start(s_netif); if(error!=ESP_OK)goto cleanup;
#endif
    ESP_LOGI(TAG,"AP step: initialize Wi-Fi driver");
    wifi_init_config_t init=WIFI_INIT_CONFIG_DEFAULT();
    error=esp_wifi_init(&init); if (error!=ESP_OK) { ESP_LOGE(TAG,"Wi-Fi init: %s",esp_err_to_name(error)); goto cleanup; }
    wifi_initialized=true;
    uint8_t mac[6]; esp_wifi_get_mac(WIFI_IF_AP,mac);
    snprintf(s_ssid,sizeof(s_ssid),"MiniDash-%02X%02X%02X",mac[3],mac[4],mac[5]);
    snprintf(s_password,sizeof(s_password),"%s","12345678");
    for (unsigned i=0;i<4;++i) snprintf(s_token+i*8,9,"%08lx",(unsigned long)esp_random());
    wifi_config_t wifi={0}; snprintf((char *)wifi.ap.ssid,sizeof(wifi.ap.ssid),"%s",s_ssid);
    snprintf((char *)wifi.ap.password,sizeof(wifi.ap.password),"%s",s_password);
    wifi.ap.ssid_len=strlen(s_ssid); wifi.ap.channel=1; wifi.ap.max_connection=2;
    wifi.ap.authmode=WIFI_AUTH_WPA2_PSK; wifi.ap.pmf_cfg.capable=true;
    ESP_LOGI(TAG,"AP step: configure Wi-Fi");
    error=esp_wifi_set_storage(WIFI_STORAGE_RAM); if(error!=ESP_OK)goto cleanup;
    error=esp_wifi_set_mode(WIFI_MODE_AP); if(error!=ESP_OK)goto cleanup;
    error=esp_wifi_set_config(WIFI_IF_AP,&wifi); if(error!=ESP_OK)goto cleanup;
    ESP_LOGI(TAG,"AP step: start Wi-Fi");
    error=esp_wifi_start(); if(error!=ESP_OK)goto cleanup;
    wifi_started=true;
    esp_netif_ip_info_t ip;
    error=esp_netif_get_ip_info(s_netif,&ip); if(error!=ESP_OK)goto cleanup;
    ESP_LOGI(TAG,"AP address: " IPSTR,IP2STR(&ip.ip));
    httpd_config_t server=HTTPD_DEFAULT_CONFIG(); server.task_priority=2;
    server.stack_size=
#if CONFIG_MINIDASH_CYD
        8192;
#else
        12288;
#endif
     server.max_open_sockets=3; server.max_uri_handlers=16; server.lru_purge_enable=true;
    server.recv_wait_timeout=3; server.send_wait_timeout=3;
    ESP_LOGI(TAG,"AP step: start HTTP server");
    error=httpd_start(&s_server,&server); if(error!=ESP_OK)goto cleanup;
    static const char *const gets[]={"/","/api/session","/api/config","/api/signals","/api/device","/api/layouts","/api/themes","/api/defaults","/api/presets","/api/analytics","/api/validation"};
    for (unsigned i=0;i<sizeof(gets)/sizeof(gets[0]);++i) {
        httpd_uri_t uri={.uri=gets[i],.method=HTTP_GET,.handler=guarded_get_handler};
        error=httpd_register_uri_handler(s_server,&uri); if(error!=ESP_OK)goto cleanup;
    }
    static const char *const writes[]={"/api/config","/api/preview","/api/cancel","/api/reset-trip","/api/reset-session"};
    for (unsigned i=0;i<5;++i) {
        httpd_uri_t uri={.uri=writes[i],.method=i==0 ? HTTP_PUT : HTTP_POST,.handler=guarded_mutation_handler};
        error=httpd_register_uri_handler(s_server,&uri); if(error!=ESP_OK)goto cleanup;
    }
#if CONFIG_MINIDASH_CYD
    error=httpd_register_err_handler(s_server,HTTPD_404_NOT_FOUND,portal_redirect); if(error!=ESP_OK)goto cleanup;
    dns_fd=md_captive_dns_open(); if(dns_fd<0) {error=ESP_FAIL;goto cleanup;}
#endif
    atomic_store(&s_active,true); atomic_store(&s_starting,false);
    ESP_LOGI(TAG,"Configuration AP ready (credentials on device), expires in 600 seconds");
    const int64_t started=esp_timer_get_time();
    while (!atomic_load(&s_stop) && esp_timer_get_time()-started<600000000LL) {
#if CONFIG_MINIDASH_CYD
        md_captive_dns_poll(dns_fd);
#endif
        vTaskDelay(pdMS_TO_TICKS(50));
    }
cleanup:
#if CONFIG_MINIDASH_CYD
    md_captive_dns_close(dns_fd);
#endif
    if(error!=ESP_OK && error!=ESP_ERR_INVALID_STATE)
        ESP_LOGE(TAG,"AP start/operation failed: %s",esp_err_to_name(error));
    if (s_server) { httpd_stop(s_server); s_server=NULL; }
    /* Roll back a preview before another local editor can run. */
    md_device_configuration_t *config=malloc(sizeof(*config)); uint32_t revision;
    if (config && md_display_configuration_read(config,&revision,NULL))
        (void)md_display_configuration_apply(config,revision,false,true);
    free(config);
    if (wifi_started) esp_wifi_stop();
    if (wifi_initialized) esp_wifi_deinit();
    if (s_netif) { esp_netif_destroy_default_wifi(s_netif); s_netif=NULL; }
    atomic_store(&s_active,false); atomic_store(&s_starting,false);
    ESP_LOGI(TAG,"Configuration AP stopped"); vTaskDelete(NULL);
}
bool md_ui_web_start(void)
{
    bool expected=false;
    if (atomic_load(&s_active) || !atomic_compare_exchange_strong(&s_starting,&expected,true)) return false;
    if (!md_display_configuration_begin()) { atomic_store(&s_starting,false); return false; }
    atomic_store(&s_stop,false);
    if (xTaskCreate(web_task,"ui_config",8192,NULL,2,NULL)!=pdPASS) { atomic_store(&s_starting,false); return false; }
    return true;
}

typedef struct { char *body; size_t length; bool overflow, portal_redirect, discard; } http_capture_t;
static esp_err_t capture_event(esp_http_client_event_t *event)
{
    http_capture_t *capture=event->user_data;
#if CONFIG_MINIDASH_CYD
    if(event->event_id==HTTP_EVENT_ON_HEADER && event->header_key &&
       !strcasecmp(event->header_key,"Location") && !strcmp(event->header_value,"http://192.168.4.1/")) capture->portal_redirect=true;
#endif
    if (event->event_id==HTTP_EVENT_ON_DATA) {
#if CONFIG_MINIDASH_CYD
        if(capture->discard) return ESP_OK;
        if(!capture->body) capture->body=malloc(BODY_MAX);
        if(!capture->body) return ESP_ERR_NO_MEM;
#endif
        if (capture->length+event->data_len>=BODY_MAX) { capture->overflow=true; return ESP_FAIL; }
        memcpy(capture->body+capture->length,event->data,event->data_len);
        capture->length+=event->data_len; capture->body[capture->length]=0;
    }
    return ESP_OK;
}
static int request_impl(const char *path, esp_http_client_method_t method, const char *body,
                   const char *token, http_capture_t *capture, char **owned_body)
{
    char url[96]; snprintf(url,sizeof(url),"http://192.168.4.1%s",path);
    esp_http_client_config_t config={.url=url,.timeout_ms=5000,.event_handler=capture_event,.user_data=capture,
#if CONFIG_MINIDASH_CYD
        .disable_auto_redirect=true,
#endif
    };
    esp_http_client_handle_t client=esp_http_client_init(&config); if (!client) return -1;
    capture->length=0; capture->overflow=false; capture->portal_redirect=false;
#if CONFIG_MINIDASH_CYD
    /* A test client must not reserve a 16 KiB receive buffer while the server
     * constructs JSON. Only GET /api/config needs a retained response body. */
    free(capture->body); capture->body=NULL;
    capture->discard=method!=HTTP_METHOD_GET || strcmp(path,"/api/config")!=0;
#else
    capture->body[0]=0;
#endif
    esp_http_client_set_method(client,method);
    if (body) {
        esp_http_client_set_header(client,"Content-Type","application/json");
        if (token) esp_http_client_set_header(client,"X-MiniDash-Session",token);
        esp_http_client_set_post_field(client,body,strlen(body));
    }
    esp_err_t error;
#if CONFIG_MINIDASH_CYD
    if(owned_body) {
        /* The self-hosted client must release its printed request before the
         * server builds the parse tree. A phone does not occupy ESP32 RAM. */
        const int length=body ? strlen(body) : 0;
        error=esp_http_client_open(client,length);
        int sent=0;
        while(error==ESP_OK && sent<length) {
            int n=esp_http_client_write(client,body+sent,length-sent);
            if(n<=0) error=ESP_FAIL; else sent+=n;
        }
        free(*owned_body); *owned_body=NULL;
        if(error==ESP_OK && esp_http_client_fetch_headers(client)<0) error=ESP_FAIL;
        char sink[256]; int received;
        if(error==ESP_OK) {
            do { received=esp_http_client_read(client,sink,sizeof(sink)); } while(received>0);
            if(received<0) error=ESP_FAIL;
        }
        esp_http_client_close(client);
    } else error=esp_http_client_perform(client);
#else
    (void)owned_body;
    error=esp_http_client_perform(client);
#endif
    int status=error==ESP_OK && !capture->overflow ? esp_http_client_get_status_code(client) : -1;
    esp_http_client_cleanup(client); return status;
}
static int request(const char *path, esp_http_client_method_t method,const char *body,
                   const char *token,http_capture_t *capture)
{
    return request_impl(path,method,body,token,capture,NULL);
}
static int request_owned(const char *path,esp_http_client_method_t method,char **body,
                         const char *token,http_capture_t *capture)
{
#if CONFIG_MINIDASH_CYD
    return request_impl(path,method,*body,token,capture,body);
#else
    return request(path,method,*body,token,capture);
#endif
}
static cJSON *bench_configuration_json(const md_device_configuration_t *config,uint32_t revision)
{
#if CONFIG_MINIDASH_CYD
    (void)lvgl_port_lock(0);
#endif
    return md_configuration_json(config,revision);
}
static cJSON *bench_json_parse(const char *text)
{
#if CONFIG_MINIDASH_CYD
    (void)lvgl_port_lock(0);
#endif
    return cJSON_Parse(text);
}
static void bench_json_delete(cJSON *root)
{
    cJSON_Delete(root);
#if CONFIG_MINIDASH_CYD
    lvgl_port_unlock();
#endif
}
/* A direct bench read can race an asynchronous scene/NVS update. Wait for
 * the observable configuration, bounded; HTTP status and persistence checks
 * still fail normally. This is not a retry of a rejected write. */
static bool bench_configuration_read(md_device_configuration_t *config,uint32_t *revision,md_target_profile_t *target)
{
#if CONFIG_MINIDASH_CYD
    for(unsigned attempt=0;attempt<10;attempt++) {
        if(md_display_configuration_read(config,revision,target))return true;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    return false;
#else
    return md_display_configuration_read(config,revision,target);
#endif
}
static bool web_selftest_run(void)
{
#if CONFIG_MINIDASH_CYD
    if (!atomic_load(&s_active)) { printf("MINIDASH_WEB_TEST_REJECTED enable_in_Settings_first\n"); return false; }
#else
    if (md_ui_web_active()) { printf("MINIDASH_WEB_TEST_REJECTED already_active\n"); return false; }
#endif
    unsigned passed=0,failed=0;
    md_touch_map_t touch_before={0},touch_after={0};
    bool touch_loaded=md_touch_map_storage_load(&touch_before,320,240,CONFIG_MINIDASH_DISPLAY_ROTATION);
    const size_t heap_before=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
#if !CONFIG_MINIDASH_CYD
    if (!md_ui_web_start()) return false;
#endif
    for (unsigned i=0;i<50 && !atomic_load(&s_active);++i) vTaskDelay(pdMS_TO_TICKS(100));
    http_capture_t capture={
#if CONFIG_MINIDASH_CYD
        .body=NULL
#else
        .body=malloc(BODY_MAX)
#endif
    };
    md_device_configuration_t *original=malloc(sizeof(*original)), *candidate=malloc(sizeof(*candidate));
    uint32_t revision=0; char *json=NULL;
#define CHECK(NAME,EXPR) do { bool ok=(EXPR); printf("MINIDASH_WEB_TEST name=%s status=%s\n",NAME,ok?"PASS":"FAIL"); if (ok) ++passed; else ++failed; } while (0)
    if (!original || !candidate || !atomic_load(&s_active)
#if !CONFIG_MINIDASH_CYD
        || !capture.body
#endif
    ) { ++failed; goto done; }
#if CONFIG_MINIDASH_CYD
    CHECK("captive_dns_socket",md_captive_dns_selftest());
    CHECK("captive_http_redirect",request("/generate_204",HTTP_METHOD_GET,NULL,NULL,&capture)==302 && capture.portal_redirect);
#endif
    CHECK("http_config_get",request("/api/config",HTTP_METHOD_GET,NULL,NULL,&capture)==200);
    cJSON *root=bench_json_parse(capture.body);
    bool valid=md_configuration_parse_json(root,original,&revision); bench_json_delete(root);
    CHECK("http_json_roundtrip",valid); if (!valid) goto done;
    root=bench_configuration_json(original,revision); char *backup=cJSON_PrintUnformatted(root); bench_json_delete(root);
    if (backup) { printf("MINIDASH_WEB_TEST_BACKUP_JSON %s\n",backup); free(backup); }
    CHECK("http_signal_registry",request("/api/signals",HTTP_METHOD_GET,NULL,NULL,&capture)==200);
    CHECK("http_layout_presets",request("/api/layouts",HTTP_METHOD_GET,NULL,NULL,&capture)==200);
    CHECK("http_theme_presets",request("/api/themes",HTTP_METHOD_GET,NULL,NULL,&capture)==200);
    CHECK("http_defaults",request("/api/defaults",HTTP_METHOD_GET,NULL,NULL,&capture)==200);
    CHECK("http_device_capabilities",request("/api/device",HTTP_METHOD_GET,NULL,NULL,&capture)==200);
    CHECK("http_malformed_json",request("/api/preview",HTTP_METHOD_POST,"{}",s_token,&capture)==400);
    CHECK("http_nesting_limit",request("/api/preview",HTTP_METHOD_POST,"[[[[[[[[[0]]]]]]]]]",s_token,&capture)==400);
    CHECK("http_no_dtc_route",request("/api/dtc/clear",HTTP_METHOD_GET,NULL,NULL,&capture)==404);
    *candidate=*original;
    candidate->common.page_count=4;
    for (unsigned i=1;i<4;++i) candidate->common.pages[i]=candidate->common.pages[0];
    candidate->common.pages[1].widgets[0].type=MD_UI_WIDGET_BAR;
    candidate->common.pages[2].widgets[0].type=MD_UI_WIDGET_STATUS;
    candidate->common.pages[3].widgets[0].type=MD_UI_WIDGET_GRAPH;
    candidate->common.active_page=3;
    root=bench_configuration_json(candidate,revision); json=cJSON_PrintUnformatted(root); bench_json_delete(root);
    if (!json) { ++failed; goto done; }
    CHECK("http_session_gate",request("/api/preview",HTTP_METHOD_POST,json,"wrong",&capture)==403);
    CHECK("http_four_page_preview",request_owned("/api/preview",HTTP_METHOD_POST,&json,s_token,&capture)==200);
#if CONFIG_MINIDASH_CYD
    /* Recreate the same old revision after transferring the first buffer. */
    root=bench_configuration_json(candidate,revision); json=cJSON_PrintUnformatted(root); bench_json_delete(root);
#endif
    CHECK("http_revision_conflict",request_owned("/api/preview",HTTP_METHOD_POST,&json,s_token,&capture)==409);
    free(json); json=NULL;
    for (unsigned page=1;page<4;++page) {
        if (!bench_configuration_read(candidate,&revision,NULL)) { ++failed; break; }
        candidate->common.active_page=page;
        root=bench_configuration_json(candidate,revision); json=cJSON_PrintUnformatted(root); bench_json_delete(root);
        CHECK("http_widget_page_preview",json && request_owned("/api/preview",HTTP_METHOD_POST,&json,s_token,&capture)==200);
        free(json); json=NULL; vTaskDelay(pdMS_TO_TICKS(200));
        CHECK("preview_visible_on_display",md_display_app_current_screen()==MD_UI_SCREEN_CUSTOM);
    }
    if (!bench_configuration_read(candidate,&revision,NULL)) { ++failed; goto done; }
    root=bench_configuration_json(candidate,revision); json=cJSON_PrintUnformatted(root); bench_json_delete(root);
    CHECK("http_cancel",json && request_owned("/api/cancel",HTTP_METHOD_POST,&json,s_token,&capture)==200);
    if (bench_configuration_read(candidate,&revision,NULL)) {
        CHECK("cancel_restores_pages",candidate->common.page_count==original->common.page_count);
    } else ++failed;
    free(json); json=NULL;
    if (!bench_configuration_read(candidate,&revision,NULL)) { ++failed; goto done; }
    root=bench_configuration_json(candidate,revision); json=cJSON_PrintUnformatted(root); bench_json_delete(root);
    const uint8_t local_brightness=original->platform.brightness==70 ? 80 : 70;
    CHECK("local_brightness_queue_with_wifi",md_display_app_set_brightness(local_brightness));
    vTaskDelay(pdMS_TO_TICKS(350));
    CHECK("local_brightness_applied_with_wifi",bench_configuration_read(candidate,&revision,NULL) &&
        candidate->platform.brightness==local_brightness && md_ui_web_active());
    CHECK("local_edit_invalidates_web_revision",json && request_owned("/api/preview",HTTP_METHOD_POST,&json,s_token,&capture)==409);
    free(json); json=NULL;
    root=bench_configuration_json(candidate,revision); json=cJSON_PrintUnformatted(root); bench_json_delete(root);
    CHECK("cancel_keeps_local_edit",json && request_owned("/api/cancel",HTTP_METHOD_POST,&json,s_token,&capture)==200 &&
        bench_configuration_read(candidate,&revision,NULL) && candidate->platform.brightness==local_brightness);
    free(json); json=NULL;
    candidate->platform.brightness=50;
    root=bench_configuration_json(candidate,revision); json=cJSON_PrintUnformatted(root); bench_json_delete(root);
    CHECK("http_preview_before_local_takeover",json && request_owned("/api/preview",HTTP_METHOD_POST,&json,s_token,&capture)==200);
    free(json); json=NULL;
    vTaskDelay(pdMS_TO_TICKS(250));
    CHECK("local_takeover_queue",md_display_app_set_brightness(local_brightness));
    vTaskDelay(pdMS_TO_TICKS(350));
    CHECK("local_takeover_discards_preview",bench_configuration_read(candidate,&revision,NULL) &&
        candidate->platform.brightness==local_brightness && candidate->common.page_count==original->common.page_count);
    CHECK("local_settings_actions_with_wifi",
        md_display_app_dispatch(MD_UI_INPUT_CONSOLE,MD_UI_ACTION_OPEN_SETTINGS) &&
        md_display_app_dispatch(MD_UI_INPUT_CONSOLE,MD_UI_ACTION_NEXT_ITEM) &&
        md_display_app_dispatch(MD_UI_INPUT_CONSOLE,MD_UI_ACTION_INCREMENT));
    bool local_theme_applied=false;
    for(unsigned i=0;i<40 && !local_theme_applied;i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
        local_theme_applied=bench_configuration_read(candidate,&revision,NULL) &&
            candidate->common.theme==(original->common.theme+1)%MD_UI_THEME_COUNT;
    }
    CHECK("local_theme_applied_with_wifi",local_theme_applied && md_ui_web_active());
    if (!bench_configuration_read(candidate,&revision,NULL)) { ++failed; goto done; }
    candidate->common.gauge_ranges[MD_SIGNAL_ENGINE_RPM].maximum=0;
    root=bench_configuration_json(original,revision);
    cJSON *range=cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(root,"common"),"gauge_ranges"),0);
    cJSON_ReplaceItemInObjectCaseSensitive(range,"maximum",cJSON_CreateNumber(0));
    json=cJSON_PrintUnformatted(root); bench_json_delete(root);
    CHECK("http_invalid_gauge_range",json && request_owned("/api/preview",HTTP_METHOD_POST,&json,s_token,&capture)==400);
    free(json); json=NULL;
    candidate->common.gauge_auto=false;
    candidate->common.gauge_ranges[MD_SIGNAL_ENGINE_RPM].maximum=10000;
    root=bench_configuration_json(candidate,revision); json=cJSON_PrintUnformatted(root); bench_json_delete(root);
    CHECK("http_save_manual_gauge",json && request_owned("/api/config",HTTP_METHOD_PUT,&json,s_token,&capture)==200);
    free(json); json=NULL;
    md_target_profile_t gauge_target={0};
    CHECK("nvs_manual_gauge_roundtrip",bench_configuration_read(candidate,&revision,&gauge_target) &&
        md_configuration_storage_load(candidate,&gauge_target) && !candidate->common.gauge_auto &&
        candidate->common.gauge_ranges[MD_SIGNAL_ENGINE_RPM].maximum==10000);
    const uint8_t gauge_local_brightness=local_brightness==70 ? 80 : 70;
    const bool gauge_local_queued=md_display_app_set_brightness(gauge_local_brightness);
    vTaskDelay(pdMS_TO_TICKS(400));
    CHECK("local_action_preserves_gauge_ranges",gauge_local_queued &&
        bench_configuration_read(candidate,&revision,NULL) && candidate->platform.brightness==gauge_local_brightness && !candidate->common.gauge_auto &&
        candidate->common.gauge_ranges[MD_SIGNAL_ENGINE_RPM].maximum==10000);
    vTaskDelay(pdMS_TO_TICKS(2000));
    CHECK("nvs_manual_gauge_after_local_edit",md_configuration_storage_load(candidate,&gauge_target) &&
        !candidate->common.gauge_auto && candidate->common.gauge_ranges[MD_SIGNAL_ENGINE_RPM].maximum==10000);
#if CONFIG_MINIDASH_CYD
    for(unsigned theme=0;theme<MD_UI_THEME_COUNT;theme++) {
        if(!bench_configuration_read(candidate,&revision,NULL)){++failed;break;}
        candidate->common.theme=theme;
        root=bench_configuration_json(candidate,revision);json=cJSON_PrintUnformatted(root);bench_json_delete(root);
        printf("MINIDASH_CYD_RENDER theme=%u\n",theme);
        CHECK("cyd_theme_preview",json && request_owned("/api/preview",HTTP_METHOD_POST,&json,s_token,&capture)==200);
        free(json);json=NULL;vTaskDelay(pdMS_TO_TICKS(400));
    }
    for(unsigned layout=MD_UI_LAYOUT_SINGLE;layout<MD_UI_LAYOUT_COUNT;layout++) {
        if(!bench_configuration_read(candidate,&revision,NULL)){++failed;break;}
        candidate->common.theme=MD_UI_THEME_OEM;candidate->common.active_page=0;candidate->common.page_count=1;
        bool layout_ok=md_configuration_set_layout(&candidate->common.pages[0],layout);
        root=bench_configuration_json(candidate,revision);json=cJSON_PrintUnformatted(root);bench_json_delete(root);
        printf("MINIDASH_CYD_RENDER layout=%u\n",layout);
        CHECK("cyd_layout_preview",layout_ok && json && request_owned("/api/preview",HTTP_METHOD_POST,&json,s_token,&capture)==200);
        free(json);json=NULL;vTaskDelay(pdMS_TO_TICKS(400));
    }
    for(unsigned rotation=0;rotation<4;rotation++) {
        if(!bench_configuration_read(candidate,&revision,NULL)){++failed;break;}
        candidate->platform.rotation=rotation;
        root=bench_configuration_json(candidate,revision);json=cJSON_PrintUnformatted(root);bench_json_delete(root);
        printf("MINIDASH_CYD_RENDER rotation=%u\n",rotation);
        CHECK("cyd_rotation_preview",json && request_owned("/api/preview",HTTP_METHOD_POST,&json,s_token,&capture)==200);
        free(json);json=NULL;vTaskDelay(pdMS_TO_TICKS(500));
    }
#endif
    /* Always save the captured original, never the last preview if cancel
       or a config read failed. Tests must not persist synthetic pages. */
    if (!bench_configuration_read(candidate,&revision,NULL)) { ++failed; goto done; }
    root=bench_configuration_json(original,revision); json=cJSON_PrintUnformatted(root); bench_json_delete(root);
    CHECK("http_save_original",json && request_owned("/api/config",HTTP_METHOD_PUT,&json,s_token,&capture)==200);
    md_target_profile_t target;
    CHECK("nvs_roundtrip",bench_configuration_read(candidate,&revision,&target) &&
        md_configuration_storage_load(candidate,&target) &&
        original->common.page_count==candidate->common.page_count && original->common.language==candidate->common.language);
    CHECK("touch_calibration_preserved",!touch_loaded || (md_touch_map_storage_load(&touch_after,320,240,CONFIG_MINIDASH_DISPLAY_ROTATION) &&
        !memcmp(&touch_before,&touch_after,sizeof(touch_before))));
done:
    free(json); free(original); free(candidate); free(capture.body);
#if !CONFIG_MINIDASH_CYD
    md_ui_web_stop();
#endif
#if !CONFIG_MINIDASH_CYD
    for (unsigned i=0;i<50 && md_ui_web_active();++i) vTaskDelay(pdMS_TO_TICKS(100));
#endif
    ESP_LOGI(TAG,"AP cycle internal_before=%u after=%u minimum=%u",(unsigned)heap_before,
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
        (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
#if CONFIG_MINIDASH_CYD
    printf("MINIDASH_CYD_STACK http_min=%u selftest_min=%u\n",s_http_stack_min,(unsigned)uxTaskGetStackHighWaterMark(NULL));
#endif
    printf("MINIDASH_WEB_TEST_SUMMARY pass=%u fail=%u\n",passed,failed); fflush(stdout);
    return failed==0;
#undef CHECK
}

static atomic_bool s_testing;
static void web_test_task(void *argument)
{
    (void)argument; web_selftest_run(); atomic_store(&s_testing,false); vTaskDelete(NULL);
}
bool md_ui_web_selftest(void)
{
    bool expected=false;
    if (!atomic_compare_exchange_strong(&s_testing,&expected,true)) return false;
    if (xTaskCreate(web_test_task,"ui_web_test",
#if CONFIG_MINIDASH_CYD
        8192,
#else
        12288,
#endif
        NULL,2,NULL)!=pdPASS) {
        atomic_store(&s_testing,false); return false;
    }
    return true;
}
