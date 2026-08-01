// =============================================================
//  SensorCal.h — Lastabhaengige Die-Korrektur (siehe CLAUDE.md
//  "Sensor-Kalibrierung", config.h SENSOR_K).
//
//  Reine Logik, KEIN Arduino.h -> nativ testbar. Ein konstanter
//  Offset waere physikalisch falsch: die Differenz Die-zu-Sonde ist
//  bei Nulllast selbst null und waechst mit der Verlustleistung
//  (ΔT = P * R_thermisch). Da P nicht gemessen wird, dient die
//  Kuehler-Uebertemperatur ueber Ambient (rawSondeC - ambC) als
//  monoton steigender Stellvertreter fuer die Last:
//
//    T_die ≈ T_sonde + k * (T_sonde - T_amb), k=0 -> keine Korrektur.
//
//  rawSondeC geht IMMER unveraendert ins Log, niemals das Ergebnis
//  dieser Funktion -- nur so verliert eine spaetere Nachkalibrierung
//  (neues k) nicht die Historie. Aufrufer muessen rawSondeC deshalb
//  weiterhin separat fuers Log vorhalten.
// =============================================================
#pragma once

float dieTempC(float rawSondeC, float ambC, float k);
