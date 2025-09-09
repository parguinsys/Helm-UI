// Carto_Minimal_v11.3a_hotkeys — adds label→page hotkeys (non-breaking)
// OLED (SSD1322 via U8g2 HW SPI) + TCA8418 keypad (I2C, INT=7).

#include "HUB75Config.h"
#include "LegacyHub75.h"
#include <Arduino.h>
#include <Wire.h>
#include "pins.h"
#include "Display_SSD1322.h"
#include "Keypad_TCA8418.h"
#include "KeyLabels.h"
#include "Pages.h"

// === Encoders-only additions ===
#define PIN_MCP_RST 8
#ifndef PIN_MCP_INT
#define PIN_MCP_INT 5   // change if your MCP INT is not on GPIO5
#endif
#include "Encoders_MCP23017.h"
// ===============================

static bool okHub75=false; static unsigned long hubLast=0;

static Display_SSD1322  gDisp;
static Keypad_TCA8418   gKeys;

static PageManager  gPages;
static HomePage     gHome;
static SettingsPage gSettings;

// Section pages (stubs for now)
static TitlePage   gEngine1("Engine 1");
static TitlePage   gEngine2("Engine 2");
static TitlePage   gOsc1("Osc 1");
static TitlePage   gOsc2("Osc 2");
static TitlePage   gSubOsc("Sub Osc");
static TitlePage   gAmp1("Amp 1");
static TitlePage   gAmp2("Amp 2");
static TitlePage   gAmp3("Amp 3");
static TitlePage   gFilter("Filter");
static TitlePage   gGlobalAmp("Global Amp");
static TitlePage   gFX("FX");

static char lastMsg[64] = "ready";

void setup() {
  Serial.begin(115200);
  delay(20);
  Serial.println("\\nCarto_Minimal boot");

  // I2C at 400 kHz on UI bus
  Wire.begin(UI_I2C_SDA, UI_I2C_SCL, 400000);
  delay(5);

  bool okDisp = gDisp.begin();
  bool okKeys = gKeys.begin(Wire, TCA8418_INT);
  pinMode(TCA8418_INT, INPUT_PULLUP);
  Serial.printf("{\"oled\":%s,\"keys\":%s}\\n", okDisp?"true":"false", okKeys?"true":"false");

  gDisp.clear();
  gPages.begin(&gDisp);

  // --- MCP23017 RESET + cold-boot probe (active-low) ---
  pinMode(PIN_MCP_RST, OUTPUT);
  digitalWrite(PIN_MCP_RST, HIGH);  // idle HIGH
  delay(2);
  digitalWrite(PIN_MCP_RST, LOW);   // assert reset
  delay(3);
  digitalWrite(PIN_MCP_RST, HIGH);  // release reset
  delay(10);

  // quick I2C probe (0 == ACK)
  auto i2cProbe = [](uint8_t addr)->uint8_t { Wire.beginTransmission(addr); return Wire.endTransmission(); };
  const uint8_t MCP_ADDR = 0x20;

  bool mcpSeen = false;
  for (int i=0; i<15 && !mcpSeen; ++i) { mcpSeen = (i2cProbe(MCP_ADDR)==0); if (!mcpSeen) delay(100); }
  if (!mcpSeen) {
    Serial.println("MCP23017 not found (retrying)");
    uint32_t t1 = millis();
    while (i2cProbe(MCP_ADDR)!=0 && (millis()-t1)<2000) delay(50);
    for (int i=0; i<5 && !mcpSeen; ++i) { mcpSeen = (i2cProbe(MCP_ADDR)==0); if (!mcpSeen) delay(100); }
  }
  Serial.printf("{\"enc_probe\":%s}\\n", mcpSeen ? "true" : "false");

  // bring up the encoder driver once
  Encoders::begin(Wire);
  Encoders::usePanelMapping_EFGH_DCBA();

  Serial.printf("{\"enc_mcp\":%s}\\n", Encoders::isOnline() ? "true" : "false");

  // Add core + section pages and capture indices
  uint8_t iHome      = gPages.add(&gHome);
  uint8_t iSettings  = gPages.add(&gSettings);
  uint8_t iEngine1   = gPages.add(&gEngine1);
  uint8_t iEngine2   = gPages.add(&gEngine2);
  uint8_t iOsc1      = gPages.add(&gOsc1);
  uint8_t iOsc2      = gPages.add(&gOsc2);
  uint8_t iSubOsc    = gPages.add(&gSubOsc);
  uint8_t iAmp1      = gPages.add(&gAmp1);
  uint8_t iAmp2      = gPages.add(&gAmp2);
  uint8_t iAmp3      = gPages.add(&gAmp3);
  uint8_t iFilter    = gPages.add(&gFilter);
  uint8_t iGlobalAmp = gPages.add(&gGlobalAmp);
  uint8_t iFX        = gPages.add(&gFX);

  // Bind hotkeys (label → page). Labels must match KeyLabels.h entries.
  gPages.bind("BTN_ENGINE1",    iEngine1);
  gPages.bind("BTN_ENGINE2",    iEngine2);
  gPages.bind("BTN_OSC1",       iOsc1);
  gPages.bind("BTN_OSC2",       iOsc2);
  gPages.bind("BTN_SUBOSC",     iSubOsc);
  gPages.bind("BTN_AMP1",       iAmp1);
  gPages.bind("BTN_AMP2",       iAmp2);
  gPages.bind("BTN_AMP3",       iAmp3);
  gPages.bind("BTN_FILTER",     iFilter);
  gPages.bind("BTN_GLOBAL_AMP", iGlobalAmp);
  gPages.bind("BTN_FX",         iFX);
  gPages.bind("BTN_SETTINGS",   iSettings); // convenience

  gPages.show(iHome);            // start on Home
  gDisp.present();
  okHub75 = hub75_begin();
}

void loop() {
  // Drain keypad events; act on press (down) only
  gKeys.poll([&](uint8_t row, uint8_t col, bool down){
    if (!down) return;
    const char* label = (row < 8 && col < 10) ? KEY_LABELS[row][col] : "";
    snprintf(lastMsg, sizeof(lastMsg), "%s down", label);
    Serial.printf("{\"type\":\"key\",\"row\":%u,\"col\":%u,\"label\":\"%s\",\"action\":\"down\"}\\n",
                  row, col, label);
    gPages.onKeyLabel(label);
  });

 // --- Encoders: service + consume deltas; show whichever moved (MAPPED) ---
Encoders::service();

char   shownLabel = 0;   // mapped 'A'..'H'
int8_t shownDelta = 0;   // accumulated delta for the last-moving encoder
uint8_t lastHwIdx = 255;

for (uint8_t i = 0; i < 8; ++i) {
  int8_t d = Encoders::getDelta(i);
  if (d) {
    // If you later hook to params, use the MAPPED index:
    // uint8_t j = Encoders::mappedIndex(i);
    // apply 'd' to parameter slot 'j' here

    lastHwIdx = i;
    shownDelta += d;
  }
}

if (lastHwIdx != 255) {
  shownLabel = Encoders::mappedLabel(lastHwIdx);
  snprintf(lastMsg, sizeof(lastMsg), "Enc%c %+d", shownLabel, shownDelta);
  // Optional: Encoders::dumpNonZero(Serial);  // prints {"enc":{"E":1}} etc.
}

  static uint32_t tPrev = 0;
  uint32_t now = millis();
  if (now - tPrev >= 100) {
    tPrev = now;
    gDisp.status(lastMsg);
    gDisp.present();
  }
  if (okHub75 && now - hubLast >= 33) {
    hubLast = now;
    hub75_tick();   // flush once per frame
  }
}
