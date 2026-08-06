#include "Dashboard.h"
#include "StatusCell.h"
#include "ScrollLine.h"
#include "config.h"
#include <cstdio>
#include <cstring>

namespace {

void formatTemp(char* out, size_t outSize, float tempC) {
  std::snprintf(out, outSize, "%.0f", static_cast<double>(tempC));
}

void formatRpm(char* out, size_t outSize, int16_t rpm) {
  if (rpm < 0) {
    std::snprintf(out, outSize, "-");
  } else {
    std::snprintf(out, outSize, "%d", rpm);
  }
}

const char* statusText(ChStatus st) {
  switch (st) {
    case ChStatus::Idle: return "idle";
    case ChStatus::Ok:   return "OK";
    case ChStatus::Warn: return "WARN";
    case ChStatus::Err:  return "ERR";
    case ChStatus::NA:   return "-";
    default:             return "?";
  }
}

}  // namespace

bool cellInverted(ChStatus st, uint8_t phase) {
  if (st == ChStatus::Warn) {
    return true;
  }
  if (st == ChStatus::Err) {
    return phase != 0;
  }
  return false;
}

char heartbeatChar(uint8_t phase) {
  return phase != 0 ? HEARTBEAT_A : HEARTBEAT_B;
}

uint8_t invertFillWidth(uint8_t col) {
  const uint8_t naturalWidth = CELL_W[col];
  uint8_t gap;
  if (col + 1 < 4) {
    gap = static_cast<uint8_t>(COL_X[col + 1] - COL_X[col]);
  } else {
    gap = static_cast<uint8_t>(OLED_WIDTH - COL_X[col]);
  }
  return naturalWidth < gap ? naturalWidth : gap;
}

void Dashboard::drawStatusCell(uint8_t col, ChStatus status, uint8_t phase) {
  const bool inv = cellInverted(status, phase);
  const uint8_t x = COL_X[col];
  const uint8_t y = ROW_Y[3];
  if (inv) {
    const uint8_t w = invertFillWidth(col);
    const uint8_t h = static_cast<uint8_t>(8 * TXT_SIZE_VAL + 1);
    display_.fillRect(x, static_cast<uint8_t>(y - 1), w, h);
  }
  display_.drawText(x, y, statusText(status), TXT_SIZE_VAL, inv);
}

