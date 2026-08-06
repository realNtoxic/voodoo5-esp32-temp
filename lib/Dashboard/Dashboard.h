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

// --- Finales 4-Kanal-Layout (Zusatz, siehe CLAUDE.md "Dashboard-
// Anzeige") -- additiv neben DashboardData/render() oben, die
// unveraendert das bestehende 3-Spalten-Layout bedienen. ---
struct ChannelStatus {
  float    tempC;
  int16_t  rpm;    // -1 = "-"; vom Ambient-Segment ungenutzt
  ChStatus status;
  bool     acked;  // nur relevant, wenn status == ChStatus::Err
};

struct FinalDashboardData {
  ChannelStatus channels[4];  // #1..#4
  ChannelStatus ambient;      // Segment in Zeile 5, rpm wird nicht gezeichnet
  const char*   scrollText;   // vorformatierter Laufband-Text, siehe ScrollLine.h
  uint8_t       scrollTextLen;
};

class Dashboard {
public:
  explicit Dashboard(IDisplay& display) : display_(display) {}

  // `phase` kommt vom Hauptloop (Regelkreis-Takt), wird hier NICHT
  // per Timer erzeugt -- siehe CLAUDE.md "Dashboard-Anzeige".
  void render(const DashboardData& d, uint8_t phase);

  // Finales 4-Kanal-Layout: Kanalspalten #1..#4 + Ambient-Segment/
  // Laufband in Zeile 5. `phaseOn`/`scrollOffsetPx` kommen ebenfalls
  // aus dem Regelkreis, kein eigener Display-Timer (siehe CLAUDE.md).
  // Additiv neben render() -- aendert nichts an dessen Verhalten.
  //
  // `debugMode` (siehe config.h DEBUG_SINGLE_CHANNEL): zeigt statt des
  // Lebenszeichens ein grosses, im Blink-Takt invertierendes "D" in
  // Zelle 0;0 -- unuebersehbarer Hinweis, dass nur ein Teil der Kanaele
  // real bestueckt ist und der Rest bewusst als Idle-Platzhalter laeuft.
  void renderFinal(const FinalDashboardData& d, bool phaseOn, int32_t scrollOffsetPx,
                    bool debugMode);

private:
  void drawStatusCell(uint8_t col, ChStatus status, uint8_t phase);

  IDisplay& display_;
};
