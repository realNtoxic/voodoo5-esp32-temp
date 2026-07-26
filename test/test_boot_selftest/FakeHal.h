// =============================================================
//  FakeHal.h — Test-Double fuer Hal. Protokolliert jeden Aufruf
//  als String, damit Tests Reihenfolge und Inhalt pruefen koennen.
// =============================================================
#pragma once
#include <string>
#include <vector>
#include "Hal.h"

class FakeHal : public Hal {
public:
  std::vector<std::string> calls;
  bool oledInitResult = true;

  // Nur zur Dokumentation des Testszenarios (Bus haengt nach Reset fest,
  // siehe CLAUDE.md "Fallen") -- FakeHal simuliert keine echte GPIO-Ebene,
  // i2cRecover() wird unten unabhaengig davon einfach geloggt.
  bool sdaStuckLow = false;

  void beep(uint16_t freqHz, uint16_t ms) override {
    calls.push_back("beep:" + std::to_string(freqHz) + ":" + std::to_string(ms));
  }

  void beepAsync(uint16_t freqHz, uint16_t ms) override {
    calls.push_back("beepAsync:" + std::to_string(freqHz) + ":" + std::to_string(ms));
  }

  void delayMs(uint32_t ms) override {
    calls.push_back("delayMs:" + std::to_string(ms));
  }

  void i2cRecover(uint8_t sdaPin, uint8_t sclPin) override {
    calls.push_back("i2cRecover:" + std::to_string(sdaPin) + ":" + std::to_string(sclPin));
    sdaStuckLow = false;
  }

  bool oledInit() override {
    calls.push_back("oledInit");
    return oledInitResult;
  }

  void oledShowLine(uint8_t row, const char* text) override {
    calls.push_back("oledShowLine:" + std::to_string(row) + ":" + text);
  }

  void setHeartbeatLed(bool on) override {
    calls.push_back(std::string("setHeartbeatLed:") + (on ? "1" : "0"));
  }
};
