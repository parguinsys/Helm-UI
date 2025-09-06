// === I2C device policy (no-scan, INT-driven) — 2025-09-05 ===
#ifndef I2C_FULL_SCAN_ENABLE
#define I2C_FULL_SCAN_ENABLE 0
#endif
unsigned long i2c_lastRetryMs = 0;

// === end policy ===

// === DEBUG / UTIL FLAGS (2025-09-05) ===
#ifndef MIDI_MONITOR_ENABLE
#define MIDI_MONITOR_ENABLE 1
#endif
#ifndef I2C_PROBE_AT_BOOT
#define I2C_PROBE_AT_BOOT 0
#endif
#ifndef MIDI_EDGE_DEBUG
#define MIDI_EDGE_DEBUG 0
#endif
void MIDI_Init();
void MIDI_Poll();
void I2C_ProbeOnce();
void MIDI_EdgeDebug_Init();
void MIDI_EdgeDebug_Tick();
// === end flags ===

// === MIDI IN (6N137 → RX=GPIO37) monitor — added 2025-09-05 ===
#ifndef MIDI_MONITOR_ENABLE
#define MIDI_MONITOR_ENABLE 1
#endif
void MIDI_Init();
void MIDI_Poll();
// === end MIDI header ===

// === Forward decls (placed at file top) — 2025-09-05 ===
void HELM_ShowWaveGrid64();
void HELM_GridStyleNext();
void HELM_GridStylePrev();
// === End forward decls ===


// --- Feature toggles ---
#ifndef USE_HUB75
#define USE_HUB75 1
#endif

/*
 * Atlas UI Bus UI - Stable Encoders + Keys (INT + drain pattern)
 * Uses: U8g2, Adafruit_MCP23X17, Adafruit_TCA8418
 * Focus: match the exact interrupt/drain approach that worked for you.
 */
#include <Arduino.h>
#include "esp_log.h"  // quiet I2C driver logs
#include <Wire.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <Adafruit_MCP23X17.h>
#include <Adafruit_TCA8418.h>

#include "uibus_v1_pins.h"

// ---- Guaranteed definition to satisfy linker (INT-only build) ----
extern "C" void I2C_DeviceMgr_Tick(void);
void I2C_DeviceMgr_Tick(void) { /* no-scan/INT-only: nothing to do */ }
// ------------------------------------------------------------------

// --- Optional external reset for MCP23X17 (active-low). Wire this pin to MCP RESET via 10k pull-up to 3V3.
#ifndef PIN_MCP_RST
#define PIN_MCP_RST 8  // pick a free GPIO; 21 is usually safe on ESP32-S3 dev boards
#endif
// === ENGINE ONE externs (forward declarations) ===
extern bool engineOneActive;
void drawEngineOne();
void runEngineOneModal(); // blocks until BTN_CANCEL


// --- HUB75 FALLBACK PINS/CONFIG (safe to paste at top) -----------------------
#ifndef HUB75_PIN_R1
  // RGB data
  #define HUB75_PIN_R1   1
  #define HUB75_PIN_G1   2
  #define HUB75_PIN_B1   3
  #define HUB75_PIN_R2   15
  #define HUB75_PIN_G2   16
  #define HUB75_PIN_B2   17

  // Row address
  #define HUB75_PIN_A    18
  #define HUB75_PIN_B    21
  #define HUB75_PIN_C    41
  #define HUB75_PIN_D    42
  #define HUB75_PIN_E    40

  // Control
  #define HUB75_PIN_CLK  9
  #define HUB75_PIN_LAT  47
  #define HUB75_PIN_OE   48   // active LOW (library handles this)

  // Panel geometry + chain + brightness
  #define HUB75_W        64
  #define HUB75_H        64
  #define HUB75_CHAIN    1
  #define HUB75_BRIGHTNESS 140
#endif
// ----------------------------------------------------------------------------

