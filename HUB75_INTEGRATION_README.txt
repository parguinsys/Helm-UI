Carto v11.3 HUB75 Integration (Minimal Patch)
=============================================
Baseline: Carto_Minimal_v11_3a_hotkeys

Files added:
  • HUB75Config.h
  • LegacyHub75.h
  • LegacyHub75.cpp
(Place them in the same folder as Carto_Minimal_v11_3a_hotkeys.ino.)

Minimal .ino edits (already made in the patched copy):
  1) Add includes:
     #include "HUB75Config.h"
     #include "LegacyHub75.h"

  2) Globals:
     static bool okHub75=false; static uint32_t hubLast=0;

  3) setup(): after gDisp.present();
     okHub75 = hub75_begin();

  4) loop(): after gDisp.present();
     if(okHub75 && millis()-hubLast>=33){ hubLast=millis(); hub75_tick(); }

What you'll see on boot:
  • OLED and hotkeys unchanged.
  • HUB75 shows green header band + checkerboard (init-only smoke test).
  • A single red pixel moves at ~30 Hz (non-blocking).

Pins/brightness:
  • Pins and geometry are set in HUB75Config.h matching your ESP32-S3 map.
  • Brightness defaults to 60; change HUB75_BRIGHTNESS to taste (e.g., 140).

Patch summary:
{
  "ino_file": "Carto_Minimal_v11_3a_hotkeys/Carto_Minimal_v11_3a_hotkeys.ino",
  "changes": [
    "Added HUB75 includes",
    "Added okHub75/hubLast globals",
    "Inserted hub75_begin() after gDisp.present() in setup()",
    "Inserted 30Hz hub75_tick() gate after gDisp.present() in loop()"
  ]
}
