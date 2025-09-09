// Encoders_MCP23017.h — Carto v11.3
// INT-driven quadrature decode for 8 encoders via MCP23017 @0x20.
// - No I2C in ISR; service() reads INTCAP A/B snapshots.
// - Gray-code table decode with detent policy (1x/2x).
// - Per-encoder direction invert.
// - Minimal debug helpers to keep .ino clean.
// - Label/Index remap helpers so panel silkscreen can differ from hardware wiring.

#pragma once
#include <Arduino.h>
#include <Wire.h>

#ifndef ENC_MCP_ADDR
#define ENC_MCP_ADDR 0x20
#endif

#ifndef PIN_MCP_INT
#define PIN_MCP_INT 5   // ESP32-S3 GPIO5 (active-low, open-drain from MCP)
#endif

#ifndef ENC_DIR_DEFAULT
#define ENC_DIR_DEFAULT +1
#endif

namespace Encoders {

// ---- Core API ---------------------------------------------------------------
void begin(TwoWire &w = Wire);
void service();                     // call from loop()
int8_t getDelta(uint8_t idx);       // returns & clears delta for encoder hw index 0..7
void  setDetentsPerStep(uint8_t d); // 1 or 2 (default 2 for PEC12R)
void  setDir(uint8_t idx, int8_t dir); // +1 or -1 per encoder
bool  isOnline();                   // true if MCP responded at init
uint16_t lastSnapshot();            // last 16-bit INTCAP snapshot (A=low byte)

// ---- Debug / convenience (optional) ----------------------------------------
char label(uint8_t idx);            // 'A'..'H' for hardware index (pre-remap)
void getAllDeltas(int8_t out[8]);   // fill out[8] with current deltas (clears them)
void dumpNonZero(Stream& s);        // print only moved encoders as {"enc":{"A":1,...}}

// ---- Label / index remap ---------------------------------------------------
// Some front panels wire A..H differently from the logical order you want to show/use.
// Provide a label remap for display, and an index remap for routing deltas.
// Defaults are identity (label 'A'..'H', index 0..7).

void setLabelRemap(const char labels[8]);  // e.g., {'E','F','G','H','D','C','B','A'}
void setIndexRemap(const uint8_t map[8]);  // e.g., {4,5,6,7,3,2,1,0}

// Get mapped values (safe helpers):
char mappedLabel(uint8_t hwIdx);     // returns labelRemap[hwIdx]
uint8_t mappedIndex(uint8_t hwIdx);  // returns indexRemap[hwIdx]

// One-liner for your stated mapping: ABCD→EFGH and HGFE→ABCD
void usePanelMapping_EFGH_DCBA();

} // namespace Encoders
