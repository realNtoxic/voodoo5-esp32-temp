// =============================================================
//  History.h — Persistente Max-Uebertemperatur-Historie zur
//  Degradierungs-Erkennung (siehe CLAUDE.md "Verschleiss-Historie").
//
//  Reine Logik, KEIN Arduino.h -> nativ testbar. Gespeichert wird die
//  UEBERTEMPERATUR ueber Ambient (rawSondeC - rawAmbC), nicht der
//  Roh-Absolutwert und nicht der kalibrierte Wert (SENSOR_K): ein
//  Absolut-Rekord waere teils nur Raumtemperatur, und ein
//  raw-basierter Wert bleibt gueltig, auch wenn `k` spaeter
//  nachkalibriert wird -- konsistent zur Rohwert-Regel aus
//  lib/SensorCal/.
// =============================================================
#pragma once
#include <cstdint>

struct HistoryData {
  float maxDeltaC[4];  // VSA1.1, VSA1.2, VSA2.1, VSA2.2: max(rawSondeC - rawAmbC)
  float maxAmbC;       // Gehaeuse-Rekord, roh
};

// candidate zaehlt nur als neuer Rekord, wenn er `stored` um
// mindestens `eps` uebersteigt (siehe config.h HISTORY_EPSILON_C) --
// filtert Sensorrauschen und Luftturbulenz. Aktualisiert `stored` bei
// einem echten Rekord. Rueckgabe true = neuer Rekord ("dirty", muss
// spaeter persistiert werden).
bool tryUpdateMax(float& stored, float candidate, float eps);
