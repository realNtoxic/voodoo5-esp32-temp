// =============================================================
//  ILed.h — schmale Schnittstelle fuer den LED-Meldekanal (PIN_LED,
//  siehe config.h). Analog zu IDisplay.h: eine fokussierte
//  Abstraktion fuer genau dieses Modul, unabhaengig von der
//  groeberen Hal::setHeartbeatLed(). Reine Schnittstelle, KEIN
//  Arduino.h -> nativ mit einem Fake testbar.
// =============================================================
#pragma once

class ILed {
public:
  virtual ~ILed() = default;
  virtual void set(bool on) = 0;
};
