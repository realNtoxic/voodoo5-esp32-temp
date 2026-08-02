// =============================================================
//  StatusCell.h — gemeinsame Status-Zell-Routine fuer die vier
//  Kanal-Statuszellen UND das Ambient-Segment in Zeile 5 (siehe
//  CLAUDE.md "Dashboard-Anzeige"). Kein doppelter Code zwischen
//  Kanaelen und Ambient-Segment: beide nutzen exakt dieselbe
//  Funktion. Reine Teil-Logik als freie Funktionen, unabhaengig
//  testbar; das Zeichnen selbst laeuft ausschliesslich ueber
//  IDisplay, KEIN Arduino.h.
// =============================================================
#pragma once
#include <cstdint>
#include "Dashboard.h"
#include "IDisplay.h"

// Ok/Idle -> nie. Warn -> immer (lebende Bedingung, kein Ack-Konzept,
// verschwindet von selbst wieder). Err -> nur waehrend der An-Phase.
bool cellShowsBlock(ChStatus st, bool phaseOn);

// Nur die unquittierte Err-Aus-Phase zeigt einen Rahmen -- damit sie
// nicht mit "Ok" verwechselbar ist. Sobald quittiert, faellt der
// Rahmen weg (leiserer, aber weiterhin blinkender Zustand).
bool cellShowsFrame(ChStatus st, bool phaseOn, bool acked);

// Quittierter Err-Zustand zeigt zusaetzlich in BEIDEN Phasen eine
// gegenphasige Eckmarke -- das eigentliche Ack-Signal ("Blinken mit
// Marke = acked, ohne Marke = unacked").
bool cellShowsMark(ChStatus st, bool acked);

// Zeichnet EINE Statuszelle nach obigen Regeln. fillW ist bereits an
// die naechste Spaltengrenze geclippt (Aufrufer siehe
// invertFillWidth()). Bei Warn/Err-Block: fillRect (1 px Luft oben,
// Hoehe 8*TXT_SIZE_VAL+1) + Text in Hintergrundfarbe. Bei
// unquittiertem Err in der Aus-Phase: frameRect statt fillRect. Bei
// quittiertem Err: zusaetzliche 3x3-Eckmarke oben rechts, deren Farbe
// gegenphasig zu phaseOn ist (siehe cellShowsMark()).
void drawStatusCell(IDisplay& display, uint8_t x, uint8_t y, const char* text,
                     ChStatus st, bool phaseOn, bool acked, uint8_t fillW);