void Dashboard::render(const DashboardData& d, uint8_t phase) {
  display_.clear();

  // Kopfzeile: Lebenszeichen (0;0) + Spaltenlabels.
  const char hb[2] = { heartbeatChar(phase), '\0' };
  display_.drawText(COL_X[0], ROW_Y[0], hb, TXT_SIZE_HEAD, false);
  display_.drawText(COL_X[1], ROW_Y[0], "#1", TXT_SIZE_HEAD, false);
  display_.drawText(COL_X[2], ROW_Y[0], "#2", TXT_SIZE_HEAD, false);
  display_.drawText(COL_X[3], ROW_Y[0], "#Amb", TXT_SIZE_HEAD, false);

  if (HEADER_UNDERLINE) {
    display_.hLine(static_cast<uint8_t>(ROW_Y[1] - 1));
  }

  // Temp-Zeile.
  char buf1[8];
  char buf2[8];
  char buf3[8];
  display_.drawText(COL_X[0], ROW_Y[1], "T C", TXT_SIZE_VAL, false);
  formatTemp(buf1, sizeof(buf1), d.vsa1.tempC);
  formatTemp(buf2, sizeof(buf2), d.vsa2.tempC);
  formatTemp(buf3, sizeof(buf3), d.amb.tempC);
  display_.drawText(COL_X[1], ROW_Y[1], buf1, TXT_SIZE_VAL, false);
  display_.drawText(COL_X[2], ROW_Y[1], buf2, TXT_SIZE_VAL, false);
  display_.drawText(COL_X[3], ROW_Y[1], buf3, TXT_SIZE_VAL, false);

  if (HSEP) {
    display_.hLine(static_cast<uint8_t>(ROW_Y[2] - 1));
  }

  // rpm-Zeile. Ambient hat keinen Luefter -> immer "-", unabhaengig
  // vom Status (auch wenn der jetzt Ok/Warn/Err sein kann).
  display_.drawText(COL_X[0], ROW_Y[2], "rpm", TXT_SIZE_VAL, false);
  formatRpm(buf1, sizeof(buf1), d.vsa1.rpm);
  formatRpm(buf2, sizeof(buf2), d.vsa2.rpm);
  display_.drawText(COL_X[1], ROW_Y[2], buf1, TXT_SIZE_VAL, false);
  display_.drawText(COL_X[2], ROW_Y[2], buf2, TXT_SIZE_VAL, false);
  display_.drawText(COL_X[3], ROW_Y[2], "-", TXT_SIZE_VAL, false);

  if (HSEP) {
    display_.hLine(static_cast<uint8_t>(ROW_Y[3] - 1));
  }

  // Status-Zeile: alle drei Kanaele -- auch Ambient -- ueber
  // cellInverted(), keine Sonderrolle mehr fuer Ambient.
  display_.drawText(COL_X[0], ROW_Y[3], "Sta", TXT_SIZE_VAL, false);
  drawStatusCell(1, d.vsa1.status, phase);
  drawStatusCell(2, d.vsa2.status, phase);
  drawStatusCell(3, d.amb.status, phase);

  if (VSEP) {
    for (uint8_t col = 0; col < 3; ++col) {
      const uint8_t gapStart = static_cast<uint8_t>(COL_X[col] + CELL_W[col]);
      const uint8_t gapEnd = COL_X[col + 1];
      display_.vLine(static_cast<uint8_t>((gapStart + gapEnd) / 2));
    }
  }

  if (SELFDIAG_ROW) {
    display_.drawText(0, SELFDIAG_Y, d.selfLine, TXT_SIZE_VAL, false);
  }

  display_.present();
}

namespace {

// Fuellbreite fuer eine Kanalspalte im finalen Layout, geclippt an die
// naechste Spaltengrenze bzw. den Bildschirmrand -- analog zu
// invertFillWidth() oben, aber fuer CH_COL_X/CH_CELL_W (5 Spalten:
// Label + #1..#4).
uint8_t chFillWidth(uint8_t col) {
  const uint8_t naturalWidth = CH_CELL_W[col];
  uint8_t gap;
  if (col + 1 < 5) {
    gap = static_cast<uint8_t>(CH_COL_X[col + 1] - CH_COL_X[col]);
  } else {
    gap = static_cast<uint8_t>(OLED_WIDTH - CH_COL_X[col]);
  }
  return naturalWidth < gap ? naturalWidth : gap;
}

}  // namespace

