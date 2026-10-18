#include "display_app.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <string.h>

#include "display_manager.h"
#include "backlight_controller.h"
#include "display_serial_protocol.h"
#include "ui_instrument_profiles.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "ili9341_backend.h"
#include "sdkconfig.h"
#include "st7789_backend.h"
#include "ui_screens.h"
#include "ui_settings_nvs.h"
#include "nvs.h"
#include "ui_configuration_nvs.h"
#include "ui_web.h"
#include "freertos/semphr.h"
#include "ui_touch_map.h"
#include "ui_touch_nvs.h"
#include "ui_theme.h"
#include "ui_startup.h"
#include "xpt2046_touch.h"
#include "vehicle_app.h"
#include "derived_engine.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define UI_UPDATE_PERIOD_MS 100U
#define UI_SETTINGS_DEBOUNCE_MS 1500U
#define UI_EVENT_QUEUE_LENGTH 16U
#define UI_TOUCH_SAMPLE_PERIOD_MS 25U

/* Headless builds retain the public display API, but never start these tasks. */
#if CONFIG_MINIDASH_DISPLAY_ENABLE
#define MD_DISPLAY_TASK_UNUSED
#else
#define MD_DISPLAY_TASK_UNUSED __attribute__((unused))
#endif

typedef enum {
    UI_EVENT_SET_BRIGHTNESS = 7,
    UI_EVENT_INSTRUMENT_PROFILE = 8,
    UI_EVENT_CONFIG_MODE = 0,
    UI_EVENT_ACTION = 1,
    UI_EVENT_TOUCH_GESTURE,
    UI_EVENT_SET_WARNING,
    UI_EVENT_TOUCH_PRESS,
    UI_EVENT_TOUCH_RELEASE,
    UI_EVENT_VEHICLE_CONTEXT,
} ui_event_type_t;

typedef struct {
    ui_event_type_t type;
    md_ui_input_source_t source;
    md_ui_action_t action;
    md_ui_touch_gesture_t touch_gesture;
    uint16_t raw_x;
    uint16_t raw_y;
    md_ui_signal_id_t signal_id;
    md_ui_context_t vehicle_context;
    bool context_verified;
    char canonical_signal[MD_UI_SIGNAL_NAME_LEN];
    uint8_t brightness_pct;
    uint32_t profile_ttl_ms;
    bool warning_enabled;
    md_ui_threshold_direction_t threshold_direction;
    int32_t warning_milli;
    int32_t critical_milli;
} ui_event_t;

static const char *TAG = "md_ui";
static QueueHandle_t s_ui_event_queue;
static QueueHandle_t s_settings_save_queue;
static TaskHandle_t s_ui_task;
static atomic_int s_visible_screen;
static SemaphoreHandle_t s_config_mutex;
static md_device_configuration_t s_configuration, s_saved_configuration;
static md_target_profile_t s_target;
static uint32_t s_config_revision;
static bool s_config_ready, s_config_driving, s_config_preview, s_preview_ended_locally;
static lv_display_t *s_lv_display;
static uint8_t s_applied_rotation;
static uint16_t s_ui_width, s_ui_height;

bool md_display_configuration_read(md_device_configuration_t *config, uint32_t *revision,
                                   md_target_profile_t *target)
{
    if (!config || !revision || !s_config_mutex || xSemaphoreTake(s_config_mutex,pdMS_TO_TICKS(200))!=pdTRUE) return false;
    bool ready=s_config_ready;
    if (ready) { *config=s_configuration; *revision=s_config_revision; if (target) *target=s_target; }
    xSemaphoreGive(s_config_mutex); return ready;
}
bool md_display_configuration_begin(void)
{
    if (!s_config_mutex || xSemaphoreTake(s_config_mutex,pdMS_TO_TICKS(500))!=pdTRUE) return false;
    bool ok=s_config_ready && !s_config_driving && !s_config_preview;
    if (ok && memcmp(&s_configuration,&s_saved_configuration,sizeof(s_configuration))) {
        ok=md_configuration_storage_save(&s_configuration);
        if (ok) s_saved_configuration=s_configuration;
    }
    xSemaphoreGive(s_config_mutex); return ok;
}
int md_display_configuration_apply(const md_device_configuration_t *config, uint32_t revision, bool save, bool cancel)
{
    if (!config || !s_config_mutex || xSemaphoreTake(s_config_mutex,pdMS_TO_TICKS(500))!=pdTRUE) return 500;
    int result=200;
    if (!s_config_ready || revision!=s_config_revision) result=409;
    else if (!cancel && (!md_ui_web_active() || s_config_driving)) result=403;
    else if (!cancel && (!md_configuration_validate(config,&s_target) ||
        config->platform.present!=s_configuration.platform.present)) result=400;
    else if (cancel && !s_config_preview) { /* Keep pending local edits when AP stops. */ }
    else if (!save && !cancel && !s_config_preview &&
             memcmp(&s_configuration,&s_saved_configuration,sizeof(s_configuration)) &&
             !md_configuration_storage_save(&s_configuration)) result=500;
    else if (save && !md_configuration_storage_save(config)) result=500;
    else {
        /* Checkpoint the local baseline before the first temporary Web preview. */
        if (!cancel && !save && !s_config_preview) s_saved_configuration=s_configuration;
        if (cancel) { s_configuration=s_saved_configuration; s_config_preview=false; }
        else { s_configuration=*config; s_config_preview=!save; if (save) s_saved_configuration=*config; }
        ++s_config_revision;
    }
    xSemaphoreGive(s_config_mutex); return result;
}
static md_ui_signal_store_t s_signal_store;
static portMUX_TYPE s_signal_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool s_display_ready;
static volatile bool s_touch_ready;
static volatile bool s_touch_calibrated;
static volatile bool s_touch_calibrating;
static md_touch_map_t s_touch_map;
static md_touch_cal_point_t s_calibration_points[MD_TOUCH_CAL_POINTS];
static uint8_t s_calibration_step;
static int16_t s_last_press_x = -1;
static int16_t s_last_press_y = -1;

static portMUX_TYPE s_perf_mux = portMUX_INITIALIZER_UNLOCKED;
static int64_t s_render_started_us;
static int64_t s_render_window_started_us;
static uint64_t s_render_window_total_us;
static uint32_t s_render_window_count;
static uint32_t s_render_rate;
static uint32_t s_average_render_ms;
static bool s_render_seen;

static md_display_backend_t *configured_backend(void)
{
#if CONFIG_MINIDASH_LCD_CONTROLLER_ILI9341
    return md_ili9341_backend();
#else
    return md_st7789_backend();
#endif
}

static md_driver_t configured_driver(void)
{
#if CONFIG_MINIDASH_LCD_CONTROLLER_ILI9341
    return MD_DRIVER_ILI9341;
#else
    return MD_DRIVER_ST7789;
#endif
}

static const char *configured_driver_name(void)
{
#if CONFIG_MINIDASH_LCD_CONTROLLER_ILI9341
    return "ILI9341";
#else
    return "ST7789";
#endif
}

static void render_event_cb(lv_event_t *event)
{
    const lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_RENDER_START) {
        s_render_started_us = esp_timer_get_time();
        return;
    }
    if (code != LV_EVENT_RENDER_READY) return;

    const int64_t now_us = esp_timer_get_time();
    const uint64_t elapsed_us = s_render_started_us > 0 && now_us >= s_render_started_us
        ? (uint64_t)(now_us - s_render_started_us) : 0U;
    portENTER_CRITICAL(&s_perf_mux);
    ++s_render_window_count;
    s_render_window_total_us += elapsed_us;
    s_render_seen = true;
    portEXIT_CRITICAL(&s_perf_mux);
}

