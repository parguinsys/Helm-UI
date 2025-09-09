Encoders_MCP23017 — Carto v11.3 (INT-driven, non-blocking)
===========================================================
Drop both files next to your .ino.

.ino hooks
----------
#include "Encoders_MCP23017.h"

setup():
  Wire.setClock(400000);
  Encoders::begin(Wire);

loop():
  Encoders::service();
  for (uint8_t i=0;i<8;++i){
    int8_t d = Encoders::getDelta(i);
    if (d) { /* apply to parameter i */ }
  }

Defaults
--------
- MCP23017 @ 0x20
- INT -> GPIO5 (active-low), change with #define PIN_MCP_INT
- 2 transitions per detent; change via Encoders::setDetentsPerStep(1 or 2)
- Per-encoder direction: Encoders::setDir(idx, -1) to invert

Notes
-----
- No I²C in ISR. We read INTCAP A/B to clear and decode snapshots.
- Gray-code table rejects illegal hops; no bounce heuristics needed.
