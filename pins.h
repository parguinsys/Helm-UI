\
#pragma once
// UI bus baseline (2025-09-01)
#define UI_I2C_SDA   38
#define UI_I2C_SCL   39

// SSD1322 OLED (SPI pins)
#define OLED_SCLK    10
#define OLED_MOSI    11
#define OLED_CS      12
#define OLED_DC      13
#define OLED_RST     14

// TCA8418 keypad
#define TCA8418_ADDR 0x34
#define TCA8418_INT  7     // your wiring: INT → GPIO7
