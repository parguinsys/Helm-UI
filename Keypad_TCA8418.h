\
#pragma once
#include <Arduino.h>
#include <Wire.h>

class Keypad_TCA8418 {
public:
  bool begin(TwoWire& w, int intPin = -1);

  template<typename Fn>
  void poll(Fn onEvent) {
    if (!_ok) return;
    for (;;) {
      int k = _tcaGetEvent();
      if (k <= 0) break;
      auto ev = _decode(k);
      if (ev.isKey) onEvent(ev.row, ev.col, ev.down);
    }
    s_irqFlag = false;
  }

  static volatile bool s_irqFlag;

private:
  struct Ev { bool isKey; uint8_t row, col; bool down; };
  bool _ok = false;
  TwoWire* _w = nullptr;
  int  _tcaGetEvent();
  Ev   _decode(int k);
};
