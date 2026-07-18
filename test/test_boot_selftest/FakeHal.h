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

  // Default: alle drei Sensoren antworten mit einem plausiblen Wert.
  // selfTestSensors() fragt Rollen immer in fester Reihenfolge
  // (VSA1, VSA2, AMB) ab -> der Aufrufzaehler bestimmt die Rolle, ein
  // ROM-Wertevergleich waere unzuverlaessig (die ausgelieferte
  // config.h verwendet fuer alle drei Rollen dieselbe Platzhalter-ROM).
  float sensorTemps[3] = { 25.0f, 25.0f, 25.0f };
  uint8_t sensorCallCount = 0;

  // Default: beide Luefter melden eine Drehzahl oberhalb FAN_MIN_RPM.
  uint16_t fanRpms[2] = { 1000, 1000 };

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

  float sensorReadTempC(const uint8_t rom[8]) override {
    (void)rom;
    const uint8_t role = sensorCallCount < 3 ? sensorCallCount : 2;
    ++sensorCallCount;
    calls.push_back("sensorReadTempC:" + std::to_string(role));
    return sensorTemps[role];
  }

  void runOneWireDiscovery() override {
    calls.push_back("runOneWireDiscovery");
  }

  void fanSetDutyPercent(uint8_t fanIndex, uint8_t dutyPercent) override {
    calls.push_back("fanSetDutyPercent:" + std::to_string(fanIndex) + ":" + std::to_string(dutyPercent));
  }

  uint16_t fanReadRpm(uint8_t fanIndex) override {
    calls.push_back("fanReadRpm:" + std::to_string(fanIndex));
    return fanRpms[fanIndex];
  }
};
