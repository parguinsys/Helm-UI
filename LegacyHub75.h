\
#pragma once
#include "HUB75Config.h"

#if USE_HUB75
  #include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#endif

// Free-function 8-bit RGB -> 565 packer
static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b){
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// Initialize HUB75; returns true if ready.
bool hub75_begin();

// ~30 Hz non-blocking animation hook
void hub75_tick();
