// =============================================================
//  BootSelfTest.h — Boot-Selbsttest-Ablauf: Start-Piep -> OLED.
//  Weitere Stufen (Sensoren, Luefter) folgen in spaeteren
//  Inbetriebnahme-Schritten.
// =============================================================
#pragma once
#include "Hal.h"

struct SelfTestResult {
  bool oledOk;
};

// Einzelstufen, unabhaengig testbar.
void selfTestStartBeep(Hal& hal);
bool selfTestOled(Hal& hal);

// Gesamtablauf in der in CLAUDE.md festgelegten Reihenfolge.
SelfTestResult runBootSelfTest(Hal& hal);
