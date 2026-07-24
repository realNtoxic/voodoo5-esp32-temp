// =============================================================
//  FakeHal.h — Test-Double fuer Hal. Protokolliert jeden Aufruf
//  als String, damit Tests Reihenfolge und Inhalt pruefen koennen.
//
//  Eigene Kopie je Test-Suite (PlatformIO baut jedes test/test_*
//  Verzeichnis unabhaengig) -- analog zu test/test_boot_selftest/.
// =============================================================
#pragma once
#include <string>
#include <vector>
#include "Hal.h"

class FakeHal : public Hal {
public:
  std::vector<std::string> calls;
  bool oledInitResult = true;

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
  }

  bool oledInit() override {
    calls.push_back("oledInit");
    return oledInitResult;
  }

  void oledShowLine(uint8_t row, const char* text) override {
    calls.push_back("oledShowLine:" + std::to_string(row) + ":" + text);
  }
};