// ------------ Key Labels (edit to taste) ------------
// 8 rows (A..H), 10 cols (1..10). Defaults are "A1".. "H10".
// === Button label assignments (patched) ===
const char* KEY_LABELS[8][10] = {
  /* A */ { "ENC_E_PUSH", "ENC_F_PUSH", "ENC_G_PUSH", "ENC_H_PUSH", "A5", "A6", "A7", "A8", "A9", "A10" },
  /* B */ { "ENC_A_PUSH", "ENC_B_PUSH", "ENC_C_PUSH", "ENC_D_PUSH", "B5", "B6", "B7", "B8", "B9", "B10" },
  /* C */ { "C1", "C2", "BTN_ENGINE2", "BTN_ENGINE1", "BTN_LEFT", "C6", "C7", "C8", "C9", "C10" },
  /* D */ { "D1", "D2", "BTN_OSC2", "BTN_OSC1", "BTN_UP", "BTN_SUBOSC", "D7", "D8", "D9", "D10" },
  /* E */ { "E1", "E2", "BTN_AMP2", "BTN_FN2", "BTN_AMP1", "BTN_AMP3", "E7", "E8", "E9", "E10" },
  /* F */ { "F1", "F2", "BTN_GLOBAL_AMP", "BTN_FN1", "BTN_DOWN", "F6", "F7", "F8", "F9", "F10" },
  /* G */ { "G1", "G2", "G3", "G4", "BTN_ENTER", "BTN_CANCEL", "G7", "G8", "G9", "G10" },
  /* H */ { "H1", "H2", "BTN_FX", "BTN_SETTINGS", "BTN_RIGHT", "BTN_FILTER", "H7", "H8", "H9", "H10" }
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
  if (!(tcaOK)) return;

  if(!tcaOK) return;
  while (tca.available() > 0){
    int k = tca.getEvent();
    bool pressed = (k & 0x80); k &= 0x7F; k--; // 1..80 → 0..79
    uint8_t row = k / 10, col = k % 10;
    snprintf(lastKey, sizeof(lastKey), "%s %s", keyLabel(row,col), pressed? "dn":"up");
  
    // Label-based switch with MODAL latch for ENGINE ONE
    const char* __lbl = keyLabel(row, col);
    Serial.printf("[KEY] r=%u c=%u pressed=%d label=%s\r\n", row, col, (int)pressed, __lbl?__lbl:"(null)");
    if (pressed && __lbl) {
      if (strcmp(__lbl, "BTN_ENGINE1") == 0) {
        engineOneActive = true;
        HELM_ShowWaveGrid64();
        runEngineOneModal(); // blocks until CANCEL
      } else if (strcmp(__lbl, "BTN_CANCEL") == 0) {
        engineOneActive = false;
      }
    }
}
}


// Quietly probe one I2C address; returns 0 on ACK, nonzero on NACK/error.
static uint8_t i2cQuietProbe(uint8_t addr){
  esp_log_level_set("i2c", ESP_LOG_NONE);
  esp_log_level_set("i2c.master", ESP_LOG_NONE);
  Wire.beginTransmission(addr);
  uint8_t err = Wire.endTransmission();
  esp_log_level_set("i2c.master", ESP_LOG_WARN);
  esp_log_level_set("i2c", ESP_LOG_WARN);
  return err;
}
void i2cScanPrint(){
#if I2C_FULL_SCAN_ENABLE
  Serial.print("I2C:");
  esp_log_level_set("i2c", ESP_LOG_NONE);
  esp_log_level_set("i2c.master", ESP_LOG_NONE);
  for(uint8_t a=0x03;a<=0x77;a++){
    Wire.beginTransmission(a);
    uint8_t err = Wire.endTransmission();
    if(err==0){ Serial.printf(" 0x%02X", a); }
  }
  Serial.println();
  esp_log_level_set("i2c.master", ESP_LOG_WARN);
  esp_log_level_set("i2c", ESP_LOG_WARN);
#else
  Serial.println("I2C: (scan disabled)");
#endif
}



