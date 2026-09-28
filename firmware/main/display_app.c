#include "display_app.h"

#include <stdio.h>
#include <string.h>

#include "display_manager.h"
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
#include "ui_theme.h"
#include "xpt2046_touch.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define UI_UPDATE_PERIOD_MS 100U
#define UI_SETTINGS_DEBOUNCE_MS 1500U
#define UI_EVENT_QUEUE_LENGTH 16U
#define UI_TOUCH_SAMPLE_PERIOD_MS 25U

typedef enum {
    UI_EVENT_ACTION = 0,
    UI_EVENT_TOUCH_GESTURE,
    UI_EVENT_SET_WARNING,
} ui_event_type_t;

typedef struct {
    ui_event_type_t type;
    md_ui_input_source_t source;
    md_ui_action_t action;
    md_ui_touch_gesture_t touch_gesture;
    char canonical_signal[MD_UI_SIGNAL_NAME_LEN];
    bool warning_enabled;
    md_ui_threshold_direction_t threshold_direction;
    int32_t warning_milli;
    int32_t critical_milli;
} ui_event_t;

static const char *TAG = "md_ui";
static QueueHandle_t s_ui_event_queue;
static QueueHandle_t s_settings_save_queue;
static TaskHandle_t s_ui_task;
static md_ui_signal_store_t s_signal_store;
static portMUX_TYPE s_signal_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool s_display_ready;
static volatile bool s_touch_ready;

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

