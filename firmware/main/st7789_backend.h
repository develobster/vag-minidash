#pragma once
#include "display_manager.h"

/* GPIO values come only from sdkconfig/menuconfig; defaults drive no pins. */
md_display_backend_t *md_st7789_backend(void);
