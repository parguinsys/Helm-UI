\
#include "Display_SSD1322.h"
#include <U8g2lib.h>
#include <SPI.h>
#include "pins.h"

static U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI u8g2(
  U8G2_R0, /*cs=*/OLED_CS, /*dc=*/OLED_DC, /*reset=*/OLED_RST);

bool Display_SSD1322::begin() {
  SPI.begin(OLED_SCLK, -1 /*MISO unused*/, OLED_MOSI, OLED_CS);
  u8g2.begin();
  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.setBusClock(16000000UL);
  u8g2.clearBuffer();
  _line = 0;
  return true;
}

void Display_SSD1322::clear() { u8g2.clearBuffer(); _line = 0; }
void Display_SSD1322::startPage() { u8g2.clearBuffer(); _line = 0; }

void Display_SSD1322::printCenter(const char* line) {
  startPage();
  int w = u8g2.getStrWidth(line);
  int x = (256 - w) / 2;
  int y = 16 + _line * 14;
  u8g2.drawStr(x, y, line);
  _line++;
}

void Display_SSD1322::printLine(const char* line) {
  int y = 16 + _line * 14;
  u8g2.drawStr(4, y, line);
  _line++;
}

void Display_SSD1322::status(const char* msg) {
  u8g2.setDrawColor(0);
  u8g2.drawBox(0, 52, 256, 12);
  u8g2.setDrawColor(1);
  u8g2.drawStr(4, 62, msg);
}

void Display_SSD1322::present() { u8g2.sendBuffer(); }
