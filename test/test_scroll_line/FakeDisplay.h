// =============================================================
//  FakeDisplay.h — Test-Double fuer IDisplay. Protokolliert jeden
//  Aufruf als String, damit Tests Reihenfolge und Inhalt pruefen
//  koennen (analog zu FakeHal in den anderen Test-Suiten).
// =============================================================
#pragma once
#include <string>
#include <vector>
#include "IDisplay.h"

class FakeDisplay : public IDisplay {
public:
  std::vector<std::string> calls;

  void clear() override {
    calls.push_back("clear");
  }

  void drawText(uint8_t x, uint8_t y, const char* s, uint8_t size, bool inverted) override {
    calls.push_back("drawText:" + std::to_string(x) + ":" + std::to_string(y) + ":" +
                     s + ":" + std::to_string(size) + ":" + (inverted ? "1" : "0"));
  }

  void fillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h) override {
    calls.push_back("fillRect:" + std::to_string(x) + ":" + std::to_string(y) + ":" +
                     std::to_string(w) + ":" + std::to_string(h));
  }

  void frameRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h) override {
    calls.push_back("frameRect:" + std::to_string(x) + ":" + std::to_string(y) + ":" +
                     std::to_string(w) + ":" + std::to_string(h));
  }

  void clearRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h) override {
    calls.push_back("clearRect:" + std::to_string(x) + ":" + std::to_string(y) + ":" +
                     std::to_string(w) + ":" + std::to_string(h));
  }

  void hLine(uint8_t y) override {
    calls.push_back("hLine:" + std::to_string(y));
  }

  void vLine(uint8_t x) override {
    calls.push_back("vLine:" + std::to_string(x));
  }

  void present() override {
    calls.push_back("present");
  }
};
