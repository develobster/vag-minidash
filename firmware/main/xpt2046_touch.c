#include "xpt2046_touch.h"

#include "driver/gpio.h"
#include "sdkconfig.h"

#if CONFIG_MINIDASH_TOUCH_SEPARATE_SPI
#define TOUCH_HOST SPI3_HOST
#else
#define TOUCH_HOST SPI2_HOST
#endif
#define XPT2046_CLOCK_HZ 1000000
#define XPT2046_PRESSURE_THRESHOLD 180

static bool read_channel(spi_device_handle_t spi, uint8_t command, uint16_t *value)
{
    uint8_t tx[3] = {command, 0, 0};
    uint8_t rx[3] = {0};
    spi_transaction_t transaction = {
        .length = 24,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    if (spi_device_transmit(spi, &transaction) != ESP_OK) return false;
    *value = (uint16_t)((((uint16_t)rx[1] << 8) | rx[2]) >> 3) & 0x0fff;
    return true;
}

bool md_xpt2046_open(md_xpt2046_t *touch)
{
    if (!touch) return false;
    *touch = (md_xpt2046_t){0};
#if CONFIG_MINIDASH_TOUCH_ENABLE
    const int cs = CONFIG_MINIDASH_TOUCH_PIN_CS;
    if (cs < 0 || !GPIO_IS_VALID_OUTPUT_GPIO(cs) ||
#if CONFIG_MINIDASH_TOUCH_SEPARATE_SPI
        !GPIO_IS_VALID_GPIO(CONFIG_MINIDASH_TOUCH_PIN_MISO)) return false;
    const spi_bus_config_t bus = {
        .sclk_io_num=CONFIG_MINIDASH_TOUCH_PIN_SCLK,
        .mosi_io_num=CONFIG_MINIDASH_TOUCH_PIN_MOSI,
        .miso_io_num=CONFIG_MINIDASH_TOUCH_PIN_MISO,
        .quadwp_io_num=-1, .quadhd_io_num=-1, .max_transfer_sz=8,
    };
    if (!GPIO_IS_VALID_OUTPUT_GPIO(bus.sclk_io_num) ||
        !GPIO_IS_VALID_OUTPUT_GPIO(bus.mosi_io_num) ||
        spi_bus_initialize(TOUCH_HOST,&bus,SPI_DMA_DISABLED)!=ESP_OK) return false;
#else
        !GPIO_IS_VALID_GPIO(CONFIG_MINIDASH_LCD_PIN_MISO)) return false;
#endif

    const spi_device_interface_config_t config = {
        .clock_speed_hz = XPT2046_CLOCK_HZ,
        .mode = 0,
        .spics_io_num = cs,
        .queue_size = 1,
    };
    if (spi_bus_add_device(TOUCH_HOST, &config, &touch->spi) != ESP_OK) return false;
    touch->ready = true;
    return true;
#else
    return false;
#endif
}

bool md_xpt2046_read(md_xpt2046_t *touch, md_touch_sample_t *sample)
{
    if (!touch || !touch->ready || !sample) return false;
    if (!read_channel(touch->spi, 0xb0, &sample->z1) ||
        !read_channel(touch->spi, 0xc0, &sample->z2) ||
        !read_channel(touch->spi, 0xd0, &sample->x) ||
        !read_channel(touch->spi, 0x90, &sample->y)) return false;

    const uint32_t pressure = (uint32_t)sample->z1 + (4095U - sample->z2);
    sample->pressure = pressure > UINT16_MAX ? UINT16_MAX : (uint16_t)pressure;
    sample->pressed = sample->z1 > 32 && sample->z1 < 4000 &&
                      sample->z2 > sample->z1 &&
                      sample->pressure >= XPT2046_PRESSURE_THRESHOLD;
    return true;
}
