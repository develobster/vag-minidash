#include "ui_web.h"
#include "ui_platform_selftest.h"
#include <stdio.h>
#include <string.h>
static bool analytics_json_contract(void) {
    static md_device_configuration_t a,b;
    md_ui_settings_t s;md_ui_settings_defaults(&s);
    if(!md_configuration_from_legacy(&a,&s,NULL))return false;
    for(unsigned count=12;count<=14;count+=2) {
        cJSON *root=md_configuration_json(&a,7),*common=cJSON_GetObjectItem(root,"common");
        cJSON_ReplaceItemInObject(root,"schema",cJSON_CreateNumber(3));
        cJSON_DeleteItemFromObject(common,"analytics");
        cJSON *ranges=cJSON_GetObjectItem(common,"gauge_ranges");
        while(cJSON_GetArraySize(ranges)>(int)count)cJSON_DeleteItemFromArray(ranges,count);
        uint32_t revision=0;bool ok=md_configuration_parse_json(root,&b,&revision);
        cJSON_Delete(root);
        if(!ok || revision!=7 || b.schema_version!=4 || b.common.analytics.rules[0].enabled)return false;
    }
    a.common.analytics.rules[6].enabled=true;a.common.analytics.rules[6].threshold=-3.5;
    cJSON *root=md_configuration_json(&a,8);uint32_t revision;
    bool ok=md_configuration_parse_json(root,&b,&revision);
    if(!ok || !b.common.analytics.rules[6].enabled || b.common.analytics.rules[6].threshold!=-3.5){cJSON_Delete(root);return false;}
    cJSON *analytics=cJSON_GetObjectItem(cJSON_GetObjectItem(root,"common"),"analytics");
    cJSON_AddNumberToObject(analytics,"counter_bits",16);
    ok=md_configuration_parse_json(root,&b,&revision);cJSON_Delete(root);
    if(ok)return false;
    for(unsigned p=0;p<4;p++) {a.common.pages[p]=a.common.pages[0];a.common.page_count=p+1;if(!md_configuration_analytics_preset(&a,p,p))return false;}
    root=md_configuration_json(&a,9);char *text=cJSON_PrintUnformatted(root),*pretty=cJSON_Print(root);
    ok=text && pretty && strlen(text)<16384 && strlen(pretty)<32768 && md_configuration_parse_json(root,&b,&revision);
    cJSON_free(text);cJSON_free(pretty);cJSON_Delete(root);
    return ok;
}
int main(int argc,char **argv) {
    if (argc<2) { bool ok=md_ui_test_configuration_json() && analytics_json_contract(); printf("MINIDASH_JSON_TEST status=%s\n",ok?"PASS":"FAIL"); return ok?0:1; }
    md_ui_settings_t settings; md_ui_settings_defaults(&settings);
    md_target_profile_t target={.hardware=MD_CAP_COLOR|MD_CAP_WIFI};
    md_device_configuration_t config; md_configuration_from_legacy(&config,&settings,&target);
    cJSON *root=cJSON_CreateObject();
    cJSON_AddItemToObject(root,"config",md_configuration_json(&config,1));
    cJSON_AddItemToObject(root,"defaults",md_configuration_json(&config,1));
    cJSON *signals=cJSON_AddArrayToObject(root,"signals");
    for (unsigned i=0;i<MD_SIGNAL_COUNT;++i) { cJSON *o=cJSON_CreateObject(); cJSON_AddStringToObject(o,"id",md_ui_signal_definitions[i].canonical_name); cJSON_AddStringToObject(o,"label",md_ui_signal_definitions[i].label); cJSON_AddStringToObject(o,"unit",md_ui_signal_definitions[i].canonical_unit); cJSON_AddNumberToObject(o,"precision",md_ui_signal_definitions[i].decimals); cJSON_AddItemToArray(signals,o); }
    cJSON_AddItemToObject(root,"presets",md_analytics_presets_json());
    cJSON *themes=cJSON_AddArrayToObject(root,"themes");
    for (unsigned i=0;i<MD_UI_THEME_COUNT;++i) { config.common.theme=i; config.common.palette=md_ui_theme_palette(i,false); cJSON *preset=md_configuration_json(&config,1); md_ui_theme_json_metadata(preset,i); cJSON_AddItemToArray(themes,preset); }
    cJSON *layouts=cJSON_AddArrayToObject(root,"layouts");
    for (unsigned i=1;i<MD_UI_LAYOUT_COUNT;++i) { md_ui_dashboard_page_t p={0}; md_configuration_set_layout(&p,i); cJSON *o=cJSON_CreateObject(); cJSON_AddNumberToObject(o,"id",i); cJSON *slots=cJSON_AddArrayToObject(o,"slots"); for (unsigned j=0;j<p.widget_count;++j) { md_ui_page_widget_t *w=&p.widgets[j]; cJSON *v=cJSON_CreateObject(); cJSON_AddNumberToObject(v,"column",w->column); cJSON_AddNumberToObject(v,"row",w->row); cJSON_AddNumberToObject(v,"columns",w->columns); cJSON_AddNumberToObject(v,"rows",w->rows); cJSON_AddItemToArray(slots,v); } cJSON_AddItemToArray(layouts,o); }
    cJSON *device=cJSON_AddObjectToObject(root,"device"); cJSON_AddStringToObject(device,"target","browser-fixture"); cJSON_AddNumberToObject(device,"width",320); cJSON_AddNumberToObject(device,"height",240); cJSON_AddNumberToObject(device,"maximum_pages",4); cJSON_AddBoolToObject(device,"dimming",false);
    char *json=cJSON_PrintUnformatted(root); if (json) { puts(json); cJSON_free(json); } cJSON_Delete(root); return 0;
}
