#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "ui_core.h"
#include "ui_theme.h"
#include "ui_configuration.h"



bool md_ui_screens_create(uint16_t width, uint16_t height,
                          const md_ui_settings_t *settings,
                          const md_ui_palette_t *palette);
void md_ui_screens_rebuild_custom(const md_ui_settings_t *settings);
void md_ui_screens_configuration(const md_device_configuration_t *config);
void md_ui_screens_config_credentials(const char *ssid, const char *password);
const md_ui_renderer_t *md_ui_lvgl_renderer(void);
void md_ui_screens_show(md_ui_screen_t screen,
                        const md_ui_settings_t *settings);
void md_ui_screens_set_calibrated(bool calibrated);
void md_ui_screens_focus_signal(md_ui_signal_id_t signal);
int md_ui_screens_signal_at(md_ui_screen_t screen,
                            const md_ui_settings_t *settings,
                            int16_t x, int16_t y);
int md_ui_screens_widget_slot_at(md_ui_screen_t screen,
                                 const md_ui_settings_t *settings,
                                 int16_t x, int16_t y);
int md_ui_screens_widget_picker_signal_at(int16_t x, int16_t y);
int md_ui_screens_setting_at(uint8_t selected_setting, int16_t x, int16_t y);
bool md_ui_screens_setting_action_at(int16_t x, int16_t y);
void md_ui_screens_setting_action_pressed(bool pressed);
bool md_ui_screens_setting_page_at(int16_t x, int16_t y);
void md_ui_screens_setting_page_pressed(bool pressed);
bool md_ui_screens_setting_previous_at(int16_t x, int16_t y);
void md_ui_screens_setting_previous_pressed(bool pressed);
void md_ui_screens_calibration_step(uint8_t step);
void md_ui_screens_nav_pressed(md_ui_screen_t screen,
                                const md_ui_settings_t *settings,
                                int direction);
void md_ui_screens_touch_feedback(md_ui_screen_t screen,
                                   const md_ui_settings_t *settings,
                                   int16_t x, int16_t y, bool pressed);
void md_ui_screens_debug_touch(int16_t x, int16_t y);
void md_ui_screens_retheme(const md_ui_palette_t *palette, md_ui_theme_id_t theme);
void md_ui_screens_update(md_ui_screen_t screen,
                          const md_ui_settings_t *settings,
                          const md_ui_state_t *state,
                          const md_ui_signal_store_t *signals,
                          uint32_t now_ms,
                          const md_ui_diagnostics_t *diagnostics);

/* Call under the LVGL lock; overlay objects never receive input. */
bool md_ui_screens_startup_begin(void);
void md_ui_screens_startup_frame(uint32_t elapsed_ms);
void md_ui_screens_startup_end(void);
