// =====================================================================
//  display.h - Khoi tao panel HUB75 + cac ham ve co ban
// =====================================================================
#pragma once
#include <stdint.h>
#include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"

extern MatrixPanel_I2S_DMA *display;

void display_setup(void);
void display_set_brightness(uint8_t value);

// Day khung vua ve ra panel (hoan doi buffer phu <-> buffer dang quet)
static inline void present(void) { display->flipDMABuffer(); }
