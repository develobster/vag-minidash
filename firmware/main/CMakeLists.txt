idf_component_register(
    SRCS "main.c" "backlight_controller.c" "display_manager.c" "st7789_backend.c"
         "ili9341_backend.c" "xpt2046_touch.c" "display_app.c"
         "display_serial_protocol.c" "diagnostic_transport.c" "acquisition.c" "vehicle_app.c" "ui_instrument_profiles.c"
         "window_statistics.c" "performance_timer.c" "derived_engine.c"
         "ui_core.c" "ui_core_selftest.c" "ui_theme.c" "ui_startup.c" "ui_i18n.c" "ui_screens.c"
         "ui_font_ru_10.c" "ui_font_ru_12.c" "ui_font_ru_14.c" "ui_font_ru_16.c"
         "ui_font_mono_10.c" "ui_font_mono_12.c" "ui_font_mono_14.c" "ui_font_mono_16.c" "ui_font_mono_20.c" "ui_font_mono_28.c" "ui_font_mono_48.c" "ui_font_cluster_16.c" "ui_font_cluster_20.c" "ui_font_cluster_28.c" "ui_font_cluster_48.c"
         "ui_settings_nvs.c" "ui_platform.c" "ui_model.c"
         "ui_configuration.c" "ui_configuration_nvs.c" "ui_platform_selftest.c" "ui_configuration_json.c" "ui_web.c"
         "ui_touch_map.c" "ui_touch_nvs.c"
    EMBED_FILES "webui.html"
    INCLUDE_DIRS "."
    PRIV_REQUIRES driver bt esp_app_format esp_driver_gpio esp_driver_ledc
                   esp_driver_spi esp_lcd esp_lcd_ili9341 esp_lvgl_port
                   esp_http_client esp_http_server json esp_netif esp_timer lvgl nvs_flash esp_wifi spi_flash
)

# The UI controller has an 8 KiB task stack. Reserve room for nested NVS/LVGL
# calls; reject the analytics regression's 4384/5840-byte local frames in every
# firmware build, including builds outside the optional bench scripts.
set_source_files_properties("display_app.c" PROPERTIES
    COMPILE_OPTIONS "-fstack-usage;-Werror=stack-usage=3072")

# Use one task budget for allocation and the post-compile check of nested paths.
set(MD_UI_TASK_STACK_BYTES 8192)
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    MD_UI_TASK_STACK_BYTES=${MD_UI_TASK_STACK_BYTES})
set_source_files_properties("ui_configuration.c" "ui_configuration_nvs.c" PROPERTIES
    COMPILE_OPTIONS "-fstack-usage")
add_custom_command(TARGET ${COMPONENT_LIB} POST_BUILD
    COMMAND ${PYTHON} ${CMAKE_CURRENT_LIST_DIR}/../../tools/check_ui_stack_budget.py
        --build-dir ${CMAKE_BINARY_DIR} --stack-bytes ${MD_UI_TASK_STACK_BYTES}
    VERBATIM)
