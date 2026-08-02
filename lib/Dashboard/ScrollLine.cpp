#include "ScrollLine.h"
#include "config.h"

namespace {
constexpr uint8_t GLYPH_W = 6;  // Default-Font, Groesse 1, siehe TXT_SIZE_VAL

void drawCopy(IDisplay& display, const char* text, uint8_t textLen, int32_t offsetPx,
              uint8_t startX, uint8_t y, uint8_t screenWidth) {
  for (uint8_t i = 0; i < textLen; ++i) {
    const int32_t x = scrollCharX(i, offsetPx, GLYPH_W, startX);
    // Links vom Ambient-Segment und rechts vom Bildschirm geclippt --
    // kein Zeichen darf dort gezeichnet werden.
    if (x < static_cast<int32_t>(startX) || x >= static_cast<int32_t>(screenWidth)) {
      continue;
    }
    const char glyph[2] = { text[i], '\0' };
    display.drawText(static_cast<uint8_t>(x), y, glyph, TXT_SIZE_VAL, false);
  }
}

}  // namespace

int32_t scrollCharX(uint8_t charIndex, int32_t offsetPx, uint8_t glyphW, uint8_t startX) {
  return static_cast<int32_t>(startX) + static_cast<int32_t>(charIndex) * glyphW - offsetPx;
}

void drawScrollLine(IDisplay& display, const char* text, uint8_t textLen,
                     int32_t offsetPx, uint8_t startX, uint8_t y, uint8_t screenWidth) {
  if (textLen == 0) {
    return;
  }
  const int32_t totalWidthPx = static_cast<int32_t>(textLen) * GLYPH_W;

  // Offset in [0, totalWidthPx) normalisieren -- nahtloser Umlauf,
  // funktioniert auch fuer negative Offsets.
  int32_t normOffset = offsetPx % totalWidthPx;
  if (normOffset < 0) {
    normOffset += totalWidthPx;
  }

  drawCopy(display, text, textLen, normOffset, startX, y, screenWidth);
  // Zweite, um die Textbreite zurueckversetzte Kopie: sobald die erste
  // Kopie nach links herauslaeuft, folgt sie nahtlos.
  drawCopy(display, text, textLen, normOffset - totalWidthPx, startX, y, screenWidth);
}
