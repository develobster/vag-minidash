idf_component_register(
    SRCS "main.c" "display_model.c" "display_manager.c" "st7789_backend.c" "display_app.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES bt esp_driver_gpio esp_driver_spi esp_lcd esp_netif esp_wifi esp_timer spi_flash
)