void setup(){
  MIDI_Init(); // ensure MIDI UART up

  // Presence check (one-shot) for I2C devices
  tcaOK = (i2cQuietProbe(I2C_ADDR_TCA8418) == 0);
  mcpOK = (i2cQuietProbe(I2C_ADDR_MCP23017) == 0);

  Serial.begin(115200);
  delay(20);
  Serial.println("\n[Atlas UI - Stable Encoders+Keys]");

  SPI.begin(PIN_OLED_SCLK, -1, PIN_OLED_MOSI, PIN_OLED_CS);
  u8g2.begin();
  u8g2.setBusClock(10000000UL);
  u8g2.setContrast(0x7F);

  UI_BUS_beginI2C(Wire, 400000UL); // 100 kHz for robustness
  delay(500); // cold-boot settle for I2C peripherals
  i2cScanPrint();

  // TCA with cold-boot retries
  tcaOK = false;
  for (int i=0;i<15 && !tcaOK; ++i) { tcaOK = tca.begin(I2C_ADDR_TCA8418, &Wire); if(!tcaOK) delay(100); }
  if(!tcaOK) Serial.println("TCA8418 not found (retrying)");
  if(!tcaOK){
    uint32_t t0 = millis();
    while ( i2cQuietProbe(I2C_ADDR_TCA8418)!=0 && (millis()-t0)<2000 ) delay(50);
    for (int i=0;i<5 && !tcaOK; ++i) { tcaOK = tca.begin(I2C_ADDR_TCA8418, &Wire); if(!tcaOK) delay(100); }
  }
  if(tcaOK){ tca.matrix(8,10); delay(120); tca.flush(); while(tca.available()>0) (void)tca.getEvent(); pinMode(PIN_INT_TCA8418, INPUT_PULLUP); }

  // MCP encoders with hardware RESET pulse + cold-boot retries
  pinMode(PIN_MCP_RST, OUTPUT);
  digitalWrite(PIN_MCP_RST, HIGH); // keep high normally
  delay(2);
  digitalWrite(PIN_MCP_RST, LOW);  // assert reset
  delay(3);
  digitalWrite(PIN_MCP_RST, HIGH); // release reset
  delay(5);
  mcpOK = false;
  for (int i=0;i<15 && !mcpOK; ++i) { mcpOK = mcp.begin_I2C(I2C_ADDR_MCP23017, &Wire); if(!mcpOK) delay(100); }
  if(!mcpOK) Serial.println("MCP23017 not found (retrying)");
  if(!mcpOK){
    uint32_t t1 = millis();
    while ( i2cQuietProbe(I2C_ADDR_MCP23017)!=0 && (millis()-t1)<2000 ) delay(50);
    for (int i=0;i<5 && !mcpOK; ++i) { mcpOK = mcp.begin_I2C(I2C_ADDR_MCP23017, &Wire); if(!mcpOK) delay(100); }
  }
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
#if USE_HUB75
  hub75_begin();
#endif

  // --- Add-ons (2025-09-05) ---
  // Hold MCP23017 /RESET high on GPIO8 to avoid I2C NACKs if floating
  pinMode(8, OUTPUT);
  digitalWrite(8, HIGH);
  // Reduce verbose I2C logs to WARN (missing devices won't spam USB)
  esp_log_level_set("i2c", ESP_LOG_WARN);
  // Start MIDI monitor (RX=37)
  MIDI_Init();
  // --- End add-ons ---

  // --- Add-ons (2025-09-05) ---
  // Keep MCP23017 /RESET (GPIO8) asserted HIGH
  pinMode(8, OUTPUT);
  digitalWrite(8, HIGH);
  // Silence ESP-IDF I2C driver logs entirely for tag "i2c"
  esp_log_level_set("i2c", ESP_LOG_NONE);
  // Optional: show which I2C addresses ACK at boot
  #if I2C_PROBE_AT_BOOT
    I2C_ProbeOnce();
  #endif
  // Start MIDI UART on RX=37
  MIDI_Init();
  // Optional: edge counter ISR on GPIO37 to prove signal activity
  #if MIDI_EDGE_DEBUG
    MIDI_EdgeDebug_Init();
  #endif
  // --- End add-ons ---
}

void loop(){
  MIDI_Poll(); // MIDI hex echo

  I2C_DeviceMgr_Tick();

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
  if (millis() - lastUI >= 50){ lastUI = millis(); drawDiag(lastKey, encCount);
#if USE_HUB75
    hub75_tick();
#endif
  }
}


// ================= HUB75 Demo (MatrixPanel_I2S_DMA v3.0.x) =================
#if USE_HUB75

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

// === Forward decls for grid style controls (added 2025-09-05) ===
void HELM_ShowWaveGrid64();
// === End forward decls ===


static MatrixPanel_I2S_DMA* hub75 = nullptr;

