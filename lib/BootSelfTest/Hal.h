// =============================================================
//  Hal.h — Hardware-Abstraktion fuer den Boot-Selbsttest.
//
//  Reine Schnittstelle, KEIN Arduino.h. Jede Hardware-Interaktion
//  (Speaker, OLED, Zeit) laeuft ausschliesslich hierueber, damit
//  die Selbsttest-Logik nativ mit FakeHal testbar bleibt.
// =============================================================
#pragma once
#include <cstdint>

class Hal {
public:
  virtual ~Hal() = default;

  virtual void beep(uint16_t freqHz, uint16_t ms) = 0;
  virtual void delayMs(uint32_t ms) = 0;

  virtual bool oledInit() = 0;
  virtual void oledShowLine(uint8_t row, const char* text) = 0;
};
