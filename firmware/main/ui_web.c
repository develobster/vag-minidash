#include "ui_web.h"
#include "display_app.h"
#include "ui_configuration_nvs.h"
#include "ui_touch_nvs.h"
#include "sdkconfig.h"
#include "esp_http_server.h"
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
#include <stdatomic.h>

#define BODY_MAX 16384
static httpd_handle_t s_server;
static esp_netif_t *s_netif;
static atomic_bool s_active, s_starting, s_stop;
static char s_ssid[32], s_password[24], s_token[33];
extern const unsigned char webui_start[] asm("_binary_webui_html_start");
extern const unsigned char webui_end[] asm("_binary_webui_html_end");
static const char *TAG = "md_web";

bool md_ui_web_active(void) { return atomic_load(&s_active) || atomic_load(&s_starting); }
const char *md_ui_web_ssid(void) { return atomic_load(&s_active) ? s_ssid : "Starting..."; }
const char *md_ui_web_password(void) { return atomic_load(&s_active) ? s_password : ""; }
void md_ui_web_stop(void) { atomic_store(&s_stop, true); }

static esp_err_t respond(httpd_req_t *req, cJSON *object)
{
    char *json = object ? cJSON_PrintUnformatted(object) : NULL;
    cJSON_Delete(object);
    if (!json) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "allocation failed");
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");
    esp_err_t result = httpd_resp_send(req,json,HTTPD_RESP_USE_STRLEN); free(json); return result;
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
    } else if (!strcmp(req->uri,"/api/signals")) {
        root = cJSON_CreateArray();
        for (unsigned i=0;i<MD_SIGNAL_COUNT;++i) {
            cJSON *o=cJSON_CreateObject(); const md_ui_signal_definition_t *d=&md_ui_signal_definitions[i];
            cJSON_AddStringToObject(o,"id",d->canonical_name); cJSON_AddStringToObject(o,"label",d->label);
            cJSON_AddStringToObject(o,"unit",d->canonical_unit); cJSON_AddNumberToObject(o,"precision",d->decimals);
            cJSON_AddItemToArray(root,o);
        }
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
    int status=md_display_configuration_apply(config,revision,!strcmp(req->uri,"/api/config"),!strcmp(req->uri,"/api/cancel"));
    free(config);
    if (status!=200) {
        httpd_resp_set_status(req,status==409 ? "409 Conflict" : status==403 ? "403 Forbidden" : status==400 ? "400 Bad Request" : "500 Internal Server Error");
        return httpd_resp_send(req,"configuration rejected",HTTPD_RESP_USE_STRLEN);
    }
    md_device_configuration_t *current=malloc(sizeof(*current));
    if (!current || !md_display_configuration_read(current,&revision,NULL)) { free(current); return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"read failed"); }
    cJSON *result=md_configuration_json(current,revision); free(current); return respond(req,result);
}
static void web_task(void *argument)
{
    (void)argument;
    bool wifi_initialized=false, wifi_started=false;
    ESP_LOGI(TAG,"AP start internal_free=%u largest=%u",(unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),(unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
    esp_err_t error=esp_netif_init();
    if (error!=ESP_OK && error!=ESP_ERR_INVALID_STATE) goto cleanup;
    error=esp_event_loop_create_default();
    if (error!=ESP_OK && error!=ESP_ERR_INVALID_STATE) goto cleanup;
    s_netif=esp_netif_create_default_wifi_ap(); if (!s_netif) goto cleanup;
    wifi_init_config_t init=WIFI_INIT_CONFIG_DEFAULT();
    error=esp_wifi_init(&init); if (error!=ESP_OK) { ESP_LOGE(TAG,"Wi-Fi init: %s",esp_err_to_name(error)); goto cleanup; }
    wifi_initialized=true;
    uint8_t mac[6]; esp_wifi_get_mac(WIFI_IF_AP,mac);
    snprintf(s_ssid,sizeof(s_ssid),"MiniDash-%02X%02X%02X",mac[3],mac[4],mac[5]);
    snprintf(s_password,sizeof(s_password),"%08lx%08lx",(unsigned long)esp_random(),(unsigned long)esp_random());
    for (unsigned i=0;i<4;++i) snprintf(s_token+i*8,9,"%08lx",(unsigned long)esp_random());
    wifi_config_t wifi={0}; snprintf((char *)wifi.ap.ssid,sizeof(wifi.ap.ssid),"%s",s_ssid);
    snprintf((char *)wifi.ap.password,sizeof(wifi.ap.password),"%s",s_password);
    wifi.ap.ssid_len=strlen(s_ssid); wifi.ap.channel=1; wifi.ap.max_connection=2;
    wifi.ap.authmode=WIFI_AUTH_WPA2_PSK; wifi.ap.pmf_cfg.capable=true;
    if (esp_wifi_set_storage(WIFI_STORAGE_RAM)!=ESP_OK || esp_wifi_set_mode(WIFI_MODE_AP)!=ESP_OK ||
        esp_wifi_set_config(WIFI_IF_AP,&wifi)!=ESP_OK || esp_wifi_start()!=ESP_OK) goto cleanup;
    wifi_started=true;
    httpd_config_t server=HTTPD_DEFAULT_CONFIG(); server.task_priority=2;
    server.stack_size=12288; server.max_open_sockets=3; server.max_uri_handlers=11; server.lru_purge_enable=true;
    server.recv_wait_timeout=3; server.send_wait_timeout=3;
    if (httpd_start(&s_server,&server)!=ESP_OK) goto cleanup;
    static const char *const gets[]={"/","/api/session","/api/config","/api/signals","/api/device","/api/layouts","/api/themes","/api/defaults"};
    for (unsigned i=0;i<8;++i) {
        httpd_uri_t uri={.uri=gets[i],.method=HTTP_GET,.handler=get_handler};
        if (httpd_register_uri_handler(s_server,&uri)!=ESP_OK) goto cleanup;
    }
    static const char *const writes[]={"/api/config","/api/preview","/api/cancel"};
    for (unsigned i=0;i<3;++i) {
        httpd_uri_t uri={.uri=writes[i],.method=i==0 ? HTTP_PUT : HTTP_POST,.handler=mutation_handler};
        if (httpd_register_uri_handler(s_server,&uri)!=ESP_OK) goto cleanup;
    }
    atomic_store(&s_active,true); atomic_store(&s_starting,false);
    ESP_LOGI(TAG,"Configuration AP ready (credentials on device), expires in 600 seconds");
    const int64_t started=esp_timer_get_time();
    while (!atomic_load(&s_stop) && esp_timer_get_time()-started<600000000LL) vTaskDelay(pdMS_TO_TICKS(200));
cleanup:
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

typedef struct { char *body; size_t length; bool overflow; } http_capture_t;
static esp_err_t capture_event(esp_http_client_event_t *event)
{
    http_capture_t *capture=event->user_data;
    if (event->event_id==HTTP_EVENT_ON_DATA) {
        if (capture->length+event->data_len>=BODY_MAX) { capture->overflow=true; return ESP_FAIL; }
        memcpy(capture->body+capture->length,event->data,event->data_len);
        capture->length+=event->data_len; capture->body[capture->length]=0;
    }
    return ESP_OK;
}
static int request(const char *path, esp_http_client_method_t method, const char *body,
                   const char *token, http_capture_t *capture)
{
    char url[96]; snprintf(url,sizeof(url),"http://192.168.4.1%s",path);
    esp_http_client_config_t config={.url=url,.timeout_ms=5000,.event_handler=capture_event,.user_data=capture};
    esp_http_client_handle_t client=esp_http_client_init(&config); if (!client) return -1;
    capture->length=0; capture->overflow=false; capture->body[0]=0;
    esp_http_client_set_method(client,method);
    if (body) {
        esp_http_client_set_header(client,"Content-Type","application/json");
        if (token) esp_http_client_set_header(client,"X-MiniDash-Session",token);
        esp_http_client_set_post_field(client,body,strlen(body));
    }
    esp_err_t error=esp_http_client_perform(client);
    int status=error==ESP_OK && !capture->overflow ? esp_http_client_get_status_code(client) : -1;
    esp_http_client_cleanup(client); return status;
}
static bool web_selftest_run(void)
{
    if (md_ui_web_active()) { printf("MINIDASH_WEB_TEST_REJECTED already_active\n"); return false; }
    unsigned passed=0,failed=0;
    md_touch_map_t touch_before={0},touch_after={0};
    bool touch_loaded=md_touch_map_storage_load(&touch_before,320,240,CONFIG_MINIDASH_DISPLAY_ROTATION);
    const size_t heap_before=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    if (!md_ui_web_start()) return false;
    for (unsigned i=0;i<50 && !atomic_load(&s_active);++i) vTaskDelay(pdMS_TO_TICKS(100));
    http_capture_t capture={.body=malloc(BODY_MAX)};
    md_device_configuration_t *original=malloc(sizeof(*original)), *candidate=malloc(sizeof(*candidate));
    uint32_t revision=0; char *json=NULL;
#define CHECK(NAME,EXPR) do { bool ok=(EXPR); printf("MINIDASH_WEB_TEST name=%s status=%s\n",NAME,ok?"PASS":"FAIL"); if (ok) ++passed; else ++failed; } while (0)
    if (!capture.body || !original || !candidate || !atomic_load(&s_active)) { ++failed; goto done; }
    CHECK("http_config_get",request("/api/config",HTTP_METHOD_GET,NULL,NULL,&capture)==200);
    cJSON *root=cJSON_Parse(capture.body);
    bool valid=md_configuration_parse_json(root,original,&revision); cJSON_Delete(root);
    CHECK("http_json_roundtrip",valid); if (!valid) goto done;
    root=md_configuration_json(original,revision); char *backup=cJSON_PrintUnformatted(root); cJSON_Delete(root);
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
    root=md_configuration_json(candidate,revision); json=cJSON_PrintUnformatted(root); cJSON_Delete(root);
    if (!json) { ++failed; goto done; }
    CHECK("http_session_gate",request("/api/preview",HTTP_METHOD_POST,json,"wrong",&capture)==403);
    CHECK("http_four_page_preview",request("/api/preview",HTTP_METHOD_POST,json,s_token,&capture)==200);
    CHECK("http_revision_conflict",request("/api/preview",HTTP_METHOD_POST,json,s_token,&capture)==409);
    free(json); json=NULL;
    for (unsigned page=1;page<4;++page) {
        if (!md_display_configuration_read(candidate,&revision,NULL)) { ++failed; break; }
        candidate->common.active_page=page;
        root=md_configuration_json(candidate,revision); json=cJSON_PrintUnformatted(root); cJSON_Delete(root);
        CHECK("http_widget_page_preview",json && request("/api/preview",HTTP_METHOD_POST,json,s_token,&capture)==200);
        free(json); json=NULL; vTaskDelay(pdMS_TO_TICKS(200));
        CHECK("preview_visible_on_display",md_display_app_current_screen()==MD_UI_SCREEN_CUSTOM);
    }
    if (!md_display_configuration_read(candidate,&revision,NULL)) { ++failed; goto done; }
    root=md_configuration_json(candidate,revision); json=cJSON_PrintUnformatted(root); cJSON_Delete(root);
    CHECK("http_cancel",json && request("/api/cancel",HTTP_METHOD_POST,json,s_token,&capture)==200);
    if (md_display_configuration_read(candidate,&revision,NULL)) {
        CHECK("cancel_restores_pages",candidate->common.page_count==original->common.page_count);
    } else ++failed;
    free(json); json=NULL;
    /* Always save the captured original, never the last preview if cancel
       or a config read failed. Tests must not persist synthetic pages. */
    if (!md_display_configuration_read(candidate,&revision,NULL)) { ++failed; goto done; }
    root=md_configuration_json(original,revision); json=cJSON_PrintUnformatted(root); cJSON_Delete(root);
    CHECK("http_save_original",json && request("/api/config",HTTP_METHOD_PUT,json,s_token,&capture)==200);
    md_target_profile_t target;
    if (md_display_configuration_read(candidate,&revision,&target)) {
        CHECK("nvs_roundtrip",md_configuration_storage_load(candidate,&target) &&
            original->common.page_count==candidate->common.page_count && original->common.language==candidate->common.language);
    } else ++failed;
    CHECK("touch_calibration_preserved",!touch_loaded || (md_touch_map_storage_load(&touch_after,320,240,CONFIG_MINIDASH_DISPLAY_ROTATION) &&
        !memcmp(&touch_before,&touch_after,sizeof(touch_before))));
done:
    free(json); free(original); free(candidate); free(capture.body);
    md_ui_web_stop();
    for (unsigned i=0;i<50 && md_ui_web_active();++i) vTaskDelay(pdMS_TO_TICKS(100));
    ESP_LOGI(TAG,"AP cycle internal_before=%u after=%u minimum=%u",(unsigned)heap_before,
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
        (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
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
    if (xTaskCreate(web_test_task,"ui_web_test",12288,NULL,2,NULL)!=pdPASS) {
        atomic_store(&s_testing,false); return false;
    }
    return true;
}