static void render_metrics_read(uint32_t *renders_per_second,
                                uint32_t *average_render_ms,
                                bool *available)
{
    const int64_t now_us = esp_timer_get_time();
    portENTER_CRITICAL(&s_perf_mux);
    if (s_render_window_started_us == 0) s_render_window_started_us = now_us;
    const int64_t elapsed_us = now_us - s_render_window_started_us;
    if (elapsed_us >= 1000000) {
        if (s_render_window_count > 0) {
            s_render_rate = (uint32_t)(((uint64_t)s_render_window_count * 1000000U) /
                                       (uint64_t)elapsed_us);
            s_average_render_ms = (uint32_t)(s_render_window_total_us /
                s_render_window_count / 1000U);
        } else {
            s_render_rate = 0;
            s_average_render_ms = 0;
        }
        s_render_window_count = 0;
        s_render_window_total_us = 0;
        s_render_window_started_us = now_us;
    }
    *renders_per_second = s_render_rate;
    *average_render_ms = s_average_render_ms;
    *available = s_render_seen;
    portEXIT_CRITICAL(&s_perf_mux);
}

static void MD_DISPLAY_TASK_UNUSED settings_storage_task(void *argument)
{
    (void)argument;
    uint32_t pending;
    for (;;) {
        if (xQueueReceive(s_settings_save_queue,&pending,portMAX_DELAY)!=pdTRUE) continue;
        TickType_t quiet_since=xTaskGetTickCount();
        const TickType_t quiet_period=pdMS_TO_TICKS(UI_SETTINGS_DEBOUNCE_MS);
        for (;;) {
            TickType_t elapsed=xTaskGetTickCount()-quiet_since;
            if (elapsed>=quiet_period) break;
            if (xQueueReceive(s_settings_save_queue,&pending,quiet_period-elapsed)==pdTRUE) quiet_since=xTaskGetTickCount();
        }
        if (xSemaphoreTake(s_config_mutex,portMAX_DELAY)==pdTRUE) {
            if (pending==s_config_revision && !s_config_preview) {
                if (md_configuration_storage_save(&s_configuration)) s_saved_configuration=s_configuration;
                else ESP_LOGW(TAG,"Configuration not saved; retry by changing a setting");
            }
            xSemaphoreGive(s_config_mutex);
        }
    }
}

#if CONFIG_MINIDASH_TOUCH_ENABLE
static void enqueue_touch_gesture(md_ui_touch_gesture_t gesture)
{
    if (!s_ui_event_queue) return;
    const ui_event_t event = {
        .type = UI_EVENT_TOUCH_GESTURE,
        .source = MD_UI_INPUT_TOUCH,
        .touch_gesture = gesture,
    };
    (void)xQueueSend(s_ui_event_queue, &event, 0);
}

static void enqueue_touch_raw(ui_event_type_t type, uint16_t x, uint16_t y)
{
    if (!s_ui_event_queue) return;
    const ui_event_t event = {.type = type, .source = MD_UI_INPUT_TOUCH,
                              .raw_x = x, .raw_y = y};
    (void)xQueueSend(s_ui_event_queue, &event, 0);
}

