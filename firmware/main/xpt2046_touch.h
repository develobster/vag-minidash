#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "driver/spi_master.h"

typedef struct {
    spi_device_handle_t spi;
    bool ready;
} md_xpt2046_t;

typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t z1;
    uint16_t z2;
    uint16_t pressure;
    bool pressed;
} md_touch_sample_t;

bool md_xpt2046_open(md_xpt2046_t *touch);
bool md_xpt2046_read(md_xpt2046_t *touch, md_touch_sample_t *sample);
