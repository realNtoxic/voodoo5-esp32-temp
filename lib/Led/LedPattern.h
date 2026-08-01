// =============================================================
//  LedPattern.h — reine, unabhaengig testbare Musterlogik fuer den
//  LED-Meldekanal (PIN_LED, siehe config.h). KEIN Arduino.h -> nativ
//  testbar. Kein delay()/millis() intern -- Zeit/Takt werden
//  hereingereicht (siehe CLAUDE.md Architektur-Regel 3/4).
//
//  Heartbeat folgt derselben Regelkreis-`phase` wie das
//  OLED-Lebenszeichen (Dashboard::heartbeatChar()) -- deshalb der
//  phase-Parameter statt eines eigenen Timers: friert der Loop,
//  friert auch die LED (das beweist Liveness ueberhaupt erst).
//  Fault ist ein schnelles, vom Heartbeat klar unterscheidbares
//  Doppelblinken ueber ein festes Zeitfenster (LED_FAULT_* in
//  config.h) -- dafuer braucht es echte Millisekunden statt phase.
//  ledLevel() bekommt deshalb beide Parameter und nutzt je nach
//  Modus nur den jeweils passenden.
// =============================================================
#pragma once
#include <cstdint>

enum class LedMode : uint8_t { Heartbeat, Fault };

// faultActive = gelatchter, nicht quittierter Fehler (latched & ~acked
// != 0, siehe CLAUDE.md "Fehler-Latch und Alarm"). Der Latch selbst
// wird hier nicht neu gebaut -- der Aufrufer ermittelt den Bool aus
// seinem jeweiligen Latch-Zustand und reicht ihn herein.
LedMode selectLedMode(bool faultActive);

bool ledLevel(LedMode mode, uint8_t phase, uint32_t nowMs);