static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b){
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

bool hub75_begin(){
  HUB75_I2S_CFG::i2s_pins pins = {
    HUB75_PIN_R1, HUB75_PIN_G1, HUB75_PIN_B1,
    HUB75_PIN_R2, HUB75_PIN_G2, HUB75_PIN_B2,
    HUB75_PIN_A, HUB75_PIN_B, HUB75_PIN_C, HUB75_PIN_D, HUB75_PIN_E,
    HUB75_PIN_LAT, HUB75_PIN_OE, HUB75_PIN_CLK
  };
  HUB75_I2S_CFG cfg(HUB75_W, HUB75_H, HUB75_CHAIN, pins);
  hub75 = new MatrixPanel_I2S_DMA(cfg);
  if (!hub75->begin()) { Serial.println(F("[HUB75] begin() failed")); return false; }
  hub75->setBrightness8(HUB75_BRIGHTNESS);
  hub75->fillScreen(0);
  Serial.println(F("[HUB75] online"));
  return true;
}

// simple color wheel animation
void hub75_tick(){
  static uint16_t hue = 0;
  if (!hub75) return;
  // Draw three vertical bands as a quick confidence demo
  int w = HUB75_W/3;
  uint8_t r = (hue     ) & 0xFF;
  uint8_t g = (hue + 85) & 0xFF;
  uint8_t b = (hue +170) & 0xFF;
  hub75->fillRect(0,     0, w, HUB75_H, rgb565(r,0,0));
  hub75->fillRect(w,     0, w, HUB75_H, rgb565(0,g,0));
  hub75->fillRect(w*2,   0, w, HUB75_H, rgb565(0,0,b));
  hue+=2;
}
#endif // USE_HUB75

// === ENGINE ONE modal latch implementation ===
bool engineOneActive = false;

void drawEngineOne(){
  // uses your existing u8g2 instance
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_8x13B_tf);
  u8g2.drawStr(2, 14, "ENGINE ONE");
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(2, 30, "Placeholder screen");
  u8g2.drawStr(2, 46, "Press CANCEL to return");
  u8g2.sendBuffer();
}

// Block here, repeatedly drawing, until BTN_CANCEL press is seen
void runEngineOneModal(){
  // small debounce guard
  unsigned long lastDraw = 0;
  while (engineOneActive) {
    unsigned long now = millis();
    if (now - lastDraw >= 16) { // ~60 FPS max
      drawEngineOne();
      lastDraw = now;
    }
    // Check for keys and specifically BTN_CANCEL
    while (tca.available() > 0) {
      int k = tca.getEvent();
      bool pressed = (k & 0x80);
      k &= 0x7F; k--;
      uint8_t row = k/10, col = k%10;
      const char* lbl = keyLabel(row, col);
      
      if (pressed && lbl) {
        if (strcmp(lbl, "BTN_LEFT") == 0) {
          HELM_GridStylePrev();
          HELM_ShowWaveGrid64();
        } else if (strcmp(lbl, "BTN_RIGHT") == 0) {
          HELM_GridStyleNext();
          HELM_ShowWaveGrid64();
        } else if (strcmp(lbl, "BTN_CANCEL") == 0) {
          engineOneActive = false;
          break;
        }
      }
    }
    delay(1); // yield
  }
}




/*** --- BEGIN: HELM_ShowWaveGrid64 Multi-Style Add-On (2025-09-05) --- ***
    Purpose: Provide 6 grid variants and allow cycling via BTN_LEFT/BTN_RIGHT
             while inside Engine One modal.
    Integration:
      - HELM_ShowWaveGrid64() draws the current style.
      - HELM_GridStyleNext()/Prev() change the style (wraparound).
    Notes:
      - Uses existing global `hub75` (MatrixPanel_I2S_DMA*), defined earlier.
      - RGB565 constants are used; no extra includes required.
*** ----------------------------------------------------------------- ***/

extern MatrixPanel_I2S_DMA* hub75;

static int8_t g_gridStyle = 0;
static const int8_t GRID_STYLE_COUNT = 6;

// Helpers to bump style
inline void HELM_GridStyleNext() { g_gridStyle = (g_gridStyle + 1 + GRID_STYLE_COUNT) % GRID_STYLE_COUNT; }
inline void HELM_GridStylePrev() { g_gridStyle = (g_gridStyle - 1 + GRID_STYLE_COUNT) % GRID_STYLE_COUNT; }

// Common colors
#define C_GRAY   0xC618  // light gray
#define C_WHITE  0xFFFF  // white
#define C_DARK   0x8410  // darker gray

