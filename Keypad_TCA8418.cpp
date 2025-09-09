\
#include "Keypad_TCA8418.h"
#include "pins.h"
#include <Adafruit_TCA8418.h>

static Adafruit_TCA8418 TCA;
volatile bool Keypad_TCA8418::s_irqFlag = false;
static void tca8418_isr() { Keypad_TCA8418::s_irqFlag = true; }

bool Keypad_TCA8418::begin(TwoWire& w, int intPin) {
  _w = &w;
  _ok = TCA.begin(TCA8418_ADDR, _w);
  if (_ok) {
    TCA.matrix(8, 10);
    TCA.flush();
    TCA.enableInterrupts();
    if (intPin >= 0) {
      pinMode(intPin, INPUT_PULLUP);
      attachInterrupt(digitalPinToInterrupt(intPin), tca8418_isr, FALLING);
    }
  }
  return _ok;
}

int Keypad_TCA8418::_tcaGetEvent() { return TCA.getEvent(); }

Keypad_TCA8418::Ev Keypad_TCA8418::_decode(int k) {
  Ev out{false,0,0,false};
  if (k <= 0) return out;
  bool pressed = (k & 0x80);
  k &= 0x7F;
  k -= 1;
  uint8_t row = k / 10;
  uint8_t col = k % 10;
  out.isKey = true; out.row = row; out.col = col; out.down = pressed;
  return out;
}
