/*
 * Atlas UI Bus UI - Stable Encoders + Keys (INT + drain pattern)
 * Uses: U8g2, Adafruit_MCP23X17, Adafruit_TCA8418
 * Focus: match the exact interrupt/drain approach that worked for you.
 */
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <Adafruit_MCP23X17.h>
#include <Adafruit_TCA8418.h>

#include "uibus_v1_pins.h"

// ------------ Key Labels (edit to taste) ------------
// 8 rows (A..H), 10 cols (1..10). Defaults are "A1".. "H10".
const char* KEY_LABELS[8][10] = {
  {"A1","A2","A3","A4","A5","A6","A7","A8","A9","A10"},
  {"B1","B2","B3","B4","B5","B6","B7","B8","B9","B10"},
  {"C1","C2","C3","C4","C5","C6","C7","C8","C9","C10"},
  {"D1","D2","D3","D4","D5","D6","D7","D8","D9","D10"},
  {"E1","E2","E3","E4","E5","E6","E7","E8","E9","E10"},
  {"F1","F2","F3","F4","F5","F6","F7","F8","F9","F10"},
  {"G1","G2","G3","G4","G5","G6","G7","G8","G9","G10"},
  {"H1","H2","H3","H4","H5","H6","H7","H8","H9","H10"}
};
static inline const char* keyLabel(uint8_t row, uint8_t col){
  if (row<8 && col<10) return KEY_LABELS[row][col];
  return "-";
}


// ---------------- Display ----------------
U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI u8g2(U8G2_R0, PIN_OLED_CS, PIN_OLED_DC, PIN_OLED_RST);

static void drawBoot(uint8_t pct) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_helvB12_tf);
  u8g2.setDrawColor(15);
  u8g2.drawStr(6, 14, "ATLAS UI BUS");
  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.drawStr(6, 30, "encoders + keys (INT stable)");
  // progress bar
  int x=6, y=48, w=244, h=8;
  u8g2.drawFrame(x, y, w, h);
  int fill = (int)(pct * (w-2) / 100);
  u8g2.drawBox(x+1, y+1, fill, h-2);
  
  // footer hint
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(6, 63, "Keys show labels (e.g. A1..H10)");

  u8g2.sendBuffer();
}

static void drawDiag(const char* lastKey, const int32_t encCount[8]) {
  u8g2.clearBuffer();
  u8g2.setDrawColor(15);
  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.drawStr(6, 12, "DIAG  (A..H / last key)");

  // lastKey
  u8g2.drawStr(6, 26, "KEY:");
  u8g2.drawStr(36, 26, lastKey && lastKey[0] ? lastKey : "-");

  // Enc mini bars
  const int cellW=62, topY=42, botY=58, boxW=40, boxH=6, boxDx=20, scale=2;
  auto cell = [&](int i, int col, int y){
    char lbl[8]; sprintf(lbl, "%c:%ld", 'A'+i, (long)encCount[i]);
    int x = 6 + col * cellW;
    u8g2.drawStr(x, y-2, lbl);
    int v = encCount[i]; if (v < - (boxW/2)*scale) v = - (boxW/2)*scale; if (v > (boxW/2)*scale) v = (boxW/2)*scale;
    int cx = x + boxDx + boxW/2, cy = y+2;
    u8g2.drawFrame(cx - boxW/2, cy- boxH, boxW, boxH);
    int w = v/scale;
    if (w>=0) u8g2.drawBox(cx, cy- boxH+1, w, boxH-2);
    else     u8g2.drawBox(cx+w, cy- boxH+1, -w, boxH-2);
  };
  // rows
  for (int i=0;i<4;i++) cell(i+4, i, topY);
  for (int i=0;i<4;i++) cell(i,   i, botY);

  u8g2.sendBuffer();
}

// ---------------- State ----------------
enum Mode { BOOT, DIAG };
Mode mode = BOOT;
uint32_t tStartMS = 0;

Adafruit_TCA8418 tca;
Adafruit_MCP23X17 mcp;
bool tcaOK=false, mcpOK=false;

char lastKey[24] = "-";
int32_t encCount[8] = {0};
uint8_t prev2b[8]   = {0};

// qlut used earlier – keep it identical
static const int8_t qlut[16] = {
  0,-1,+1,0,  +1,0,0,-1,  -1,0,0,+1,  0,+1,-1,0
};

// INT flag + ISR
volatile bool mcpIntFlag = false;
IRAM_ATTR void onMcpInt(){ mcpIntFlag = true; }

inline uint16_t readGPIOAB_inverted(){
  // Read GPIO registers (clears MCP int), then invert to make 1 = active/low (with pullups on)
  return (uint16_t)(~mcp.readGPIOAB());
}

