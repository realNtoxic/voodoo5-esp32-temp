// =============================================================
//  AckButton.h — Taster-Zustandsautomat fuer den Ack-Taster.
//
//  Reine Logik, KEIN Arduino.h -> nativ testbar. Erwartet einen
//  bereits entprellten Tasterzustand (Entprellung ACK_DEBOUNCE_MS
//  passiert vor diesem Aufruf, z. B. in der realen Hal-Implementierung)
//  und den aktuellen Zeitstempel als Parameter -- kein internes
//  millis().
//
//  Tick-Toene laufen ausschliesslich ueber hal.beepAsync() (nicht
//  hal.beep()), siehe CLAUDE.md Abschnitt "Akustik": die
//  1200-ms-Schwelle (AKTION2_TIME) darf nicht durch die Tonlaenge
//  des vorherigen 500-ms-Tons verzoegert werden.
// =============================================================
#pragma once
#include "Hal.h"

enum class AckEvent : uint8_t { None, Ack, LatchReset };

class AckButton {
public:
  // pressed: entprellter Tasterzustand (true = gedrueckt).
  // now: aktueller Zeitstempel in ms.
  AckEvent tick(Hal& hal, bool pressed, uint32_t now);

private:
  bool wasPressed_ = false;
  uint32_t pressStartMs_ = 0;
  bool aktion1Fired_ = false;
  bool aktion2Fired_ = false;
};