// Utility: safe H/V "thick line" using fillRect to avoid single-pixel moire
static inline void HlineThick(int x, int y, int w, int t, uint16_t c){
  if (!hub75) return;
  if (w <= 0 || t <= 0) return;
  if (y < 0) y = 0;
  if (y > 63) y = 63;
  if (x < 0) x = 0;
  if (x + w > 64) w = 64 - x;
  if (t == 1) { hub75->drawFastHLine(x, y, w, c); return; }
  int y0 = y - (t/2);
  if (y0 < 0) y0 = 0;
  int h = t;
  if (y0 + h > 64) h = 64 - y0;
  hub75->fillRect(x, y0, w, h, c);
}
static inline void VlineThick(int x, int y, int h, int t, uint16_t c){
  if (!hub75) return;
  if (h <= 0 || t <= 0) return;
  if (x < 0) x = 0;
  if (x > 63) x = 63;
  if (y < 0) y = 0;
  if (y + h > 64) h = 64 - y;
  if (t == 1) { hub75->drawFastVLine(x, y, h, c); return; }
  int x0 = x - (t/2);
  if (x0 < 0) x0 = 0;
  int w = t;
  if (x0 + w > 64) w = 64 - x0;
  hub75->fillRect(x0, y, w, h, c);
}

// ---- Style 0: Majors only (16 px), thick 3 px
static inline void HELM_DrawGrid_Style0() {
  if (!hub75) return;
  hub75->clearScreen();
  for (int x=16; x<64; x+=16) VlineThick(x, 0, 64, 3, C_WHITE);
  for (int y=16; y<64; y+=16) HlineThick(0, y, 64, 3, C_WHITE);
}

// ---- Style 1: 8 px minors (1 px dim) + 16 px majors (2 px)
static inline void HELM_DrawGrid_Style1() {
  if (!hub75) return;
  hub75->clearScreen();
  for (int x=8; x<64; x+=8) { if (x % 16) VlineThick(x, 0, 64, 1, C_DARK); else VlineThick(x, 0, 64, 2, C_WHITE); }
  for (int y=8; y<64; y+=8) { if (y % 16) HlineThick(0, y, 64, 1, C_DARK); else HlineThick(0, y, 64, 2, C_WHITE); }
}

// ---- Style 2: Majors + 8 px dotted lattice
static inline void HELM_DrawGrid_Style2() {
  if (!hub75) return;
  hub75->clearScreen();
  for (int x=0; x<64; x+=16) VlineThick(x, 0, 64, 2, C_WHITE);
  for (int y=0; y<64; y+=16) HlineThick(0, y, 64, 2, C_WHITE);
  for (int x=8; x<64; x+=8){
    for (int y=8; y<64; y+=8){
      if ((x%16) && (y%16)) hub75->drawPixel(x, y, C_DARK);
    }
  }
}

// ---- Style 3: Sparse minor dots along majors + majors
static inline void HELM_DrawGrid_Style3() {
  if (!hub75) return;
  hub75->clearScreen();
  for (int x=16; x<64; x+=16){ for (int y=8; y<64; y+=8){ if (y%16) hub75->drawPixel(x, y, C_DARK); } }
  for (int y=16; y<64; y+=16){ for (int x=8; x<64; x+=8){ if (x%16) hub75->drawPixel(x, y, C_DARK); } }
  for (int x=16; x<64; x+=16) VlineThick(x, 0, 64, 2, C_WHITE);
  for (int y=16; y<64; y+=16) HlineThick(0, y, 64, 2, C_WHITE);
}

// ---- Style 4: Cell corner & midpoint dots + majors
static inline void HELM_DrawGrid_Style4() {
  if (!hub75) return;
  hub75->clearScreen();
  for (int x=0; x<64; x+=16){ for (int y=0; y<64; y+=16){ hub75->drawPixel(x, y, C_GRAY); } }
  for (int x=8; x<64; x+=16){ for (int y=0; y<64; y+=16){ hub75->drawPixel(x, y, C_DARK); } }
  for (int y=8; y<64; y+=16){ for (int x=0; x<64; x+=16){ hub75->drawPixel(x, y, C_DARK); } }
  for (int x=16; x<64; x+=16) VlineThick(x, 0, 64, 2, C_WHITE);
  for (int y=16; y<64; y+=16) HlineThick(0, y, 64, 2, C_WHITE);
}

// ---- Style 5: Center cross (3 px) + soft 16 px guides (1 px)
static inline void HELM_DrawGrid_Style5() {
  if (!hub75) return;
  hub75->clearScreen();
  VlineThick(32, 0, 64, 3, C_WHITE);
  HlineThick(0, 32, 64, 3, C_WHITE);
  for (int x=16; x<64; x+=16){ if (x!=32) VlineThick(x, 0, 64, 1, C_DARK); }
  for (int y=16; y<64; y+=16){ if (y!=32) HlineThick(0, y, 64, 1, C_DARK); }
}

