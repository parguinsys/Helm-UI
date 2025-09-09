\
#pragma once
#include <Arduino.h>

class Display_SSD1322 {
public:
  bool begin();
  void clear();
  void printCenter(const char* line);
  void printLine(const char* line);
  void status(const char* msg); // updates bottom line with msg
  void present();                // flush once per frame

private:
  void startPage();
  uint8_t _line = 0;
};
