// =============================================================
//  SensorCal.h — Sensor-Offset-Kalibrierung (siehe CLAUDE.md
//  "Sensor-Kalibrierung", config.h SENSOR_OFFSET_C).
//
//  Reine Logik, KEIN Arduino.h -> nativ testbar. Der Offset ist eine
//  reine Regel-/Anzeige-Groesse: rawC geht IMMER unveraendert ins
//  Log, calC (= rawC + Offset) nur in Regelung/Anzeige. Diese Funktion
//  darf deshalb niemals als Ersatz fuer den geloggten Rohwert
//  verwendet werden -- Aufrufer muessen rawC weiterhin separat
//  fuer das Log vorhalten.
// =============================================================
#pragma once
#include <cstdint>

float applySensorOffset(float rawC, uint8_t sensorIndex);
