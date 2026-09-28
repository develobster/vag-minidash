#include "ili9341_backend.h"

#include <string.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_ili9341.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"

#define LCD_HOST SPI2_HOST
#define LINES_PER_TRANSFER 16

typedef struct {
    esp_lcd_panel_handle_t panel;
    esp_lcd_panel_io_handle_t io;
    SemaphoreHandle_t transfer_done;
    uint16_t *line_buffer;
    uint16_t width;
    uint16_t height;
} ili9341_context_t;

static ili9341_context_t context;

static bool on_color_transfer_done(esp_lcd_panel_io_handle_t io,
                                   esp_lcd_panel_io_event_data_t *event,
                                   void *user_context)
{
    (void)io;
    (void)event;
    BaseType_t higher_priority_task_woken = pdFALSE;
    xSemaphoreGiveFromISR(((ili9341_context_t *)user_context)->transfer_done,
                          &higher_priority_task_woken);
    return higher_priority_task_woken == pdTRUE;
}

static bool pins_are_valid(void)
{
    const int pins[] = {
        CONFIG_MINIDASH_LCD_PIN_SCLK,
        CONFIG_MINIDASH_LCD_PIN_MOSI,
        CONFIG_MINIDASH_LCD_PIN_CS,
        CONFIG_MINIDASH_LCD_PIN_DC,
        CONFIG_MINIDASH_LCD_PIN_RST,
        CONFIG_MINIDASH_LCD_PIN_BL,
    };

    for (unsigned i = 0; i < sizeof(pins) / sizeof(pins[0]); ++i) {
        if (i < 4 && pins[i] < 0) {
            return false;
        }
        if (pins[i] >= 0 && !GPIO_IS_VALID_OUTPUT_GPIO(pins[i])) {
            return false;
        }
        for (unsigned j = 0; pins[i] >= 0 && j < i; ++j) {
            if (pins[i] == pins[j]) {
                return false;
            }
        }
    }
    return true;
}

static bool open_panel(md_display_backend_t *backend,
                       const md_display_profile_t *profile)
{
    (void)backend;
    if (profile->driver != MD_DRIVER_ILI9341 || profile->color_depth != 16 ||
        !pins_are_valid()) {
        return false;
    }

    context.width = profile->width;
    context.height = profile->height;
    context.transfer_done = xSemaphoreCreateBinary();
    context.line_buffer = heap_caps_malloc(
        (size_t)profile->width * LINES_PER_TRANSFER * sizeof(uint16_t),
        MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    if (!context.transfer_done || !context.line_buffer) {
        return false;
    }

    const spi_bus_config_t bus_config = {
        .sclk_io_num = CONFIG_MINIDASH_LCD_PIN_SCLK,
        .mosi_io_num = CONFIG_MINIDASH_LCD_PIN_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = profile->width * LINES_PER_TRANSFER * 2 + 8,
    };
    if (spi_bus_initialize(LCD_HOST, &bus_config, SPI_DMA_CH_AUTO) != ESP_OK) {
        return false;
    }

    const esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = CONFIG_MINIDASH_LCD_PIN_DC,
        .cs_gpio_num = CONFIG_MINIDASH_LCD_PIN_CS,
        .pclk_hz = 10 * 1000 * 1000,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 1,
    };
    if (esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST,
                                 &io_config, &context.io) != ESP_OK) {
        return false;
    }

    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = CONFIG_MINIDASH_LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    if (esp_lcd_new_panel_ili9341(context.io, &panel_config,
                                  &context.panel) != ESP_OK) {
        return false;
    }

    const esp_lcd_panel_io_callbacks_t callbacks = {
        .on_color_trans_done = on_color_transfer_done,
    };
    if (esp_lcd_panel_io_register_event_callbacks(context.io, &callbacks,
                                                  &context) != ESP_OK) {
        return false;
    }

#if CONFIG_MINIDASH_LCD_PIN_BL >= 0
    const gpio_config_t backlight_config = {
        .pin_bit_mask = 1ULL << CONFIG_MINIDASH_LCD_PIN_BL,
        .mode = GPIO_MODE_OUTPUT,
    };
    if (!GPIO_IS_VALID_OUTPUT_GPIO(CONFIG_MINIDASH_LCD_PIN_BL) ||
        gpio_config(&backlight_config) != ESP_OK) {
        return false;
    }
    gpio_set_level(CONFIG_MINIDASH_LCD_PIN_BL, !CONFIG_MINIDASH_LCD_BL_ACTIVE_HIGH);
#endif

    if (esp_lcd_panel_reset(context.panel) != ESP_OK ||
        esp_lcd_panel_init(context.panel) != ESP_OK ||
        esp_lcd_panel_set_gap(context.panel, CONFIG_MINIDASH_LCD_X_GAP,
                              CONFIG_MINIDASH_LCD_Y_GAP) != ESP_OK) {
        return false;
    }
#ifdef CONFIG_MINIDASH_LCD_INVERT
    if (esp_lcd_panel_invert_color(context.panel, true) != ESP_OK) {
        return false;
    }
#endif
    if (esp_lcd_panel_swap_xy(context.panel, profile->rotation % 2 != 0) != ESP_OK ||
        esp_lcd_panel_mirror(context.panel, profile->rotation >= 2,
                             profile->rotation == 1 || profile->rotation == 2) != ESP_OK ||
        esp_lcd_panel_disp_on_off(context.panel, true) != ESP_OK) {
        return false;
    }

#if CONFIG_MINIDASH_LCD_PIN_BL >= 0
    gpio_set_level(CONFIG_MINIDASH_LCD_PIN_BL, CONFIG_MINIDASH_LCD_BL_ACTIVE_HIGH);
#endif
    return true;
}

static bool present_frame(md_display_backend_t *backend, const uint16_t *rgb565)
{
    (void)backend;
    for (int y = 0; y < context.height; y += LINES_PER_TRANSFER) {
        int lines = context.height - y;
        if (lines > LINES_PER_TRANSFER) {
            lines = LINES_PER_TRANSFER;
        }
        memcpy(context.line_buffer, rgb565 + (size_t)y * context.width,
               (size_t)lines * context.width * sizeof(uint16_t));
        if (esp_lcd_panel_draw_bitmap(context.panel, 0, y, context.width,
                                      y + lines, context.line_buffer) != ESP_OK ||
            xSemaphoreTake(context.transfer_done, pdMS_TO_TICKS(1000)) != pdTRUE) {
            return false;
        }
    }
    return true;
}

static void set_panel_power(md_display_backend_t *backend, bool on)
{
    (void)backend;
    if (!context.panel) {
        return;
    }
#if CONFIG_MINIDASH_LCD_PIN_BL >= 0
    if (!on) {
        gpio_set_level(CONFIG_MINIDASH_LCD_PIN_BL, !CONFIG_MINIDASH_LCD_BL_ACTIVE_HIGH);
    }
#endif
    esp_lcd_panel_disp_on_off(context.panel, on);
#if CONFIG_MINIDASH_LCD_PIN_BL >= 0
    if (on) {
        gpio_set_level(CONFIG_MINIDASH_LCD_PIN_BL, CONFIG_MINIDASH_LCD_BL_ACTIVE_HIGH);
    }
#endif
}

md_display_backend_t *md_ili9341_backend(void)
{
    static md_display_backend_t backend = {
        .context = &context,
        .open = open_panel,
        .present = present_frame,
        .power = set_panel_power,
    };
    return &backend;
}