static void settings_storage_task(void *argument)
{
    (void)argument;
    md_ui_settings_t pending;
    for (;;) {
        if (xQueueReceive(s_settings_save_queue, &pending, portMAX_DELAY) != pdTRUE) continue;
        TickType_t quiet_since = xTaskGetTickCount();
        const TickType_t quiet_period = pdMS_TO_TICKS(UI_SETTINGS_DEBOUNCE_MS);
        for (;;) {
            const TickType_t elapsed = xTaskGetTickCount() - quiet_since;
            if (elapsed >= quiet_period) break;
            md_ui_settings_t latest;
            if (xQueueReceive(s_settings_save_queue, &latest,
                              quiet_period - elapsed) == pdTRUE) {
                pending = latest;
                quiet_since = xTaskGetTickCount();
            }
        }
        if (!md_ui_settings_storage_save(&pending)) {
            ESP_LOGW(TAG, "Settings were not saved; current session remains active");
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
    ESP_LOGI(TAG, "Touch input ready; full-screen gestures do not require coordinate calibration");

    bool pressed = false;
    bool long_press_sent = false;
    bool pending_tap = false;
    uint8_t pressed_samples = 0;
    uint8_t released_samples = 0;
    uint32_t pressed_at = 0;
    uint32_t press_candidate_at = 0;
    uint32_t pending_tap_at = 0;
    for (;;) {
        md_touch_sample_t sample = {0};
        if (md_xpt2046_read(&touch, &sample)) {
            const uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
            if (sample.pressed) {
                if (pressed_samples == 0 && !pressed) press_candidate_at = now_ms;
                pressed_samples = pressed_samples < 2 ? pressed_samples + 1U : 2U;
                released_samples = 0;
                if (!pressed && pressed_samples >= 2U) {
                    pressed = true;
                    pressed_at = press_candidate_at;
                    long_press_sent = false;
                }
                if (pressed && !long_press_sent &&
                    (uint32_t)(now_ms - pressed_at) >= 700U) {
                    enqueue_touch_gesture(MD_UI_TOUCH_LONG_PRESS);
                    long_press_sent = true;
                }
            } else {
                pressed_samples = 0;
                if (pressed) {
                    released_samples = released_samples < 2 ? released_samples + 1U : 2U;
                    if (released_samples >= 2U) {
                        pressed = false;
                        released_samples = 0;
                        if (!long_press_sent) {
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
                    }
                }
            }
            if (pending_tap && !pressed && pressed_samples == 0 &&
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

static md_ui_diagnostics_t diagnostics_snapshot(uint32_t now_ms)
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
    render_metrics_read(&diagnostics.refreshes_per_second,
                        &diagnostics.average_render_ms,
                        &diagnostics.render_metrics_available);

    md_ui_signal_store_t snapshot;
    portENTER_CRITICAL(&s_signal_mux);
    snapshot = s_signal_store;
    portEXIT_CRITICAL(&s_signal_mux);
    diagnostics.signal_summary = md_ui_signal_summary(&snapshot, now_ms);
    return diagnostics;
}

static void enqueue_settings_save(const md_ui_settings_t *settings)
{
    if (s_settings_save_queue && settings) {
        (void)xQueueOverwrite(s_settings_save_queue, settings);
    }
}

static void apply_event(const ui_event_t *event,
                        md_ui_state_t *state,
                        md_ui_settings_t *settings,
                        md_display_manager_t *display,
                        const md_display_profile_t *profile,
                        md_ui_palette_t *palette)
{
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

    md_ui_input_source_t source = event->source;
    md_ui_action_t action = event->action;
    if (event->type == UI_EVENT_TOUCH_GESTURE) {
        source = MD_UI_INPUT_TOUCH;
        if (!md_ui_touch_action(state->screen, state->sleeping,
                                event->touch_gesture, &action)) return;
    } else if (event->type != UI_EVENT_ACTION) {
        return;
    }
    if (!md_ui_input_valid(source, action)) return;
    const md_ui_screen_t previous_screen = state->screen;
    const md_ui_dashboard_t previous_dashboard = settings->dashboard;
    const md_ui_theme_id_t previous_theme = settings->theme;
    const md_ui_day_night_t previous_day_night = settings->day_night;
    const uint8_t previous_brightness = settings->brightness_pct;
    const bool previous_sleeping = state->sleeping;
    const bool changed = md_ui_state_dispatch(state, settings, action);
    if (!changed) return;

    if (previous_sleeping != state->sleeping && lvgl_port_lock(0)) {
        md_display_power(display, !state->sleeping);
        lvgl_port_unlock();
    }
    if (previous_theme != settings->theme || previous_day_night != settings->day_night) {
        *palette = md_ui_theme_palette(settings->theme, effective_day_mode(settings));
        if (lvgl_port_lock(100)) {
            md_ui_screens_retheme(palette);
            lvgl_port_unlock();
        }
    }
    if (previous_brightness != settings->brightness_pct && profile->width > 0) {
        if (!md_display_set_brightness(display, settings->brightness_pct)) {
            ESP_LOGD(TAG, "Brightness value saved; this display has no PWM backlight control");
        }
    }

    if (previous_screen != state->screen || previous_dashboard != settings->dashboard) {
        if (!state->sleeping && lvgl_port_lock(100)) {
            md_ui_screens_show(state->screen, settings);
            lvgl_port_unlock();
        }
    }
    enqueue_settings_save(settings);
}

static void ui_task(void *argument)
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
    (void)md_ui_settings_storage_load(&settings);
    md_ui_state_t state;
    md_ui_state_init(&state, &settings);
    md_ui_palette_t palette = md_ui_theme_palette(settings.theme,
                                                   effective_day_mode(&settings));

    md_display_orientation_t orientation;
    if (!md_display_orientation_select(&profile, &orientation)) {
        ESP_LOGE(TAG, "Unsupported display orientation");
        vTaskDelete(NULL);
        return;
    }

    lvgl_port_cfg_t lvgl_config = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_config.task_priority = 4;
    lvgl_config.task_stack = 8192;
    lvgl_config.task_stack_caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_DEFAULT;
    if (lvgl_port_init(&lvgl_config) != ESP_OK) {
        ESP_LOGE(TAG, "LVGL port initialization failed");
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
    if (!lv_display) {
        ESP_LOGE(TAG, "Could not attach LCD to LVGL");
        vTaskDelete(NULL);
        return;
    }
    if (!lvgl_port_lock(0)) {
        ESP_LOGE(TAG, "Could not acquire LVGL during initial screen creation");
        vTaskDelete(NULL);
        return;
    }
    lv_display_add_event_cb(lv_display, render_event_cb, LV_EVENT_RENDER_START, NULL);
    lv_display_add_event_cb(lv_display, render_event_cb, LV_EVENT_RENDER_READY, NULL);
    const bool screens_ready = md_ui_screens_create(profile.width, profile.height,
                                                     &settings, &palette);
    if (screens_ready) md_ui_screens_show(state.screen, &settings);
    lvgl_port_unlock();
    if (!screens_ready) {
        ESP_LOGE(TAG, "Could not create LVGL screens");
        vTaskDelete(NULL);
        return;
    }

    s_display_ready = true;
    (void)md_display_set_brightness(&display, settings.brightness_pct);
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
    for (;;) {
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

        md_ui_signal_store_t signals;
        portENTER_CRITICAL(&s_signal_mux);
        signals = s_signal_store;
        portEXIT_CRITICAL(&s_signal_mux);
        const uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
        const md_ui_diagnostics_t diagnostics = diagnostics_snapshot(now_ms);
        md_ui_screens_update(state.screen, &settings, &state, &signals,
                             now_ms, &diagnostics);
        lvgl_port_unlock();
    }
}

void md_display_app_start(void)
{
#if CONFIG_MINIDASH_DISPLAY_ENABLE
    md_ui_signal_store_init(&s_signal_store);
    s_ui_event_queue = xQueueCreate(UI_EVENT_QUEUE_LENGTH, sizeof(ui_event_t));
    s_settings_save_queue = xQueueCreate(1, sizeof(md_ui_settings_t));
    if (!s_ui_event_queue || !s_settings_save_queue) {
        ESP_LOGE(TAG, "Could not allocate fixed UI queues");
        return;
    }
    if (xTaskCreate(settings_storage_task, "ui_settings", 4096, NULL, 2, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Could not start settings storage task");
        return;
    }
    if (xTaskCreate(ui_task, "ui_controller", 8192, NULL, 3, &s_ui_task) != pdPASS) {
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
    if (!s_ui_event_queue || !md_ui_input_valid(source, action)) return false;
    const ui_event_t event = {.type = UI_EVENT_ACTION, .source = source, .action = action};
    return xQueueSend(s_ui_event_queue, &event, 0) == pdTRUE;
}

bool md_display_app_publish_signal(const char *canonical_signal, float value,
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
