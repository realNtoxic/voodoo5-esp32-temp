// =============================================================
//  Hal.h — Hardware-Abstraktion fuer den Boot-Selbsttest.
//
//  Reine Schnittstelle, KEIN Arduino.h. Jede Hardware-Interaktion
//  (Speaker, OLED, Sensoren, Luefter, Zeit) laeuft ausschliesslich
//  hierueber, damit die Selbsttest-Logik nativ mit FakeHal testbar
//  bleibt.
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

  // Liefert die Temperatur des Sensors mit der uebergebenen ROM-Adresse,
  // oder -127.0f (DS18B20-Diskonnekt-Sentinel), wenn der Sensor nicht
  // antwortet.
  virtual float sensorReadTempC(const uint8_t rom[8]) = 0;

  // Scannt den 1-Wire-Bus und gibt gefundene ROM-Adressen paste-fertig
  // ueber Serial aus. Nur im Discovery-Modus aufgerufen.
  virtual void runOneWireDiscovery() = 0;

  // fanIndex: 0 = Luefter 1, 1 = Luefter 2.
  virtual void fanSetDutyPercent(uint8_t fanIndex, uint8_t dutyPercent) = 0;

  // Misst die Drehzahl ueber ein Zeitfenster (TACHO_WIN_MS) und liefert
  // sie in RPM.
  virtual uint16_t fanReadRpm(uint8_t fanIndex) = 0;
};
