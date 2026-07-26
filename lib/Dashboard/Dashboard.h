// =============================================================
//  Dashboard.h — Kanal-Dashboard (4 Spalten x 4 Zeilen) + freie
//  SelfDiag-Zeile. Reine Logik, KEIN Arduino.h -> nativ testbar.
//  Zeichnet ausschliesslich ueber IDisplay (siehe IDisplay.h).
// =============================================================
#pragma once
#include <cstdint>
#include "IDisplay.h"

enum class ChStatus : uint8_t { Idle, Ok, Warn, Err, NA };

struct ChannelView {
  float    tempC;
  int16_t  rpm;    // -1 = keine Anzeige ("-"), z. B. Ambient
  ChStatus status;
};

struct DashboardData {
  ChannelView vsa1;
  ChannelView vsa2;
  ChannelView amb;
  char        selfLine[22];  // SelfDiag-Text, siehe SELFDIAG_ROW
};

// Reine, unabhaengig testbare Regeln (siehe CLAUDE.md "Dashboard").
bool cellInverted(ChStatus st, uint8_t phase);
char heartbeatChar(uint8_t phase);

// Clipped Fuellbreite fuer die Invert-Markierung einer Spalte: nie
// breiter als die Luecke zur naechsten Spalte (bzw. zum Bildschirmrand
// bei der letzten Spalte).
uint8_t invertFillWidth(uint8_t col);

class Dashboard {
public:
  explicit Dashboard(IDisplay& display) : display_(display) {}

  // `phase` kommt vom Hauptloop (Regelkreis-Takt), wird hier NICHT
  // per Timer erzeugt -- siehe CLAUDE.md "Dashboard-Anzeige".
  void render(const DashboardData& d, uint8_t phase);

private:
  void drawStatusCell(uint8_t col, ChStatus status, uint8_t phase);

  IDisplay& display_;
};
