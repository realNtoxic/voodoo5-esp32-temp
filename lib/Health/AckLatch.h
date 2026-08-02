// =============================================================
//  AckLatch.h — Ack-Zustandslogik pro Fehler-Latch (siehe CLAUDE.md
//  "Fehler-Latch und Alarm" / "Ack-Taster"). Reine Logik, KEIN
//  Arduino.h -> nativ testbar. Baut NICHT den vollen Health-Monitor
//  (Sensor-Plausibilitaet, CRC-Retry, ...) -- nur den Ack-Teil des
//  spaeteren lib/Health/-Bausteins.
//
//  Kein internes millis() -- der aktuelle Zeitstempel wird
//  hereingereicht, analog zu AckButton::tick().
// =============================================================
#pragma once
#include <cstdint>

struct ChannelLatch {
  bool     active;       // Fehler aktuell gelatcht (offen)
  bool     acked;        // bereits quittiert
  uint32_t erroredAtMs;  // Zeitpunkt, an dem der Fehler auftrat
};

// Ein Fehler, der juenger als ACK_GRACE_MS ist, kann noch nicht
// quittiert werden (Karenzzone, siehe config.h ACK_GRACE_MS) --
// erzwingt bewusste Kenntnisnahme statt reflexartigem Wegdruecken und
// loest den Race "Ack genau im Moment eines neuen Fehlers" ohne
// Zeitstempel-Tricks.
bool canAck(uint32_t errAgeMs);

// Quittiert alle aktiven, noch nicht quittierten und nicht in der
// Karenzzone befindlichen Latches auf einmal (Ein-Taster-Geraet -- ein
// Tastendruck quittiert alles gleichzeitig). Latches, die juenger als
// ACK_GRACE_MS sind, bleiben unacked und muessen beim naechsten
// Tastendruck erneut geprueft werden.
void ackAllActive(ChannelLatch* latches, uint8_t count, uint32_t now);
