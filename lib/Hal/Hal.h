// =============================================================
//  Hal.h — Hardware-Abstraktion, gemeinsam genutzt von allen
//  testbaren Logik-Modulen (BootSelfTest, AckButton, ...).
//
//  Reine Schnittstelle, KEIN Arduino.h. Jede Hardware-Interaktion
//  (Speaker, OLED, Zeit) laeuft ausschliesslich hierueber, damit
//  die Logik nativ mit FakeHal testbar bleibt.
// =============================================================
#pragma once
#include <cstdint>

class Hal {
public:
  virtual ~Hal() = default;

  // Blockierender Ton. Ausschliesslich dem Boot-Selbsttest vorbehalten
  // (siehe CLAUDE.md, Abschnitt "Akustik").
  virtual void beep(uint16_t freqHz, uint16_t ms) = 0;

  // Nicht-blockierender Ton fuer den Laufbetrieb (Wiederhol-Alarm,
  // Ack-Taster). Kehrt sofort zurueck, die Rechteckwelle laeuft in
  // einer separaten FreeRTOS-Task weiter.
  virtual void beepAsync(uint16_t freqHz, uint16_t ms) = 0;

  virtual void delayMs(uint32_t ms) = 0;

  // Bus-Recovery: Muss vor jedem Wire.begin() laufen (siehe CLAUDE.md,
  // Abschnitt "Fallen"). Ein Reset waehrend einer laufenden
  // I2C-Uebertragung kann den Slave mit SDA auf LOW haengen lassen --
  // ohne Recovery bleibt der Bus dann dauerhaft blockiert.
  virtual void i2cRecover(uint8_t sdaPin, uint8_t sclPin) = 0;

  virtual bool oledInit() = 0;
  virtual void oledShowLine(uint8_t row, const char* text) = 0;
};
