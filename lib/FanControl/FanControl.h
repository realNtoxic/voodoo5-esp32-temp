// =============================================================
//  FanControl.h — Kennlinie (curveDuty), Ambient-Panik-Uebersteuerung
//  und Sensorausfall-Fail-Safe.
//
//  4 unabhaengige Regelkreise (siehe CLAUDE.md "Regelmodell"): jeder
//  Luefter reagiert nur auf seinen zugeordneten Sensor, kein
//  Zonen-Mischen. curveDuty() wird viermal unabhaengig mit je eigenem
//  (tempC, fanWasOn)-Zustand aufgerufen. Reine Logik, KEIN Arduino.h
//  -> nativ testbar.
// =============================================================
#pragma once
#include <cstdint>

// Kennlinienfunktion (Architektur-Regel 3): bekommt Temperatur und
// vorherigen Zustand (`fanWasOn`, das Hysterese-Gedaechtnis dieses
// einen Kreises) herein und gibt die Duty zurueck -- keine
// Seiteneffekte, keine Zeitabhaengigkeit. Kickstart lebt im
// Controller, nicht hier (siehe CLAUDE.md Architektur-Regel 3).
uint8_t curveDuty(float tempC, bool fanWasOn);

// Hysterese-Zustandsautomat fuer die Ambient-Panik-Schwelle
// (AMB_WARN_C / AMB_WARN_OFF_C, siehe config.h). `wasPanicActive` ist
// der vorherige Zustand -- keine Zeitabhaengigkeit, kein internes
// millis(), wie bei allen testbaren Kennlinienfunktionen.
bool ambientPanicActive(float ambTempC, bool wasPanicActive);

// Panik-Uebersteuerung gewinnt gegen die normale Kennlinie:
// effektive Duty = max(Kennlinien-Duty, CURVE.maxDuty) bei aktiver
// Panik, sonst unveraendert die Kennlinien-Duty.
uint8_t effectiveDuty(uint8_t curveDuty, bool panicActive);

// Sensorausfall-Fail-Safe PRO KREIS (nicht mehr "der andere Luefter"
// wie in der alten 2-Kanal-Logik): faellt der einem Kreis zugeordnete
// Sensor aus, geht NUR dieser eine Luefter auf 100 %. Andere Kreise
// bleiben unberuehrt.
uint8_t channelFailSafeDuty(uint8_t curveDuty, bool sensorOk);
