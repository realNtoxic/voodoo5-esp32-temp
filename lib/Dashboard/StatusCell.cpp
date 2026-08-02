#include "StatusCell.h"
#include "config.h"

namespace {
constexpr uint8_t MARK_SIZE_PX = 3;
}

bool cellShowsBlock(ChStatus st, bool phaseOn) {
  return st == ChStatus::Warn || (st == ChStatus::Err && phaseOn);
}

bool cellShowsFrame(ChStatus st, bool phaseOn, bool acked) {
  return st == ChStatus::Err && !phaseOn && !acked;
}

bool cellShowsMark(ChStatus st, bool acked) {
  return st == ChStatus::Err && acked;
}

void drawStatusCell(IDisplay& display, uint8_t x, uint8_t y, const char* text,
                     ChStatus st, bool phaseOn, bool acked, uint8_t fillW) {
  const bool block = cellShowsBlock(st, phaseOn);
  const bool frame = cellShowsFrame(st, phaseOn, acked);
  const bool mark = cellShowsMark(st, acked);
  const uint8_t cellY = static_cast<uint8_t>(y - 1);
  const uint8_t cellH = static_cast<uint8_t>(8 * TXT_SIZE_VAL + 1);

  if (block) {
    display.fillRect(x, cellY, fillW, cellH);
  } else if (frame) {
    display.frameRect(x, cellY, fillW, cellH);
  }

  display.drawText(x, y, text, TXT_SIZE_VAL, block);

  if (mark) {
    // Eckmarke oben rechts, gegenphasig zu phaseOn -- so bleibt sie in
    // BEIDEN Phasen sichtbar: waehrend des Blocks (heller Hintergrund)
    // als dunkler Punkt (clearRect), sonst als heller Punkt (fillRect).
    const uint8_t markX = static_cast<uint8_t>(x + fillW - MARK_SIZE_PX);
    if (phaseOn) {
      display.clearRect(markX, cellY, MARK_SIZE_PX, MARK_SIZE_PX);
    } else {
      display.fillRect(markX, cellY, MARK_SIZE_PX, MARK_SIZE_PX);
    }
  }
}
