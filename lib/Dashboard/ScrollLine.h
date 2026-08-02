// =============================================================
//  ScrollLine.h — Laufband fuer Zeile 5, rechts vom festen
//  Ambient-Segment (siehe CLAUDE.md "Dashboard-Anzeige"). Reine
//  Positions-Berechnung + Zeichnen ueber IDisplay, KEIN Arduino.h,
//  kein internes millis() -- der Scroll-Offset kommt von aussen
//  (Regelkreis-Takt, siehe config.h SCROLL_MS/SCROLL_STEP_PX).
// =============================================================
#pragma once
#include <cstdint>
#include "IDisplay.h"

// x-Position des Zeichens an charIndex bei gegebenem Scroll-Offset.
// glyphW = Zeichenbreite in Pixeln (6 bei Default-Font, Groesse 1).
// Rein arithmetisch, kein Clipping -- das Ergebnis kann links von
// startX oder rechts vom Bildschirm liegen, das entscheidet der
// Aufrufer (siehe drawScrollLine()).
int32_t scrollCharX(uint8_t charIndex, int32_t offsetPx, uint8_t glyphW, uint8_t startX);

// Zeichnet das Laufband zeichenweise: `text` (Laenge textLen) scrollt
// ab `startX` durch -- links davon wird NIE ein Zeichen gezeichnet,
// damit das Laufband nie ins feste Ambient-Segment laeuft. Nahtloser
// Umlauf: der Text wird intern ein zweites Mal um seine eigene Breite
// versetzt gezeichnet, damit beim Auslaufen der ersten Kopie nahtlos
// die naechste folgt. `screenWidth` begrenzt rechts (OLED_WIDTH).
void drawScrollLine(IDisplay& display, const char* text, uint8_t textLen,
                     int32_t offsetPx, uint8_t startX, uint8_t y, uint8_t screenWidth);
