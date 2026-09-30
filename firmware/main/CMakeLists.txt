idf_component_register(
    SRCS "main.c" "display_manager.c" "st7789_backend.c"
         "ili9341_backend.c" "xpt2046_touch.c" "display_app.c"
         "display_serial_protocol.c"
         "ui_core.c" "ui_core_selftest.c" "ui_theme.c" "ui_i18n.c" "ui_screens.c"
         "ui_font_ru_10.c" "ui_font_ru_12.c" "ui_font_ru_14.c" "ui_font_ru_16.c"
         "ui_settings_nvs.c" "ui_platform.c" "ui_model.c"
         "ui_configuration.c" "ui_configuration_nvs.c" "ui_platform_selftest.c" "ui_configuration_json.c" "ui_web.c"
         "ui_touch_map.c" "ui_touch_nvs.c"
    EMBED_FILES "webui.html"
    INCLUDE_DIRS "."
    PRIV_REQUIRES bt esp_app_format esp_driver_gpio esp_driver_ledc
                   esp_driver_spi esp_lcd esp_lcd_ili9341 esp_lvgl_port
                   esp_http_client esp_http_server json esp_netif esp_timer lvgl nvs_flash esp_wifi spi_flash
)
