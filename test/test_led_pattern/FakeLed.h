// =============================================================
//  FakeLed.h — Test-Double fuer ILed. Protokolliert jeden
//  set()-Aufruf mit Zeitstempel in einen std::vector, damit Tests
//  das erzeugte Muster pruefen koennen (analog zu FakeDisplay in
//  test_dashboard). ILed::set() selbst kennt keine Zeit -- der Test
//  setzt `simulatedNowMs` vor jedem Aufruf, FakeLed protokolliert
//  nur, was ihr in diesem Moment mitgeteilt wurde.
// =============================================================
#pragma once
#include <cstdint>
#include <vector>
#include "ILed.h"

class FakeLed : public ILed {
public:
  struct Call {
    bool     level;
    uint32_t atMs;
  };

  std::vector<Call> calls;
  uint32_t simulatedNowMs = 0;

  void set(bool on) override {
    calls.push_back(Call{ on, simulatedNowMs });
  }
};
