#pragma once
/**
 * UI Bus v1 Pin Map (stable set)
 * Target: ESP32-S3 UI Baseboard
 *
 * OLED (SSD1322, SPI 4-wire): SCLK=GPIO10, MOSI=GPIO11, CS=GPIO12, DC=GPIO13, RST=GPIO14
 * I2C shared bus: SDA=GPIO38, SCL=GPIO39
 * TCA8418 keypad: I2C addr 0x34, INT on GPIO7 (open-drain)
 * MCP23017 encoders: I2C addr 0x20, INT (mirrored) on GPIO5 (open-drain)
 */

#include <stdint.h>

// ---------------- OLED (SSD1322) ----------------
static constexpr int PIN_OLED_SCLK = 10;
static constexpr int PIN_OLED_MOSI = 11; // D1 / SDA on panel
static constexpr int PIN_OLED_CS   = 12;
static constexpr int PIN_OLED_DC   = 13; // A0
static constexpr int PIN_OLED_RST  = 14;

// ---------------- I2C (shared) ------------------
static constexpr int PIN_I2C_SDA = 38;
static constexpr int PIN_I2C_SCL = 39;

static constexpr uint8_t I2C_ADDR_TCA8418  = 0x34;
static constexpr uint8_t I2C_ADDR_MCP23017 = 0x20;

static constexpr int PIN_INT_TCA8418  = 7;
static constexpr int PIN_INT_MCP23017 = 5;

// MCP helpers (0..7 = GPA0..7, 8..15 = GPB0..7)
#define MCP_GPA(n)  ((n) & 0x07)
#define MCP_GPB(n)  (8 + ((n) & 0x07))

/*
 * Logical indices 0..7 correspond to panel letters A..H.
 * Physical-to-MCP bits (per your board):
 *   phys0: A=GPA0(0)  B=GPA1(1)
 *   phys1: A=GPA2(2)  B=GPA3(3)
 *   phys2: A=GPA4(4)  B=GPA5(5)
 *   phys3: A=GPA6(6)  B=GPA7(7)
 *   phys4: A=GPB0(8)  B=GPB1(9)
 *   phys5: A=GPB2(10) B=GPB3(11)
 *   phys6: A=GPB4(12) B=GPB5(13)
 *   phys7: A=GPB6(14) B=GPB7(15)
 *
 * Map phys → logical (A..H): 7→A, 6→B, 5→C, 4→D, 0→E, 1→F, 2→G, 3→H
 */
// A-channel pins per logical encoder index (0..7 = A..H)
static constexpr uint8_t ENC_A_MCP_PINS[8] = {
  /*0:A*/  MCP_GPB(6),  // phys7
  /*1:B*/  MCP_GPB(4),  // phys6
  /*2:C*/  MCP_GPB(2),  // phys5
  /*3:D*/  MCP_GPB(0),  // phys4
  /*4:E*/  MCP_GPA(0),  // phys0
  /*5:F*/  MCP_GPA(2),  // phys1
  /*6:G*/  MCP_GPA(4),  // phys2
  /*7:H*/  MCP_GPA(6)   // phys3
};

// B-channel pins per logical encoder index (0..7 = A..H)
static constexpr uint8_t ENC_B_MCP_PINS[8] = {
  /*0:A*/  MCP_GPB(7),  // phys7
  /*1:B*/  MCP_GPB(5),  // phys6
  /*2:C*/  MCP_GPB(3),  // phys5
  /*3:D*/  MCP_GPB(1),  // phys4
  /*4:E*/  MCP_GPA(1),  // phys0
  /*5:F*/  MCP_GPA(3),  // phys1
  /*6:G*/  MCP_GPA(5),  // phys2
  /*7:H*/  MCP_GPA(7)   // phys3
};

// Direction: invert G (index 6), others forward
static constexpr int8_t ENC_DIR[8] = { +1, +1, +1, +1, +1, +1, +1, +1 };

#ifdef ARDUINO
  #include <Arduino.h>
  #include <Wire.h>
  inline void UI_BUS_beginI2C(TwoWire &w = Wire, uint32_t freq = 400000UL) {
    w.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    w.setClock(freq);
  }
  inline bool UI_BUS_probeI2C(uint8_t addr, TwoWire &w = Wire) {
    w.beginTransmission(addr);
    return (w.endTransmission() == 0);
  }
#endif
