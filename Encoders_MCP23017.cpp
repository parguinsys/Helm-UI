// Encoders_MCP23017.cpp — Carto v11.3
#include "Encoders_MCP23017.h"

namespace {
  TwoWire *bus = &Wire;
  volatile bool intFlag = false;
  bool mcpOk = false;

  // MCP23017 registers
  constexpr uint8_t IODIRA   = 0x00;
  constexpr uint8_t IODIRB   = 0x01;
  constexpr uint8_t IPOLA    = 0x02;
  constexpr uint8_t IPOLB    = 0x03;
  constexpr uint8_t GPINTENA = 0x04;
  constexpr uint8_t GPINTENB = 0x05;
  constexpr uint8_t DEFVALA  = 0x06;
  constexpr uint8_t DEFVALB  = 0x07;
  constexpr uint8_t INTCONA  = 0x08;
  constexpr uint8_t INTCONB  = 0x09;
  constexpr uint8_t IOCON    = 0x0A;
  constexpr uint8_t GPPUA    = 0x0C;
  constexpr uint8_t GPPUB    = 0x0D;
  constexpr uint8_t INTFA    = 0x0E;
  constexpr uint8_t INTFB    = 0x0F;
  constexpr uint8_t INTCAPA  = 0x10;
  constexpr uint8_t INTCAPB  = 0x11;
  constexpr uint8_t GPIOA    = 0x12;
  constexpr uint8_t GPIOB    = 0x13;

  // Hardware wiring: A..D on GPA0..7 (even/odd), E..H on GPB0..7 (even/odd)
  const uint8_t ENC_A_BITS[8] = { 0,2,4,6, 0,2,4,6 };
  const uint8_t ENC_B_BITS[8] = { 1,3,5,7, 1,3,5,7 };

  // Quadrature decode: index = (prev<<2)|curr -> step -1,0,+1
  const int8_t QDEC_TBL[16] = {
     0, -1, +1,  0,
    +1,  0,  0, -1,
    -1,  0,  0, +1,
     0, +1, -1,  0
  };

  // State
  uint8_t prevAB[8] = {0};
  int8_t  dir[8]    = {ENC_DIR_DEFAULT,ENC_DIR_DEFAULT,ENC_DIR_DEFAULT,ENC_DIR_DEFAULT,
                        ENC_DIR_DEFAULT,ENC_DIR_DEFAULT,ENC_DIR_DEFAULT,ENC_DIR_DEFAULT};
  int8_t  detentsPerStep = 2; // 2 transitions per detent (common)
  int8_t  accum[8]  = {0};
  int8_t  delta[8]  = {0};
  uint16_t lastCapAB = 0;

  // Label & index remap (defaults: identity)
  char    labelRemap[8] = { 'A','B','C','D','E','F','G','H' };
  uint8_t indexRemap[8] = {  0,  1,  2,  3,  4,  5,  6,  7  };

  void IRAM_ATTR onInt() { intFlag = true; }

  // I2C helpers
  void writeReg(uint8_t reg, uint8_t val){
    bus->beginTransmission(ENC_MCP_ADDR);
    bus->write(reg); bus->write(val);
    bus->endTransmission();
  }
  uint8_t readReg(uint8_t reg){
    bus->beginTransmission(ENC_MCP_ADDR);
    bus->write(reg); bus->endTransmission(false);
    bus->requestFrom((int)ENC_MCP_ADDR, 1);
    return bus->available() ? bus->read() : 0xFF;
  }
  void readRegs(uint8_t reg, uint8_t *dst, uint8_t n){
    bus->beginTransmission(ENC_MCP_ADDR);
    bus->write(reg); bus->endTransmission(false);
    bus->requestFrom((int)ENC_MCP_ADDR, (int)n);
    for(uint8_t i=0;i<n && bus->available();++i) dst[i]=bus->read();
  }

  inline uint8_t getAB(uint16_t snap, uint8_t idx){
    uint8_t bitA = ENC_A_BITS[idx] + ((idx>=4)?8:0);
    uint8_t bitB = ENC_B_BITS[idx] + ((idx>=4)?8:0);
    uint8_t a = (snap >> bitA) & 1;
    uint8_t b = (snap >> bitB) & 1;
    return (a<<1) | b;
  }

  void processSnapshot(uint16_t snap){
    for (uint8_t i=0;i<8;++i){
      uint8_t curr = getAB(snap, i);
      uint8_t prev = prevAB[i];
      uint8_t idx  = ((prev & 0x3) << 2) | (curr & 0x3);
      int8_t step  = QDEC_TBL[idx];
      if (step){
        accum[i] += step;
        if (accum[i] >= detentsPerStep){ delta[i] += dir[i];  accum[i]-=detentsPerStep; }
        else if (accum[i] <= -detentsPerStep){ delta[i] -= dir[i]; accum[i]+=detentsPerStep; }
      }
      prevAB[i] = curr;
    }
  }
} // anon