void Dashboard::renderFinal(const FinalDashboardData& d, bool phaseOn, int32_t scrollOffsetPx,
                             bool debugMode) {
  display_.clear();

  // Kopfzeile, Zelle 0;0: im Debug-Modus ein im Blink-Takt
  // invertierendes "D" statt des Lebenszeichens -- siehe config.h
  // DEBUG_SINGLE_CHANNEL. "Gross" heisst hier: die gesamte Kopfzelle
  // 0;0 wird als Block gefuellt (wie eine Statuszelle), NICHT ein
  // vergroesserter Font -- CH_ROW_Y[1]-CH_ROW_Y[0] sind nur 10 px,
  // ein groesserer Font wuerde in die Temp-Zeile darunter hineinlaufen
  // (siehe disptest-Layoutdefinition, aus der CH_ROW_Y/CH_COL_X/
  // CH_CELL_W stammen). Breite ueber chFillWidth() an die naechste
  // Spaltengrenze geclippt, exakt wie bei den Statuszellen unten. Row 0
  // hat keine Zeile darueber, deshalb hier kein "y - 1"-Polster wie bei
  // den Statuszellen.
  if (debugMode) {
    const uint8_t markW = chFillWidth(0);
    const uint8_t markH = static_cast<uint8_t>(8 * TXT_SIZE_HEAD + 1);
    if (phaseOn) {
      display_.fillRect(CH_COL_X[0], CH_ROW_Y[0], markW, markH);
    }
    display_.drawText(CH_COL_X[0], CH_ROW_Y[0], "D", TXT_SIZE_HEAD, phaseOn);
  } else {
    const char hb[2] = { phaseOn ? HEARTBEAT_A : HEARTBEAT_B, '\0' };
    display_.drawText(CH_COL_X[0], CH_ROW_Y[0], hb, TXT_SIZE_HEAD, false);
  }
  static const char* const kHeaders[4] = { "#1", "#2", "#3", "#4" };
  for (uint8_t i = 0; i < 4; ++i) {
    display_.drawText(CH_COL_X[i + 1], CH_ROW_Y[0], kHeaders[i], TXT_SIZE_HEAD, false);
  }

  if (CH_HEADER_UNDERLINE) {
    display_.hLine(static_cast<uint8_t>(CH_ROW_Y[1] - 1));
  }

  // Temp-Zeile.
  char buf[8];
  display_.drawText(CH_COL_X[0], CH_ROW_Y[1], "T", TXT_SIZE_VAL, false);
  for (uint8_t i = 0; i < 4; ++i) {
    formatTemp(buf, sizeof(buf), d.channels[i].tempC);
    display_.drawText(CH_COL_X[i + 1], CH_ROW_Y[1], buf, TXT_SIZE_VAL, false);
  }

  if (HSEP) {
    display_.hLine(static_cast<uint8_t>(CH_ROW_Y[2] - 1));
  }

  // rpm-Zeile.
  display_.drawText(CH_COL_X[0], CH_ROW_Y[2], "rpm", TXT_SIZE_VAL, false);
  for (uint8_t i = 0; i < 4; ++i) {
    formatRpm(buf, sizeof(buf), d.channels[i].rpm);
    display_.drawText(CH_COL_X[i + 1], CH_ROW_Y[2], buf, TXT_SIZE_VAL, false);
  }

  if (HSEP) {
    display_.hLine(static_cast<uint8_t>(CH_ROW_Y[3] - 1));
  }

  // Status-Zeile: gemeinsame Routine fuer alle vier Kanalspalten
  // (siehe StatusCell.h) -- kein doppelter Code zum Ambient-Segment
  // unten.
  display_.drawText(CH_COL_X[0], CH_ROW_Y[3], "Sta", TXT_SIZE_VAL, false);
  for (uint8_t i = 0; i < 4; ++i) {
    const ChannelStatus& ch = d.channels[i];
    ::drawStatusCell(display_, CH_COL_X[i + 1], CH_ROW_Y[3], statusText(ch.status),
                      ch.status, phaseOn, ch.acked, chFillWidth(i + 1));
  }

  if (CH_VSEP) {
    for (uint8_t col = 0; col < 4; ++col) {
      const uint8_t gapStart = static_cast<uint8_t>(CH_COL_X[col] + CH_CELL_W[col]);
      const uint8_t gapEnd = CH_COL_X[col + 1];
      display_.vLine(static_cast<uint8_t>((gapStart + gapEnd) / 2));
    }
  }

  // Zeile 5: Ambient-Segment fest links (dieselbe Statuszellen-Regel
  // wie die Kanalspalten), Laufband rechts daneben, links an der
  // Segmentgrenze geclippt.
  char ambBuf[12];
  std::snprintf(ambBuf, sizeof(ambBuf), "Amb:%.0f", static_cast<double>(d.ambient.tempC));
  const uint8_t ambW = static_cast<uint8_t>(std::strlen(ambBuf) * 6 * TXT_SIZE_VAL);
  ::drawStatusCell(display_, 0, CH_SELFDIAG_Y, ambBuf, d.ambient.status, phaseOn,
                    d.ambient.acked, ambW);

  const uint8_t scrollStartX = static_cast<uint8_t>(ambW + 4);
  drawScrollLine(display_, d.scrollText, d.scrollTextLen, scrollOffsetPx,
                 scrollStartX, CH_SELFDIAG_Y, OLED_WIDTH);

  display_.present();
}