static void touch_input_task(void *argument)
{
    (void)argument;
    md_xpt2046_t touch = {0};
    if (!md_xpt2046_open(&touch)) {
        ESP_LOGW(TAG, "XPT2046 input is unavailable");
        vTaskDelete(NULL);
        return;
    }
    s_touch_ready = true;
    ESP_LOGI(TAG, "Touch input ready; %s", s_touch_calibrated
             ? "calibrated edge controls active" : "gesture fallback active");

    bool pressed = false;
    bool long_press_sent = false;
    bool pending_tap = false;
    uint8_t pressed_samples = 0;
    uint8_t released_samples = 0;
    uint32_t pressed_at = 0;
    uint32_t press_candidate_at = 0;
    uint32_t pending_tap_at = 0;
    uint32_t raw_x_sum = 0, raw_y_sum = 0;
    uint16_t raw_count = 0;
    for (;;) {
        md_touch_sample_t sample = {0};
        if (md_xpt2046_read(&touch, &sample)) {
            const uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
            if (sample.pressed) {
                if (raw_count < 100) {
                    raw_x_sum += sample.x;
                    raw_y_sum += sample.y;
                    ++raw_count;
                }
                if (pressed_samples == 0 && !pressed) press_candidate_at = now_ms;
                pressed_samples = pressed_samples < 2 ? pressed_samples + 1U : 2U;
                released_samples = 0;
                if (!pressed && pressed_samples >= 2U) {
                    pressed = true;
                    pressed_at = press_candidate_at;
                    long_press_sent = false;
                    if (s_touch_calibrated && !s_touch_calibrating)
                        enqueue_touch_raw(UI_EVENT_TOUCH_PRESS, sample.x, sample.y);
                }
                if (pressed && !long_press_sent &&
                    (uint32_t)(now_ms - pressed_at) >= 700U) {
                    enqueue_touch_gesture(MD_UI_TOUCH_LONG_PRESS);
                    long_press_sent = true;
                }
            } else {
                pressed_samples = 0;
                if (!pressed) {
                    raw_x_sum = raw_y_sum = 0;
                    raw_count = 0;
                }
                if (pressed) {
                    released_samples = released_samples < 2 ? released_samples + 1U : 2U;
                    if (released_samples >= 2U) {
                        pressed = false;
                        released_samples = 0;
                        if (raw_count && !long_press_sent &&
                            (s_touch_calibrated || s_touch_calibrating)) {
                            enqueue_touch_raw(UI_EVENT_TOUCH_RELEASE,
                                (uint16_t)(raw_x_sum / raw_count),
                                (uint16_t)(raw_y_sum / raw_count));
                        } else if (!long_press_sent) {
                            if (pending_tap &&
                                (uint32_t)(press_candidate_at - pending_tap_at) <= 350U) {
                                enqueue_touch_gesture(MD_UI_TOUCH_DOUBLE_TAP);
                                pending_tap = false;
                            } else {
                                if (pending_tap) {
                                    enqueue_touch_gesture(MD_UI_TOUCH_TAP);
                                }
                                pending_tap = true;
                                pending_tap_at = now_ms;
                            }
                        }
                        raw_x_sum = raw_y_sum = 0;
                        raw_count = 0;
                    }
                }
            }
            if (pending_tap && !pressed && pressed_samples == 0 &&
                !s_touch_calibrated && !s_touch_calibrating &&
                (uint32_t)(now_ms - pending_tap_at) > 350U) {
                enqueue_touch_gesture(MD_UI_TOUCH_TAP);
                pending_tap = false;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(UI_TOUCH_SAMPLE_PERIOD_MS));
    }
}
#endif

static bool open_display(md_display_manager_t *manager,
                         md_display_profile_t *profile)
{
    if (!md_display_profile_select(configured_driver(), CONFIG_MINIDASH_DISPLAY_ROTATION,
                                   profile)) return false;

#if CONFIG_MINIDASH_DISPLAY_SMOKE_TEST
    uint16_t *frame = heap_caps_malloc((size_t)profile->width * profile->height * sizeof(uint16_t),
                                       MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    if (!frame) return false;
#else
    uint16_t *frame = NULL;
#endif

    const bool opened = md_display_open(manager, configured_backend(), profile, frame);
    if (!opened) {
#if CONFIG_MINIDASH_DISPLAY_SMOKE_TEST
        heap_caps_free(frame);
#endif
        return false;
    }

    ESP_LOGI(TAG, "Display initialized with %s backend at %ux%u rotation=%u",
             configured_driver_name(), profile->width, profile->height, profile->rotation);

#if CONFIG_MINIDASH_DISPLAY_SMOKE_TEST
    const struct {
        const char *name;
        uint16_t color;
        uint32_t hold_ms;
    } patterns[] = {{"RED", 0xf800, 700}, {"GREEN", 0x07e0, 700},
                    {"BLUE", 0x001f, 700}, {"BLACK", 0x0000, 500}};
    bool passed = true;
    for (size_t i = 0; i < sizeof(patterns) / sizeof(patterns[0]); ++i) {
        passed &= md_display_present_solid(manager, patterns[i].color);
        ESP_LOGI(TAG, "Display pattern %s: %s", patterns[i].name,
                 passed ? "PASS" : "FAIL");
        if (!passed) break;
        vTaskDelay(pdMS_TO_TICKS(patterns[i].hold_ms));
    }
    if (passed) passed = md_display_render_smoke_ok(manager);
    ESP_LOGI(TAG, "Display smoke test: %s", passed ? "PASS" : "FAIL");
    heap_caps_free(frame);
    return passed;
#else
    return true;
#endif
}

static bool effective_day_mode(const md_ui_settings_t *settings)
{
    /* AUTO remains night-safe until an ambient-light/vehicle context provider exists. */
    return settings->day_night == MD_UI_DAY_NIGHT_DAY;
}

static md_ui_diagnostics_t diagnostics_snapshot(uint32_t now_ms,
                                                const md_ui_signal_store_t *signals)
{
    md_ui_diagnostics_t diagnostics = {0};
    const esp_app_desc_t *app = esp_app_get_description();
    if (app && app->version[0]) {
        (void)snprintf(diagnostics.firmware_version,
                       sizeof(diagnostics.firmware_version), "%s", app->version);
    }
    diagnostics.uptime_seconds = (uint32_t)(esp_timer_get_time() / 1000000);
    diagnostics.free_heap_bytes = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    diagnostics.minimum_free_heap_bytes = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
    diagnostics.display_ready = s_display_ready;
    diagnostics.touch_ready = s_touch_ready;
    diagnostics.brightness_control = CONFIG_MINIDASH_LCD_PIN_BL >= 0;
    /* Only ui_task calls this helper; keep the analytics-sized copy off its stack. */
    static md_vehicle_status_t vehicle;
    if (md_vehicle_app_status(&vehicle)) {
        diagnostics.transport_available=true;
        snprintf(diagnostics.transport_state,sizeof(diagnostics.transport_state),"%s",md_acq_state_name(vehicle.state));
        diagnostics.ecu_count_available=vehicle.configured;
        diagnostics.ecu_count=vehicle.ecu_count;diagnostics.selected_ecu=vehicle.ecu_id;
        diagnostics.can_rx=vehicle.rx;diagnostics.can_tx=vehicle.tx;
        diagnostics.can_tx_failed=vehicle.tx_failed;
        diagnostics.can_timeouts=vehicle.timeouts;diagnostics.can_dropped=vehicle.rx_dropped;
        if(vehicle.dtc_valid && vehicle.dtc_count) {
            char code[6];md_acq_format_dtc(vehicle.dtc[(now_ms/2000)%vehicle.dtc_count],code);
            snprintf(diagnostics.dtc_text,sizeof(diagnostics.dtc_text),"%s (%u%s)",code,vehicle.dtc_count,vehicle.dtc_truncated?"+":"");
        } else snprintf(diagnostics.dtc_text,sizeof(diagnostics.dtc_text),"%s",vehicle.dtc_valid?"0 STORED":"HOLD TO READ");
    }
    diagnostics.wifi_active = md_ui_web_active();
    render_metrics_read(&diagnostics.refreshes_per_second,
                        &diagnostics.average_render_ms,
                        &diagnostics.render_metrics_available);

    diagnostics.signal_summary = md_ui_signal_summary(signals, now_ms);
    return diagnostics;
}

static void enqueue_settings_save(const md_ui_settings_t *settings)
{
    if (!s_settings_save_queue || !settings) return;
    if (xSemaphoreTake(s_config_mutex,portMAX_DELAY)==pdTRUE) {
        if (md_configuration_update_legacy(&s_configuration,settings)) {
            ++s_config_revision; xQueueOverwrite(s_settings_save_queue,&s_config_revision);
        }
        xSemaphoreGive(s_config_mutex);
    }
}

static bool end_preview_for_local(md_ui_settings_t *settings)
{
    bool ended=false;
    if(xSemaphoreTake(s_config_mutex,portMAX_DELAY)==pdTRUE) {
        if(s_config_preview) {
            s_configuration=s_saved_configuration; s_config_preview=false;
            s_preview_ended_locally=true; ++s_config_revision;
            md_configuration_to_legacy(&s_configuration,settings); ended=true;
        }
        xSemaphoreGive(s_config_mutex);
    }
    return ended;
}

static bool apply_rotation(const md_ui_settings_t *settings, md_ui_screen_t screen,
                           const md_ui_palette_t *palette)
{
    if (s_applied_rotation==settings->rotation) return true;
    if (!lvgl_port_lock(100)) return false;
    /* Complete outstanding refresh before changing panel address orientation. */
    lv_refr_now(s_lv_display);
    lv_display_set_rotation(s_lv_display,(lv_display_rotation_t)settings->rotation);
    const uint16_t width=lv_display_get_horizontal_resolution(s_lv_display);
    const uint16_t height=lv_display_get_vertical_resolution(s_lv_display);
    const bool geometry_changed=width!=s_ui_width || height!=s_ui_height;
    s_ui_width=width; s_ui_height=height;
    const bool ok=!geometry_changed || md_ui_screens_resize(width,height,settings,palette,screen);
    if (geometry_changed) md_ui_screens_set_calibrated(s_touch_calibrated);
    if (ok) s_applied_rotation=settings->rotation;
    s_last_press_x=s_last_press_y=-1;
    ESP_LOGI(TAG,"ROTATION degrees=%u size=%ux%u screens=%s", settings->rotation*90,
             s_ui_width,s_ui_height,ok ? "ready" : "failed");
    lvgl_port_unlock();
    return ok;
}

static bool map_touch(uint16_t raw_x, uint16_t raw_y, int16_t *x, int16_t *y)
{
    if (!md_touch_map_apply(&s_touch_map,raw_x,raw_y,x,y)) return false;
    float a=*x,b=*y;
    md_touch_rotate_point(&a,&b,s_touch_map.width,s_touch_map.height,s_applied_rotation,false);
    *x=(int16_t)a; *y=(int16_t)b; return true;
}

static void apply_event(const ui_event_t *event,
                        md_ui_state_t *state,
                        md_ui_settings_t *settings,
                        md_display_manager_t *display,
                        const md_display_profile_t *profile,
                        md_ui_palette_t *palette)
{
    /* Local interaction takes over an unsaved Web preview; stale Web writes
       then fail revision checking rather than replacing the local change. */
    if(event->type==UI_EVENT_SET_BRIGHTNESS ||
       (event->type==UI_EVENT_ACTION && (event->action==MD_UI_ACTION_OPEN_SETTINGS ||
        event->action==MD_UI_ACTION_INCREMENT || event->action==MD_UI_ACTION_DECREMENT ||
        event->action==MD_UI_ACTION_SELECT)) ||
       (event->type==UI_EVENT_TOUCH_GESTURE && event->touch_gesture==MD_UI_TOUCH_LONG_PRESS))
        (void)end_preview_for_local(settings);
    if(event->type==UI_EVENT_INSTRUMENT_PROFILE) {
        if(lvgl_port_lock(20)) {
            md_ui_screens_instrument_profile(event->canonical_signal,event->profile_ttl_ms,(uint32_t)(esp_timer_get_time()/1000));
            lvgl_port_unlock();
        }
        return;
    }
    if (event->type == UI_EVENT_SET_BRIGHTNESS) {
        if (!md_target_has(&s_target, MD_CAP_DIMMING)) return;
        settings->brightness_pct = event->brightness_pct;
        enqueue_settings_save(settings);
        ESP_LOGI(TAG, "Manual brightness=%u source=console", settings->brightness_pct);
        return;
    }

    if (event->type == UI_EVENT_CONFIG_MODE) {
        if (state->context == MD_UI_CONTEXT_DRIVING || state->sleeping) return;
        if (md_ui_web_active()) md_ui_web_stop();
        else if (md_ui_web_start()) { state->screen=MD_UI_SCREEN_CONFIG_MODE; }
        return;
    }
    if (event->type == UI_EVENT_VEHICLE_CONTEXT) {
        const md_ui_screen_t old_screen = state->screen;
        if (!md_ui_state_set_context(state, settings,
            event->vehicle_context, event->context_verified)) return;
        if (old_screen != state->screen) {
            s_touch_calibrating = false;
            if (lvgl_port_lock(20)) {
                md_ui_screens_show(state->screen, settings);
                lvgl_port_unlock();
            }
        }
        return;
    }
    if (event->type == UI_EVENT_TOUCH_PRESS) {
        int16_t x, y;
        s_last_press_x = s_last_press_y = -1;
        if (s_touch_calibrated &&
            map_touch(event->raw_x, event->raw_y, &x, &y)) {
            s_last_press_x = x;
            s_last_press_y = y;
            if (state->screen == MD_UI_SCREEN_SETTINGS && lvgl_port_lock(20)) {
                md_ui_screens_setting_action_pressed(
                    md_ui_screens_setting_action_at(x, y));
                md_ui_screens_setting_page_pressed(
                    md_ui_screens_setting_page_at(x, y));
                md_ui_screens_setting_previous_pressed(
                    md_ui_screens_setting_previous_at(x, y));
                md_ui_screens_debug_touch(x, y);
                lvgl_port_unlock();
            } else if (state->screen < MD_UI_SCREEN_SETTINGS && lvgl_port_lock(20)) {
                const md_touch_hit_t hit = md_touch_hit_zone(x, y,
                                                            s_ui_width, s_ui_height);
                md_ui_screens_nav_pressed(state->screen, settings,
                    hit == MD_TOUCH_HIT_PREVIOUS ? -1 :
                    (hit == MD_TOUCH_HIT_NEXT ? 1 : 0));
                if (hit == MD_TOUCH_HIT_CONTENT)
                    md_ui_screens_touch_feedback(state->screen, settings,
                                                 x, y, true);
                md_ui_screens_debug_touch(x, y);
                lvgl_port_unlock();
            }
        }
        return;
    }
    if (event->type == UI_EVENT_TOUCH_RELEASE &&
        state->screen == MD_UI_SCREEN_CALIBRATION) {
        if (s_calibration_step >= MD_TOUCH_CAL_POINTS) return;
        static const uint16_t targets[MD_TOUCH_CAL_POINTS][2] = {
            {35, 40}, {285, 40}, {285, 200}, {35, 200}, {160, 120}};
        md_touch_cal_point_t *point = &s_calibration_points[s_calibration_step];
        *point = (md_touch_cal_point_t){
            .raw_x = event->raw_x, .raw_y = event->raw_y,
            .screen_x = (float)targets[s_calibration_step][0] * s_ui_width / 320.0f,
            .screen_y = (float)targets[s_calibration_step][1] * s_ui_height / 240.0f};
        md_touch_rotate_point(&point->screen_x,&point->screen_y,profile->width,profile->height,s_applied_rotation,true);
        ++s_calibration_step;
        if (s_calibration_step < MD_TOUCH_CAL_POINTS) {
            if (lvgl_port_lock(20)) {
                md_ui_screens_calibration_step(s_calibration_step);
                lvgl_port_unlock();
            }
        } else {
            md_touch_map_t fitted;
            if (md_touch_map_fit(&fitted, s_calibration_points,
                                 profile->width, profile->height,
                                 CONFIG_MINIDASH_DISPLAY_ROTATION) &&
                md_touch_map_storage_save(&fitted)) {
                s_touch_map = fitted;
                s_touch_calibrated = true;
                ESP_LOGI(TAG, "Touch calibration saved");
                (void)md_ui_state_dispatch(state, settings, MD_UI_ACTION_BACK);
                s_touch_calibrating = false;
                if (lvgl_port_lock(20)) {
                    md_ui_screens_set_calibrated(true);
                    md_ui_screens_show(state->screen, settings);
                    lvgl_port_unlock();
                }
            } else {
                ESP_LOGW(TAG, "Calibration fit/storage failed; retry five points");
                s_calibration_step = 0;
                if (lvgl_port_lock(20)) {
                    md_ui_screens_calibration_step(0);
                    lvgl_port_unlock();
                }
            }
        }
        return;
    }
    if (event->type == UI_EVENT_SET_WARNING) {
        if (md_ui_settings_set_warning(settings, event->canonical_signal,
                                       event->warning_enabled,
                                       event->threshold_direction,
                                       event->warning_milli,
                                       event->critical_milli)) {
            enqueue_settings_save(settings);
        }
        return;
    }

    if (event->type==UI_EVENT_TOUCH_GESTURE && s_touch_calibrated &&
        event->touch_gesture==MD_UI_TOUCH_LONG_PRESS &&
        state->screen==MD_UI_SCREEN_DIAGNOSTICS && s_last_press_y>=36 &&
        !state->sleeping && state->context!=MD_UI_CONTEXT_DRIVING) {
        md_vehicle_app_scan(); return;
    }
    md_ui_input_source_t source = event->source;
    md_ui_action_t action = event->action;
    int details_signal = event->type == UI_EVENT_ACTION &&
        event->action == MD_UI_ACTION_OPEN_DETAILS ? event->signal_id : -1;
    if (event->type == UI_EVENT_TOUCH_RELEASE) {
        int16_t x, y;
        bool setting_action_hit = false;
        bool setting_page_hit = false;
        bool setting_previous_hit = false;
        bool setting_action_pressed = false;
        bool setting_page_pressed = false;
        bool setting_previous_pressed = false;
        if (!s_touch_calibrated || !map_touch(event->raw_x, event->raw_y, &x, &y)) return;
        if (!lvgl_port_lock(20)) return;
        md_ui_screens_nav_pressed(state->screen, settings, 0);
        md_ui_screens_touch_feedback(state->screen, settings, x, y, false);
        if (state->screen == MD_UI_SCREEN_SETTINGS) {
            setting_action_hit = md_ui_screens_setting_action_at(x, y);
            setting_page_hit = md_ui_screens_setting_page_at(x, y);
            setting_previous_hit = md_ui_screens_setting_previous_at(x, y);
            setting_action_pressed = md_ui_screens_setting_action_at(
                s_last_press_x, s_last_press_y);
            setting_page_pressed = md_ui_screens_setting_page_at(
                s_last_press_x, s_last_press_y);
            setting_previous_pressed = md_ui_screens_setting_previous_at(
                s_last_press_x, s_last_press_y);
            md_ui_screens_setting_action_pressed(false);
            md_ui_screens_setting_page_pressed(false);
            md_ui_screens_setting_previous_pressed(false);
        }
        md_ui_screens_debug_touch(x, y);
        lvgl_port_unlock();
        if (state->sleeping) action = MD_UI_ACTION_WAKE;
        else if (state->screen == MD_UI_SCREEN_FOCUS) action = MD_UI_ACTION_BACK;
        else if (state->screen == MD_UI_SCREEN_WIDGET_PICKER) {
            int selected_signal = -1;
            int pressed_signal = -1;
            if (!lvgl_port_lock(20)) return;
            selected_signal = md_ui_screens_widget_picker_signal_at(x, y);
            pressed_signal = md_ui_screens_widget_picker_signal_at(
                s_last_press_x, s_last_press_y);
            lvgl_port_unlock();
            if (selected_signal < 0 || selected_signal != pressed_signal ||
                !md_ui_state_widget_picker_set_signal(state,
                    (md_ui_signal_id_t)selected_signal)) return;
            action = MD_UI_ACTION_SELECT;
        }
        else if (state->screen == MD_UI_SCREEN_SETTINGS) {
            if (setting_action_hit && setting_action_pressed) {
                action = MD_UI_ACTION_SELECT;
            } else if (setting_page_hit && setting_page_pressed) {
                (void)md_ui_state_settings_page(state, 1);
                return;
            } else if (setting_previous_hit && setting_previous_pressed) {
                (void)md_ui_state_settings_page(state, -1);
                return;
            } else {
                const int row = md_ui_screens_setting_at(state->selected_setting, x, y);
                if (row < 0 || row != md_ui_screens_setting_at(
                    state->selected_setting, s_last_press_x, s_last_press_y)) return;
                (void)md_ui_state_select_setting(state, (uint8_t)row);
                return;
            }
        } else {
            const int dx = x - s_last_press_x;
            const int dy = y - s_last_press_y;
            if (s_last_press_x >= 0 && abs(dx) >= (int)(s_ui_width * 70 / 320) &&
                abs(dx) > 2 * abs(dy)) {
                action = dx < 0 ? MD_UI_ACTION_NEXT_SCREEN
                                : MD_UI_ACTION_PREVIOUS_SCREEN;
            } else {
            const md_touch_hit_t hit = md_touch_hit_zone(x, y,
                                                        s_ui_width, s_ui_height);
            if (hit != md_touch_hit_zone(s_last_press_x, s_last_press_y,
                                          s_ui_width, s_ui_height)) return;
            if (hit == MD_TOUCH_HIT_PREVIOUS) action = MD_UI_ACTION_PREVIOUS_SCREEN;
            else if (hit == MD_TOUCH_HIT_NEXT) action = MD_UI_ACTION_NEXT_SCREEN;
            else if (hit == MD_TOUCH_HIT_CONTENT) {
                if (!lvgl_port_lock(20)) return;
                details_signal = md_ui_screens_signal_at(state->screen, settings, x, y);
                const int pressed_signal = md_ui_screens_signal_at(state->screen,
                    settings, s_last_press_x, s_last_press_y);
                lvgl_port_unlock();
                if (details_signal < 0 || details_signal != pressed_signal) return;
                action = MD_UI_ACTION_OPEN_DETAILS;
            } else return;
            }
        }
    } else if (event->type == UI_EVENT_TOUCH_GESTURE && s_touch_calibrated &&
               event->touch_gesture == MD_UI_TOUCH_LONG_PRESS &&
               state->screen < MD_UI_SCREEN_SETTINGS) {
        if (s_last_press_x >= 0 && s_last_press_y >= 0 &&
            lvgl_port_lock(20)) {
            const int slot = md_ui_screens_widget_slot_at(state->screen, settings,
                s_last_press_x, s_last_press_y);
            lvgl_port_unlock();
            if (slot >= 0 && md_ui_state_open_widget_picker(state, settings,
                    (uint8_t)slot)) {
                if (lvgl_port_lock(20)) {
                    md_ui_screens_show(state->screen, settings);
                    lvgl_port_unlock();
                }
                return;
            }
        }
        if (s_last_press_x >= 0 && s_last_press_y >= 0 &&
            lvgl_port_lock(20)) {
            const int signal = md_ui_screens_signal_at(state->screen, settings,
                                                       s_last_press_x, s_last_press_y);
            lvgl_port_unlock();
            if (signal >= 0) return;
        }
    }
    if (event->type == UI_EVENT_TOUCH_GESTURE) {
        if (event->touch_gesture == MD_UI_TOUCH_LONG_PRESS &&
            state->screen == MD_UI_SCREEN_SETTINGS && lvgl_port_lock(20)) {
            md_ui_screens_setting_action_pressed(false);
            md_ui_screens_setting_page_pressed(false);
            md_ui_screens_setting_previous_pressed(false);
            lvgl_port_unlock();
        }
        source = MD_UI_INPUT_TOUCH;
        if (!md_ui_touch_action(state->screen, state->sleeping,
                                event->touch_gesture, &action)) return;
    } else if (event->type != UI_EVENT_ACTION &&
               event->type != UI_EVENT_TOUCH_RELEASE) {
        return;
    }
    if (!md_ui_input_valid(source, action)) return;
    if(state->screen==MD_UI_SCREEN_SETTINGS && (action==MD_UI_ACTION_SELECT ||
       action==MD_UI_ACTION_INCREMENT || action==MD_UI_ACTION_DECREMENT))
        (void)end_preview_for_local(settings);
    if (state->screen==MD_UI_SCREEN_SETTINGS && state->selected_setting==22 && action==MD_UI_ACTION_SELECT) {
        if (state->context==MD_UI_CONTEXT_DRIVING) return;
        if (md_ui_web_active()) md_ui_web_stop();
        else if (md_ui_web_start()) { state->screen=MD_UI_SCREEN_CONFIG_MODE; }
        return;
    }
    if ((state->screen==MD_UI_SCREEN_CUSTOM || (state->screen==MD_UI_SCREEN_DASHBOARD && settings->dashboard==MD_UI_DASHBOARD_CUSTOM)) && !state->sleeping &&
        (action==MD_UI_ACTION_NEXT_SCREEN || action==MD_UI_ACTION_PREVIOUS_SCREEN)) {
        if (xSemaphoreTake(s_config_mutex,portMAX_DELAY)==pdTRUE) {
            int next=(int)s_configuration.common.active_page + (action==MD_UI_ACTION_NEXT_SCREEN ? 1 : -1);
            if (next>=0 && next<s_configuration.common.page_count) {
                s_configuration.common.active_page=next;
                md_configuration_to_legacy(&s_configuration,settings);
                ++s_config_revision; xQueueOverwrite(s_settings_save_queue,&s_config_revision);
                xSemaphoreGive(s_config_mutex); return;
            }
            xSemaphoreGive(s_config_mutex);
        }
    }
#if CONFIG_MINIDASH_LCD_PIN_BL < 0
    if (state->screen == MD_UI_SCREEN_SETTINGS && state->selected_setting == 5U &&
        (action == MD_UI_ACTION_SELECT || action == MD_UI_ACTION_INCREMENT ||
         action == MD_UI_ACTION_DECREMENT)) {
        ESP_LOGI(TAG, "Backlight control is unavailable on this board wiring");
        return;
    }
#endif
    const md_ui_screen_t previous_screen = state->screen;
    const md_ui_dashboard_t previous_dashboard = settings->dashboard;
    const md_ui_settings_t previous_settings = *settings;
    const md_ui_theme_id_t previous_theme = settings->theme;
    const md_ui_day_night_t previous_day_night = settings->day_night;
    const bool previous_sleeping = state->sleeping;
    const bool changed = details_signal >= 0
        ? md_ui_state_open_details(state, settings,
                                   (md_ui_signal_id_t)details_signal)
        : md_ui_state_dispatch(state, settings, action);
    if (!changed) return;
    ESP_LOGI(TAG, "UI action source=%d action=%d screen=%d",
             (int)source, (int)action, (int)state->screen);
    if (previous_screen != MD_UI_SCREEN_CALIBRATION &&
        state->screen == MD_UI_SCREEN_CALIBRATION) {
        s_calibration_step = 0;
        s_touch_calibrating = true;
        if (lvgl_port_lock(20)) {
            md_ui_screens_calibration_step(0);
            lvgl_port_unlock();
        }
    } else if (previous_screen == MD_UI_SCREEN_CALIBRATION) {
        s_touch_calibrating = false;
    }

    if (previous_sleeping != state->sleeping && lvgl_port_lock(0)) {
        md_display_power(display, !state->sleeping);
        lvgl_port_unlock();
    }
    if (previous_theme != settings->theme || previous_day_night != settings->day_night) {
        *palette = md_ui_theme_palette(settings->theme, effective_day_mode(settings));
        if (lvgl_port_lock(100)) {
            md_ui_screens_retheme(palette, settings->theme);
            lvgl_port_unlock();
        }
    }
    if (previous_settings.layout != settings->layout ||
        memcmp(previous_settings.widgets, settings->widgets,
               sizeof(settings->widgets)) != 0) {
        if (lvgl_port_lock(100)) {
            md_ui_screens_rebuild_custom(settings);
            lvgl_port_unlock();
        }
    }


    if (previous_screen==MD_UI_SCREEN_DASHBOARD && state->screen==MD_UI_SCREEN_CUSTOM &&
        action==MD_UI_ACTION_PREVIOUS_SCREEN) {
        if (xSemaphoreTake(s_config_mutex,portMAX_DELAY)==pdTRUE) {
            s_configuration.common.active_page=s_configuration.common.page_count-1;
            md_configuration_to_legacy(&s_configuration,settings);
            xSemaphoreGive(s_config_mutex);
        }
    }
    if (previous_screen != state->screen || previous_dashboard != settings->dashboard) {
        if (!state->sleeping && lvgl_port_lock(100)) {
            if (state->screen == MD_UI_SCREEN_FOCUS)
                md_ui_screens_focus_signal(state->focused_signal);
            md_ui_screens_show(state->screen, settings);
            lvgl_port_unlock();
        }
    }
    if (previous_settings.rotation != settings->rotation)
        (void)apply_rotation(settings,state->screen,palette);
    if (memcmp(settings, &previous_settings, sizeof(*settings)) != 0)
        enqueue_settings_save(settings);
}

static void startup_effect(md_display_manager_t *display, lv_display_t *lv_display, uint8_t target)
{
    bool visual=false;
    (void)md_display_set_brightness(display,0);
    if(lvgl_port_lock(0)) {
        visual=md_ui_screens_startup_begin();
        lv_refr_now(lv_display); /* Flush the dark masked frame before lighting it. */
        lvgl_port_unlock();
    }
    const int64_t started=esp_timer_get_time();
    const bool fade_ok=md_backlight_startup_fade(target,MD_UI_STARTUP_DURATION_MS);
    uint8_t applied=0;
    ESP_LOGI(TAG,"STARTUP begin duration_ms=%u target=%u visual=%s",
             MD_UI_STARTUP_DURATION_MS,target,visual ? "CRT" : "fade_only");
    for(;fade_ok;) {
        const uint32_t elapsed=(uint32_t)((esp_timer_get_time()-started)/1000);
        const md_ui_startup_frame_t frame=md_ui_startup_frame(elapsed,1,1,target);
        if(visual && lvgl_port_lock(20)) {
            md_ui_screens_startup_frame(elapsed);
            lvgl_port_unlock();
        }
        if(frame.complete) {
            if(md_display_set_brightness(display,target)) applied=target;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    if(lvgl_port_lock(0)) {
        md_ui_screens_startup_end();
        lvgl_port_unlock();
    }
    ESP_LOGI(TAG,"STARTUP end elapsed_ms=%lu brightness=%u",
             (unsigned long)((esp_timer_get_time()-started)/1000),applied);
}

static void MD_DISPLAY_TASK_UNUSED ui_task(void *argument)
{
    (void)argument;
    md_display_manager_t display = {0};
    md_display_profile_t profile = {0};
    if (!open_display(&display, &profile)) {
        s_display_ready = false;
        ESP_LOGE(TAG, "Display initialization failed; UI task stopped");
        vTaskDelete(NULL);
        return;
    }
#if CONFIG_MINIDASH_DISPLAY_SMOKE_TEST
    vTaskDelete(NULL);
    return;
#endif

    md_ui_settings_t settings;
    s_target=(md_target_profile_t){.name="esp32-s3-color", .hardware=MD_CAP_COLOR|MD_CAP_WIFI|MD_CAP_BLE|MD_CAP_LARGE_MEMORY|MD_CAP_PSRAM,
        .implemented_features=(1U<<MD_FEATURE_TELEMETRY)|(1U<<MD_FEATURE_WARNINGS)|(1U<<MD_FEATURE_PAGES)|
            (1U<<MD_FEATURE_FOCUS)|(1U<<MD_FEATURE_CONFIG_IO)|(1U<<MD_FEATURE_WEB_CONFIG),
        .display_width=profile.width,.display_height=profile.height,.flash_bytes=16777216,.ram_bytes=524288,
        .visible_widgets=MD_UI_MAX_WIDGETS,.verified=true};
#if CONFIG_MINIDASH_TOUCH_ENABLE
    s_target.hardware |= MD_CAP_TOUCH;
#endif
#if CONFIG_MINIDASH_LCD_PIN_BL >= 0
    s_target.hardware |= MD_CAP_DIMMING;
#endif
    (void)md_configuration_storage_load(&s_configuration,&s_target);
    /* platform.rotation is now relative to the verified board orientation. */
    s_saved_configuration=s_configuration; s_config_revision=1; s_config_ready=true;
    (void)md_configuration_to_legacy(&s_configuration,&settings);
    /* The 92-signal snapshot must not share the 8 KiB controller stack with
     * settings migration and nested rendering. Allocate once, never per frame. */
    md_device_configuration_t *visible_config=malloc(sizeof(*visible_config));
    md_ui_signal_store_t *signals=malloc(sizeof(*signals));
    if (!visible_config || !signals) {
        free(visible_config); free(signals);
        ESP_LOGE(TAG, "UI snapshot allocation failed");
        vTaskDelete(NULL); return;
    }
    *visible_config=s_configuration; uint32_t seen_revision=s_config_revision;
    uint8_t applied_brightness = settings.brightness_pct;
    md_ui_state_t state;
    md_ui_state_init(&state, &settings);
    bool was_preview=false; md_ui_screen_t preview_return_screen=state.screen;
    s_touch_calibrated = md_touch_map_storage_load(&s_touch_map,
        profile.width, profile.height, CONFIG_MINIDASH_DISPLAY_ROTATION);
    md_ui_palette_t palette = md_ui_theme_palette(settings.theme,
                                                   effective_day_mode(&settings));

    md_display_orientation_t orientation;
    if (!md_display_orientation_select(&profile, &orientation)) {
        ESP_LOGE(TAG, "Unsupported display orientation");
        free(visible_config); free(signals);
        vTaskDelete(NULL);
        return;
    }

    lvgl_port_cfg_t lvgl_config = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_config.task_priority = 4;
    lvgl_config.task_stack = 8192;
    lvgl_config.task_stack_caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_DEFAULT;
    if (lvgl_port_init(&lvgl_config) != ESP_OK) {
        ESP_LOGE(TAG, "LVGL port initialization failed");
        free(visible_config); free(signals);
        vTaskDelete(NULL);
        return;
    }

    const lvgl_port_display_cfg_t display_config = {
        .io_handle = md_display_io(&display),
        .panel_handle = md_display_panel(&display),
        .buffer_size = (uint32_t)profile.width * 16U,
        .double_buffer = true,
        .hres = profile.width,
        .vres = profile.height,
        .monochrome = false,
        .rotation = {.swap_xy = orientation.swap_xy,
                     .mirror_x = orientation.mirror_x,
                     .mirror_y = orientation.mirror_y},
        .flags = {.buff_dma = true, .swap_bytes = true},
        .color_format = LV_COLOR_FORMAT_RGB565,
    };
    lv_display_t *lv_display = lvgl_port_add_disp(&display_config);
    s_lv_display = lv_display;
    if (!lv_display) {
        ESP_LOGE(TAG, "Could not attach LCD to LVGL");
        free(visible_config); free(signals);
        vTaskDelete(NULL);
        return;
    }
    if (!lvgl_port_lock(0)) {
        ESP_LOGE(TAG, "Could not acquire LVGL during initial screen creation");
        free(visible_config); free(signals);
        vTaskDelete(NULL);
        return;
    }
    lv_display_add_event_cb(lv_display, render_event_cb, LV_EVENT_RENDER_START, NULL);
    lv_display_add_event_cb(lv_display, render_event_cb, LV_EVENT_RENDER_READY, NULL);
    lv_display_set_rotation(lv_display, (lv_display_rotation_t)settings.rotation);
    s_applied_rotation=settings.rotation;
    s_ui_width=lv_display_get_horizontal_resolution(lv_display);
    s_ui_height=lv_display_get_vertical_resolution(lv_display);
    md_ui_screens_configuration(visible_config);
    const bool screens_ready = md_ui_screens_create(s_ui_width, s_ui_height,
                                                     &settings, &palette);
    if (screens_ready) {
        md_ui_screens_set_calibrated(s_touch_calibrated);
        md_ui_lvgl_renderer()->show(NULL,state.screen, &settings);
    }
    lvgl_port_unlock();
    if (!screens_ready) {
        ESP_LOGE(TAG, "Could not create LVGL screens");
        free(visible_config); free(signals);
        vTaskDelete(NULL);
        return;
    }

    startup_effect(&display,lv_display,settings.brightness_pct);
    s_display_ready = true;
    ESP_LOGI(TAG, "UI ready with %u x %u partial buffers and %u ms model refresh",
             profile.width, profile.height, UI_UPDATE_PERIOD_MS);

#if CONFIG_MINIDASH_TOUCH_ENABLE
    if (xTaskCreate(touch_input_task, "ui_touch", 4096, NULL, 4, NULL) != pdPASS) {
        ESP_LOGW(TAG, "Touch reader task could not start");
    }
#else
    ESP_LOGI(TAG, "Touch input is disabled");
#endif

    TickType_t last_update = xTaskGetTickCount();
    md_analytics_config_t applied_analytics=visible_config->common.analytics;
    (void)md_vehicle_app_analytics_configure(&applied_analytics);
    for (;;) {
        if (xSemaphoreTake(s_config_mutex,0)==pdTRUE) {
            s_config_driving=state.context==MD_UI_CONTEXT_DRIVING;
            if (seen_revision!=s_config_revision) {
                *visible_config=s_configuration;
                if (s_config_preview && !was_preview) preview_return_screen=state.screen;
                if (s_config_preview) state.screen=MD_UI_SCREEN_CUSTOM;
                else if (was_preview && !s_preview_ended_locally) state.screen=preview_return_screen;
                s_preview_ended_locally=false;
                was_preview=s_config_preview;
                md_configuration_to_legacy(visible_config,&settings);
                if (lvgl_port_lock(100)) {
                    md_ui_screens_configuration(visible_config);
                    palette=visible_config->common.custom_theme ? visible_config->common.palette :
                        md_ui_theme_palette(settings.theme,effective_day_mode(&settings));
                    md_ui_screens_retheme(&palette, settings.theme); md_ui_screens_rebuild_custom(&settings);
                    md_ui_lvgl_renderer()->show(NULL,state.screen,&settings); seen_revision=s_config_revision; lvgl_port_unlock();
                }
            }
            xSemaphoreGive(s_config_mutex);
        }
        if (state.context==MD_UI_CONTEXT_DRIVING && md_ui_web_active()) md_ui_web_stop();
        if(memcmp(&applied_analytics,&visible_config->common.analytics,sizeof(applied_analytics)) && md_vehicle_app_analytics_configure(&visible_config->common.analytics))applied_analytics=visible_config->common.analytics;
        if (state.screen==MD_UI_SCREEN_CONFIG_MODE) {
            if (!md_ui_web_active()) { state.screen=MD_UI_SCREEN_SETTINGS; if (lvgl_port_lock(100)) { md_ui_screens_show(state.screen,&settings); lvgl_port_unlock(); } }
            else if (lvgl_port_lock(20)) { md_ui_screens_config_credentials(md_ui_web_ssid(),md_ui_web_password()); md_ui_screens_show(state.screen,&settings); lvgl_port_unlock(); }
        }
        atomic_store(&s_visible_screen,state.screen);
        uint32_t demand=0,warning_demand=0;
        if(!state.sleeping) {
            if(state.screen==MD_UI_SCREEN_FOCUS) demand=md_derived_raw_demand(state.focused_signal);
            else {
                md_ui_card_group_t group=md_ui_card_group_for_screen(state.screen,&settings);
                if(group==MD_UI_CARD_GROUP_CUSTOM) {
                    for(unsigned i=0;i<settings.widget_count;i++) {
                        int id=md_ui_signal_id_from_name(settings.widgets[i].canonical_signal);
                        if(id>=0 && id<MD_SIGNAL_COUNT)demand|=md_derived_raw_demand(id);
                    }
                } else if(group>=0 && group<MD_UI_CARD_GROUP_COUNT) {
                    for(unsigned i=0;i<MD_UI_CARD_SLOTS_PER_GROUP;i++) {
                        int id=md_ui_settings_card_signal(&settings,group,i);
                        if(id>=0 && id<MD_SIGNAL_COUNT)demand|=md_derived_raw_demand(id);
                    }
                }
            }
        }
        if(settings.warnings_enabled) for(unsigned i=0;i<MD_UI_MAX_WARNING_THRESHOLDS;i++) {
            if(settings.warnings[i].enabled)warning_demand|=md_derived_raw_demand(settings.warnings[i].signal_id);
        }
        md_vehicle_app_demand(demand,warning_demand);
        (void)apply_rotation(&settings,state.screen,&palette);
        if (applied_brightness != settings.brightness_pct) {
            if (md_display_set_brightness(&display, settings.brightness_pct))
                applied_brightness = settings.brightness_pct;
            else ESP_LOGE(TAG, "Backlight update failed");
        }
        ui_event_t event;
        if (xQueueReceive(s_ui_event_queue, &event, pdMS_TO_TICKS(20)) == pdTRUE) {
            apply_event(&event, &state, &settings, &display, &profile, &palette);
            while (xQueueReceive(s_ui_event_queue, &event, 0) == pdTRUE) {
                apply_event(&event, &state, &settings, &display, &profile, &palette);
            }
        }

        const TickType_t now_ticks = xTaskGetTickCount();
        if ((now_ticks - last_update) < pdMS_TO_TICKS(UI_UPDATE_PERIOD_MS)) continue;
        last_update = now_ticks;
        if (state.sleeping || !lvgl_port_lock(20)) continue;

        portENTER_CRITICAL(&s_signal_mux);
        *signals = s_signal_store;
        portEXIT_CRITICAL(&s_signal_mux);
        const uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
        const md_ui_diagnostics_t diagnostics = diagnostics_snapshot(now_ms,signals);
        md_ui_lvgl_renderer()->update(NULL,state.screen, &settings, &state, signals,
                             now_ms, &diagnostics);
        lvgl_port_unlock();
    }
}

void md_display_app_start(void)
{
#if CONFIG_MINIDASH_DISPLAY_ENABLE
    md_ui_signal_store_init(&s_signal_store);
    s_ui_event_queue = xQueueCreate(UI_EVENT_QUEUE_LENGTH, sizeof(ui_event_t));
    s_settings_save_queue = xQueueCreate(1, sizeof(uint32_t));
    s_config_mutex=xSemaphoreCreateMutex();
    if (!s_ui_event_queue || !s_settings_save_queue || !s_config_mutex) {
        ESP_LOGE(TAG, "Could not allocate fixed UI queues");
        return;
    }
    if (xTaskCreate(settings_storage_task, "ui_settings", 4096, NULL, 2, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Could not start settings storage task");
        return;
    }
    if (xTaskCreate(ui_task, "ui_controller", MD_UI_TASK_STACK_BYTES, NULL, 3, &s_ui_task) != pdPASS) {
        ESP_LOGE(TAG, "Could not start UI controller task");
        s_ui_task = NULL;
        return;
    }
#else
    ESP_LOGI(TAG, "Display disabled pending verified board configuration");
#endif
}

bool md_display_app_dispatch(md_ui_input_source_t source, md_ui_action_t action)
{
    if (!s_ui_event_queue || !md_ui_input_valid(source, action) ||
        action == MD_UI_ACTION_OPEN_DETAILS) return false;
    const ui_event_t event = {.type = UI_EVENT_ACTION, .source = source, .action = action};
    return xQueueSend(s_ui_event_queue, &event, 0) == pdTRUE;
}

bool md_display_app_open_details(md_ui_input_source_t source,
                                 md_ui_signal_id_t signal)
{
    if (!s_ui_event_queue ||
        !md_ui_input_valid(source, MD_UI_ACTION_OPEN_DETAILS) ||
        signal < 0 || signal >= MD_SIGNAL_COUNT) return false;
    const ui_event_t event = {.type = UI_EVENT_ACTION, .source = source,
        .action = MD_UI_ACTION_OPEN_DETAILS, .signal_id = signal};
    return xQueueSend(s_ui_event_queue, &event, 0) == pdTRUE;
}

bool md_display_app_publish_instrument_profile(const char *frame)
{
    ui_event_t event={.type=UI_EVENT_INSTRUMENT_PROFILE};
    if(!s_ui_event_queue || !md_instrument_frame_decode(frame,event.canonical_signal,sizeof(event.canonical_signal),&event.profile_ttl_ms)) return false;
    return xQueueSend(s_ui_event_queue,&event,0)==pdTRUE;
}

bool md_display_app_set_vehicle_context(md_ui_context_t context, bool verified)
{
    if (!s_ui_event_queue || context < MD_UI_CONTEXT_UNKNOWN ||
        context > MD_UI_CONTEXT_DRIVING ||
        (context != MD_UI_CONTEXT_UNKNOWN && !verified)) return false;
    const ui_event_t event = {.type = UI_EVENT_VEHICLE_CONTEXT,
        .vehicle_context = context, .context_verified = verified};
    return xQueueSend(s_ui_event_queue, &event, 0) == pdTRUE;
}

bool md_display_app_publish_signal(const char *canonical_signal, double value,
                                   md_ui_signal_state_t state, uint32_t ttl_ms,
                                   bool demo)
{
    if (!canonical_signal) return false;
    const int id = md_ui_signal_id_from_name(canonical_signal);
    const uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
    portENTER_CRITICAL(&s_signal_mux);
    bool published;
    if (id < 0) {
        ++s_signal_store.unknown_signal_updates;
        published = false;
    } else {
        published = md_ui_signal_store_publish_id(&s_signal_store,
            (md_ui_signal_id_t)id, value, state, now_ms, ttl_ms, demo);
    }
    portEXIT_CRITICAL(&s_signal_mux);
    return published;
}

bool md_display_app_publish_serial_sample(const char *frame)
{
    md_display_serial_sample_t sample;
    if (!md_display_serial_sample_decode(frame, &sample)) return false;
    return md_display_app_publish_signal(sample.canonical_signal, sample.value,
                                         sample.state, sample.ttl_ms, false);
}
int md_display_analytics_reset(uint32_t revision,bool session)
{
    if(!s_config_mutex || xSemaphoreTake(s_config_mutex,pdMS_TO_TICKS(100))!=pdTRUE)return 500;
    int result=revision!=s_config_revision?409:s_config_driving?403:200;
    xSemaphoreGive(s_config_mutex);
    if(result==200)md_vehicle_app_analytics_reset(session);
    return result;
}

bool md_display_app_set_warning(const char *canonical_signal, bool enabled,
                                md_ui_threshold_direction_t direction,
                                int32_t warning_milli, int32_t critical_milli)
{
    if (!s_ui_event_queue || !canonical_signal) return false;
    ui_event_t event = {
        .type = UI_EVENT_SET_WARNING,
        .warning_enabled = enabled,
        .threshold_direction = direction,
        .warning_milli = warning_milli,
        .critical_milli = critical_milli,
    };
    (void)snprintf(event.canonical_signal, sizeof(event.canonical_signal), "%s",
                   canonical_signal);
    return xQueueSend(s_ui_event_queue, &event, 0) == pdTRUE;
}

void md_display_app_command(const char *command)
{
    md_ui_input_source_t source;
    md_ui_action_t action;
    if (!md_ui_input_command(command, &source, &action) ||
        !md_display_app_dispatch(source, action)) {
        ESP_LOGW(TAG, "Unknown or dropped UI command");
    }
}

bool md_display_app_config_mode(void)
{
    if (!s_ui_event_queue) return false;
    ui_event_t event={.type=UI_EVENT_CONFIG_MODE};
    return xQueueSend(s_ui_event_queue,&event,0)==pdTRUE;
}

void md_display_app_configuration_dump(void)
{
    md_device_configuration_t *config=malloc(sizeof(*config)); uint32_t revision;
    if (config && md_display_configuration_read(config,&revision,NULL)) {
        cJSON *root=md_configuration_json(config,revision); char *text=cJSON_PrintUnformatted(root);
        if (text) { printf("MINIDASH_CONFIG_JSON %s\n",text); free(text); }
        cJSON_Delete(root);
    }
    free(config); fflush(stdout);
}

md_ui_screen_t md_display_app_current_screen(void)
{
    return (md_ui_screen_t)atomic_load(&s_visible_screen);
}

bool md_display_app_restore_legacy(void)
{
    if (md_ui_web_active() || !s_config_mutex || xSemaphoreTake(s_config_mutex,pdMS_TO_TICKS(500))!=pdTRUE) return false;
    nvs_handle_t handle; size_t length=0; bool existing=false;
    if (nvs_open("minidash_ui",NVS_READONLY,&handle)==ESP_OK) {
        existing=nvs_get_blob(handle,"settings",NULL,&length)==ESP_OK && length>0; nvs_close(handle);
    }
    md_ui_settings_t legacy; md_device_configuration_t *config=malloc(sizeof(*config));
    bool ok=existing && config && s_config_ready && !s_config_driving &&
        md_ui_settings_storage_load(&legacy) && md_configuration_from_legacy(config,&legacy,&s_target);
    if (ok) {
        config->platform.rotation=s_configuration.platform.rotation;
        ok=md_configuration_storage_save(config);
        if (ok) { s_configuration=s_saved_configuration=*config; s_config_preview=false; ++s_config_revision; }
    }
    free(config); xSemaphoreGive(s_config_mutex); return ok;
}

bool md_display_app_set_brightness(uint8_t percent)
{
    if (!s_ui_event_queue || !s_display_ready || percent < 10 || percent > 100 ||
        !md_target_has(&s_target, MD_CAP_DIMMING)) return false;
    const ui_event_t event = {.type = UI_EVENT_SET_BRIGHTNESS, .brightness_pct = percent};
    return xQueueSend(s_ui_event_queue, &event, 0) == pdTRUE;
}

bool md_display_app_view_selftest(void)
{
    if(!s_display_ready || !lvgl_port_lock(500)) return false;
    const bool ok=md_ui_screens_selftest(); lvgl_port_unlock(); return ok;
}

bool md_display_app_gauge_dump(void)
{
    if(!s_ui_task || !lvgl_port_lock(200)) return false;
    md_ui_screens_gauge_dump((md_ui_screen_t)atomic_load(&s_visible_screen));
    lvgl_port_unlock(); return true;
}
