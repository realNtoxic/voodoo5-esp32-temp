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

  void beep(uint16_t freqHz, uint16_t ms) override {
    calls.push_back("beep:" + std::to_string(freqHz) + ":" + std::to_string(ms));
  }

  void delayMs(uint32_t ms) override {
    calls.push_back("delayMs:" + std::to_string(ms));
  }

  bool oledInit() override {
    calls.push_back("oledInit");
    return oledInitResult;
  }

  void oledShowLine(uint8_t row, const char* text) override {
    calls.push_back("oledShowLine:" + std::to_string(row) + ":" + text);
  }
};
