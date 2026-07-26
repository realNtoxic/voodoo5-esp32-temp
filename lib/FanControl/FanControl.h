// =============================================================
//  FanControl.h — Ambient-Panik-Uebersteuerung.
//
//  Erster Baustein des in CLAUDE.md/Projektstruktur reservierten
//  FanControl-Moduls. Kennlinie/Kickstart (curveDuty()) folgen in
//  einem spaeteren Schritt -- hier nur die neue Ambient-Panik-Logik.
//  Reine Logik, KEIN Arduino.h -> nativ testbar.
// =============================================================
#pragma once
#include <cstdint>

// Hysterese-Zustandsautomat fuer die Ambient-Panik-Schwelle
// (AMB_WARN_C / AMB_WARN_OFF_C, siehe config.h). `wasPanicActive` ist
// der vorherige Zustand -- keine Zeitabhaengigkeit, kein internes
// millis(), wie bei allen testbaren Kennlinienfunktionen.
bool ambientPanicActive(float ambTempC, bool wasPanicActive);

// Panik-Uebersteuerung gewinnt gegen die normale Kennlinie:
// effektive Duty = max(Kennlinien-Duty, CURVE.maxDuty) bei aktiver
// Panik, sonst unveraendert die Kennlinien-Duty.
uint8_t effectiveDuty(uint8_t curveDuty, bool panicActive);