inline void processEncodersFromState(uint16_t ab_inv){
  for(int i=0;i<8;i++){
    int aBit = ENC_A_MCP_PINS[i];
    int bBit = ENC_B_MCP_PINS[i];
    uint8_t a = (ab_inv >> aBit) & 1;
    uint8_t b = (ab_inv >> bBit) & 1;
    uint8_t curr = (a<<1) | b;
    uint8_t prev = prev2b[i];
    uint8_t idx  = ((prev & 3) << 2) | (curr & 3);
    int8_t  d    = qlut[idx];
    if(d){ encCount[i] += (int32_t)d * ENC_DIR[i]; }
    prev2b[i] = curr;
  }
}

// Keys (TCA8418)
void pollTCA(){
  if(!tcaOK) return;
  while (tca.available() > 0){
    int k = tca.getEvent();
    bool pressed = (k & 0x80); k &= 0x7F; k--; // 1..80 → 0..79
    uint8_t row = k / 10, col = k % 10;
    snprintf(lastKey, sizeof(lastKey), "%s %s", keyLabel(row,col), pressed? "dn":"up");
  }
}

void i2cScanPrint(){
  Serial.print("I2C:");
  for(uint8_t a=0x03;a<=0x77;a++){
    Wire.beginTransmission(a);
    if(Wire.endTransmission()==0){ Serial.printf(" 0x%02X", a); }
  }
  Serial.println();
}

void setup(){
  Serial.begin(115200);
  delay(20);
  Serial.println("\n[Atlas UI - Stable Encoders+Keys]");

  SPI.begin(PIN_OLED_SCLK, -1, PIN_OLED_MOSI, PIN_OLED_CS);
  u8g2.begin();
  u8g2.setBusClock(10000000UL);
  u8g2.setContrast(0x7F);

  UI_BUS_beginI2C(Wire, 400000UL); // 100 kHz for robustness
  i2cScanPrint();

  // TCA
  tcaOK = tca.begin(I2C_ADDR_TCA8418, &Wire);
  if(!tcaOK) Serial.println("TCA8418 not found");
  else { tca.matrix(8,10); tca.flush(); pinMode(PIN_INT_TCA8418, INPUT_PULLUP); }

  // MCP encoders
  mcpOK = mcp.begin_I2C(I2C_ADDR_MCP23017, &Wire);
  if(!mcpOK) Serial.println("MCP23017 not found");
  else {
    for(int i=0;i<8;i++){
      mcp.pinMode(ENC_A_MCP_PINS[i], INPUT_PULLUP);
      mcp.pinMode(ENC_B_MCP_PINS[i], INPUT_PULLUP);
    }
    // mirror A/B to INTA, open-drain LOW-active (matches your wiring)
    mcp.setupInterrupts(true /*mirror*/, true /*openDrain*/, LOW /*polarity*/);
    for(int i=0;i<8;i++){
      mcp.setupInterruptPin(ENC_A_MCP_PINS[i], CHANGE);
      mcp.setupInterruptPin(ENC_B_MCP_PINS[i], CHANGE);
    }
    pinMode(PIN_INT_MCP23017, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_INT_MCP23017), onMcpInt, FALLING);

    // seed prev states
    uint16_t ab = readGPIOAB_inverted();
    for(int i=0;i<8;i++){
      uint8_t a = (ab >> ENC_A_MCP_PINS[i]) & 1;
      uint8_t b = (ab >> ENC_B_MCP_PINS[i]) & 1;
      prev2b[i] = (a<<1)|b;
    }
  }

  tStartMS = millis();
  drawBoot(0);
}

void loop(){
  if (mode == BOOT){
    uint32_t e = millis() - tStartMS;
    uint8_t pct = (e >= 1500) ? 100 : (uint8_t)(e * 100UL / 1500UL);
    drawBoot(pct);
    if (e >= 1500) mode = DIAG;
    delay(10);
    return;
  }

  // Drain MCP interrupts like the proven sketch: burst-read GPIOAB while INT is held low.
  if (mcpOK && (mcpIntFlag || digitalRead(PIN_INT_MCP23017) == LOW)){
    mcpIntFlag = false;
    for (int i=0;i<32;i++){            // up to N reads per burst
      uint16_t ab = readGPIOAB_inverted();
      processEncodersFromState(ab);
      if (digitalRead(PIN_INT_MCP23017) == HIGH) break;
      delayMicroseconds(10);
    }
  }

  // Fallback poll at ~1 kHz in case an edge is missed (extra robustness)
  static uint32_t lastScan=0; uint32_t us = micros();
  if (mcpOK && (us - lastScan) >= 1000){
    lastScan = us;
    uint16_t ab = readGPIOAB_inverted();
    processEncodersFromState(ab);
  }

  // Keys
  pollTCA();

  // UI refresh ~20 Hz
  static uint32_t lastUI=0;
  if (millis() - lastUI >= 50){ lastUI = millis(); drawDiag(lastKey, encCount); }
}
