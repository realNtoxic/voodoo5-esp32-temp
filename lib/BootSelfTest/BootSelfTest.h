// =============================================================
//  BootSelfTest.h — Boot-Selbsttest-Ablauf: Start-Piep -> OLED ->
//  Sensoren -> Luefter (oder Discovery-Modus, falls SENSOR_ROM
//  ungueltig).
// =============================================================
#pragma once
#include "Hal.h"

struct SelfTestResult {
  bool oledOk;
  bool discoveryMode;  // true: Discovery statt Regelbetrieb, siehe CLAUDE.md
  bool sensorsOk;       // nur aussagekraeftig, wenn discoveryMode == false
  bool fansOk;          // nur aussagekraeftig, wenn discoveryMode == false
};

// Einzelstufen, unabhaengig testbar.
void selfTestStartBeep(Hal& hal);
bool selfTestOled(Hal& hal);
bool selfTestSensors(Hal& hal);
bool selfTestFans(Hal& hal);

// Reine Pruefung ohne Hal: Family-Byte 0x28 und CRC8 (Dallas/Maxim).
bool sensorRomValid(const uint8_t rom[8]);

// true, wenn irgendeine SENSOR_ROM-Adresse ungueltig ist oder
// FORCE_DISCOVERY gesetzt ist.
bool selfTestNeedsDiscovery();

// Gesamtablauf in der in CLAUDE.md festgelegten Reihenfolge.
SelfTestResult runBootSelfTest(Hal& hal);