// Dispatch
static inline void HELM_DrawWaveGrid64__internal() {
  switch (g_gridStyle) {
    case 0: HELM_DrawGrid_Style0(); break;
    case 1: HELM_DrawGrid_Style1(); break;
    case 2: HELM_DrawGrid_Style2(); break;
    case 3: HELM_DrawGrid_Style3(); break;
    case 4: HELM_DrawGrid_Style4(); break;
    case 5: HELM_DrawGrid_Style5(); break;
    default: HELM_DrawGrid_Style0(); break;
  }
}

// Public API
void HELM_ShowWaveGrid64() { HELM_DrawWaveGrid64__internal(); }

/*** --- END: HELM_ShowWaveGrid64 Multi-Style Add-On --- ***/




/*** --- BEGIN: MIDI IN monitor (RX=GPIO37) 2025-09-05 --- ***
  Minimal helper to confirm opto-isolated MIDI IN wiring.
  - Initializes UART @31250 on RX=37 (TX disabled)
  - Echoes bytes to USB Serial as HEX when MIDI_MONITOR_ENABLE==1
*** ----------------------------------------------------------------- ***/

#ifndef MIDI_MONITOR_ENABLE
#define MIDI_MONITOR_ENABLE 1
#endif

void MIDI_Init() {
  #if MIDI_MONITOR_ENABLE
  if (!Serial) { Serial.begin(115200); delay(10); }
  #endif
  Serial1.begin(31250, SERIAL_8N1, /*RX*/37, /*TX*/-1);
}

void MIDI_Poll() {
  #if MIDI_MONITOR_ENABLE
  while (Serial1.available() > 0) {
    uint8_t b = Serial1.read();
    static const char HEX_DIGITS[] = "0123456789ABCDEF";
    char out[4];
    out[0] = HEX_DIGITS[(b >> 4) & 0xF];
    out[1] = HEX_DIGITS[b & 0xF];
    out[2] = ' ';
    out[3] = '\0';
    Serial.print(out);
  }
  #endif
}

/*** --- END: MIDI IN monitor --- ***/



/*** --- BEGIN: I2C probe once (addresses) 2025-09-05 --- ***/
#ifndef I2C_PROBE_AT_BOOT
#define I2C_PROBE_AT_BOOT 0
#endif
void I2C_ProbeOnce() {
  #if I2C_PROBE_AT_BOOT
  if (!Serial) { Serial.begin(115200); delay(10); }
  Serial.println("\n[I2C] Probing known addresses...");
  const uint8_t addrs[] = { 0x20, 0x21, 0x22, 0x34, 0x3C, 0x3D };
  for (size_t i=0; i<sizeof(addrs); ++i){
    Wire.beginTransmission(addrs[i]);
    uint8_t err = Wire.endTransmission();
    Serial.print("  0x"); Serial.print(addrs[i], HEX); Serial.print(" : ");
    if (err == 0) Serial.println("ACK");
    else Serial.println("NACK");
  }
  #endif
}
/*** --- END: I2C probe once --- ***/



/*** --- BEGIN: MIDI edge debug (GPIO37 ISR) 2025-09-05 --- ***/
#ifndef MIDI_EDGE_DEBUG
#define MIDI_EDGE_DEBUG 0
#endif
#if MIDI_EDGE_DEBUG
volatile uint32_t g_midi_edge_count = 0;
volatile int g_last_level = -1;
void IRAM_ATTR _midi_edge_isr() { g_midi_edge_count++; }
uint32_t g_last_report_ms = 0;
void MIDI_EdgeDebug_Init(){
  pinMode(37, INPUT); // external pull-up to 3.3V already present via 4.7k
  attachInterrupt(37, _midi_edge_isr, CHANGE);
}
void MIDI_EdgeDebug_Tick(){
  uint32_t now = millis();
  if (now - g_last_report_ms >= 500){
    g_last_report_ms = now;
    if (!Serial) { Serial.begin(115200); delay(10); }
    Serial.print("\n[MIDI] edges/0.5s=");
    Serial.print(g_midi_edge_count);
    g_midi_edge_count = 0;
  }
}
#endif
/*** --- END: MIDI edge debug --- ***/
