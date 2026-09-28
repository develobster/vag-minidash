idf_component_register(
    SRCS "main.c" "display_model.c" "display_manager.c" "st7789_backend.c"
         "ili9341_backend.c" "xpt2046_touch.c" "display_app.c"
         "ui_core.c" "ui_core_selftest.c" "ui_theme.c" "ui_settings_nvs.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES bt esp_driver_gpio esp_driver_spi esp_lcd esp_lcd_ili9341
                   esp_netif esp_timer nvs_flash esp_wifi spi_flash
)
