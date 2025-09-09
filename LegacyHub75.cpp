\
#include "LegacyHub75.h"

#if USE_HUB75
static MatrixPanel_I2S_DMA* gMatrix = nullptr;

static uint16_t tick_x = 0;
static uint16_t tick_y = 0;
static bool     tick_dir = true;

bool hub75_begin() {
  if (gMatrix) return true;
  HUB75_I2S_CFG cfg(HUB75_WIDTH, HUB75_HEIGHT, HUB75_CHAIN_LENGTH);
  cfg.gpio.r1  = HUB75_PIN_R1;
  cfg.gpio.g1  = HUB75_PIN_G1;
  cfg.gpio.b1  = HUB75_PIN_B1;
  cfg.gpio.r2  = HUB75_PIN_R2;
  cfg.gpio.g2  = HUB75_PIN_G2;
  cfg.gpio.b2  = HUB75_PIN_B2;
  cfg.gpio.a   = HUB75_PIN_A;
  cfg.gpio.b   = HUB75_PIN_B;
  cfg.gpio.c   = HUB75_PIN_C;
  cfg.gpio.d   = HUB75_PIN_D;
  cfg.gpio.e   = HUB75_PIN_E;
  cfg.gpio.lat = HUB75_PIN_LAT;
  cfg.gpio.oe  = HUB75_PIN_OE;
  cfg.gpio.clk = HUB75_PIN_CLK;
  cfg.i2sspeed = HUB75_I2S_CFG::HZ_10M;
  gMatrix = new MatrixPanel_I2S_DMA(cfg);
  if (!gMatrix) return false;
  if (!gMatrix->begin()) return false;
  gMatrix->setBrightness8(HUB75_BRIGHTNESS);
  gMatrix->clearScreen();

  // Init-only checkerboard + header band
  for (int y = 0; y < HUB75_HEIGHT; ++y) {
    for (int x = 0; x < HUB75_WIDTH; ++x) {
      bool block = ((x / 8) + (y / 8)) & 1;
      gMatrix->drawPixel(x, y, block ? rgb565(255,255,255) : rgb565(0,0,0));
    }
  }
  for (int x = 0; x < HUB75_WIDTH; ++x) {
    gMatrix->drawPixel(x, 0, rgb565(0,255,0));
    if (HUB75_HEIGHT > 1) gMatrix->drawPixel(x, 1, rgb565(0,32,0));
  }
  return true;
}

void hub75_tick() {
  if (!gMatrix) return;
  // Erase previous pixel based on background pattern
  uint16_t bg = (((tick_x / 8) + (tick_y / 8)) & 1) ? rgb565(255,255,255) : rgb565(0,0,0);
  gMatrix->drawPixel(tick_x, tick_y, bg);

  // Move
  if (tick_dir) {
    if (++tick_x >= HUB75_WIDTH)  { tick_x = 0; tick_dir = !tick_dir; }
    if (++tick_y >= HUB75_HEIGHT) { tick_y = 0; }
  } else {
    if (tick_x == 0) { tick_x = HUB75_WIDTH - 1; tick_dir = !tick_dir; }
    else { --tick_x; }
    if (tick_y == 0) { tick_y = HUB75_HEIGHT - 1; }
    else { --tick_y; }
  }
  gMatrix->drawPixel(tick_x, tick_y, rgb565(255,0,0));
}
#else
bool hub75_begin() { return false; }
void hub75_tick() {}
#endif