namespace Encoders {

// ---- Core ----
void begin(TwoWire &w){
  bus = &w; intFlag=false; mcpOk=false;
  delay(10);                       // settle after power/reset
  writeReg(IOCON, 0x64);           // MIRROR=1, SEQOP=1, ODR=1 (low-active INT)
  writeReg(IODIRA, 0xFF);          // inputs
  writeReg(IODIRB, 0xFF);
  writeReg(GPPUA,  0xFF);          // pullups
  writeReg(GPPUB,  0xFF);
  writeReg(INTCONA,0x00);          // compare to previous (any change)
  writeReg(INTCONB,0x00);
  writeReg(GPINTENA,0xFF);         // enable interrupt-on-change
  writeReg(GPINTENB,0xFF);

  // Clear pending & seed prev states
  uint8_t caps[2]={0,0}; readRegs(INTCAPA, caps, 2);
  lastCapAB = (uint16_t)caps[0] | ((uint16_t)caps[1]<<8);
  uint8_t g[2]={0,0}; readRegs(GPIOA, g, 2);
  uint16_t live = (uint16_t)g[0] | ((uint16_t)g[1]<<8);
  for (uint8_t i=0;i<8;++i){
    uint8_t a = (live >> (ENC_A_BITS[i] + ((i>=4)?8:0))) & 1;
    uint8_t b = (live >> (ENC_B_BITS[i] + ((i>=4)?8:0))) & 1;
    prevAB[i] = (a<<1)|b;
  }

  pinMode(PIN_MCP_INT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_MCP_INT), onInt, FALLING);

  uint8_t iocon = readReg(IOCON);
  mcpOk = (iocon & 0x64) == 0x64;
}

void service(){
  if (!mcpOk || !intFlag) return;
  uint8_t caps[2];
  uint8_t safety = 8;
  do {
    readRegs(INTCAPA, caps, 2);
    lastCapAB = (uint16_t)caps[0] | ((uint16_t)caps[1]<<8);
    processSnapshot(lastCapAB);
    safety--;
  } while (safety && digitalRead(PIN_MCP_INT)==LOW);
  intFlag = (digitalRead(PIN_MCP_INT)==LOW);
}

int8_t getDelta(uint8_t idx){
  if (idx>=8) return 0;
  int8_t d = delta[idx]; delta[idx]=0; return d;
}

void setDetentsPerStep(uint8_t d){ detentsPerStep = (d<1)?1:((d>4)?4:d); }
void setDir(uint8_t idx, int8_t d){ if (idx<8) dir[idx] = (d>=0)?+1:-1; }
bool isOnline(){ return mcpOk; }
uint16_t lastSnapshot(){ return lastCapAB; }

// ---- Debug helpers ----
char label(uint8_t idx){ return (idx<8) ? (char)('A'+idx) : '?'; }

void getAllDeltas(int8_t out[8]){
  for (uint8_t i=0;i<8;++i){ out[i] = getDelta(i); }
}

void dumpNonZero(Stream& s){
  bool any=false;
  s.print("{\"enc\":{");
  for (uint8_t i=0;i<8;++i){
    int8_t d = getDelta(i);
    if (d){
      if (any) s.print(",");
      s.print("\""); s.print(mappedLabel(i)); s.print("\":"); s.print(d);
      any=true;
    }
  }
  s.print("}}");
  if (any) s.print("\n");
}

// ---- Label / index remap ----
void setLabelRemap(const char labels[8]){
  for (uint8_t i=0;i<8;++i){
    char L = labels[i];
    labelRemap[i] = (L>=32 && L<=126) ? L : ('A'+i); // printable fallback
  }
}

void setIndexRemap(const uint8_t map[8]){
  for (uint8_t i=0;i<8;++i){
    uint8_t m = map[i];
    indexRemap[i] = (m<8) ? m : i; // in-range fallback
  }
}

char mappedLabel(uint8_t hwIdx){
  return (hwIdx<8) ? labelRemap[hwIdx] : '?';
}

uint8_t mappedIndex(uint8_t hwIdx){
  return (hwIdx<8) ? indexRemap[hwIdx] : hwIdx;
}

// One-liner for: ABCD → EFGH, and HGFE → ABCD
void usePanelMapping_EFGH_DCBA(){
  const char    LBL[8] = { 'E','F','G','H',  'D','C','B','A' };
  const uint8_t MAP[8] = {  4,  5,  6,  7,    3,  2,  1,  0  };
  setLabelRemap(LBL);
  setIndexRemap(MAP);
}

} // namespace Encoders
