// =============================================================
//  StatusLed.h — Display-unabhaengiger dritter Meldekanal (LED an
//  GPIO2, siehe config.h PIN_LED).
//
//  Reine Logik, KEIN Arduino.h -> nativ testbar. Nur der
//  Heartbeat-Grundzustand ist hier bereits als Regel festgelegt
//  (an, wenn `phase != 0` -- derselbe Regelkreis-Takt wie das
//  OLED-Lebenszeichen, siehe Dashboard::heartbeatChar()). Das
//  unterscheidbare Fehler-Blinkmuster (synchron zum Alarmton) folgt
//  erst, wenn Latch/Alarm real existiert -- siehe CLAUDE.md
//  Abschnitt "Dashboard-Anzeige & Meldekanaele".
// =============================================================
#pragma once
#include <cstdint>

bool heartbeatLedOn(uint8_t phase);
