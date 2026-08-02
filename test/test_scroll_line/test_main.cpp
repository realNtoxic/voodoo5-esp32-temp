// =============================================================
//  test_main.cpp — Unity-Tests fuer das Laufband (Zeile 5, rechts
//  vom Ambient-Segment): scrollCharX() (reine Positionsberechnung)
//  und drawScrollLine() (linksseitiges Clipping + nahtloser Umlauf)
//  ueber FakeDisplay.
// =============================================================
#include <unity.h>
#include "ScrollLine.h"
#include "FakeDisplay.h"
#include <cstdlib>

void setUp() {}
void tearDown() {}

static void test_scroll_char_x_offset_zero() {
  TEST_ASSERT_EQUAL_INT32(20, scrollCharX(0, 0, 6, 20));
  TEST_ASSERT_EQUAL_INT32(38, scrollCharX(3, 0, 6, 20));
}

static void test_scroll_char_x_positive_offset_moves_left() {
  TEST_ASSERT_EQUAL_INT32(14, scrollCharX(0, 6, 6, 20));
}

static void test_scroll_char_x_negative_offset_moves_right() {
  TEST_ASSERT_EQUAL_INT32(26, scrollCharX(0, -6, 6, 20));
}

// Zaehlt drawText-Aufrufe und prueft, dass ALLE x-Positionen >=
// startX liegen -- kein Zeichen darf links der Segmentgrenze landen.
static int countDrawTextCallsAndCheckNoLeftOfStart(const FakeDisplay& d, uint8_t startX) {
  int count = 0;
  for (const auto& call : d.calls) {
    if (call.rfind("drawText:", 0) != 0) {
      continue;
    }
    ++count;
    const int x = std::atoi(call.c_str() + 9);  // nach "drawText:"
    TEST_ASSERT_TRUE(x >= startX);
  }
  return count;
}

static void test_draw_scroll_line_clips_left_of_segment() {
  FakeDisplay display;
  // "ABCDE", glyphW=6 -> totalWidthPx=30. offsetPx=6, startX=50:
  // 1. Kopie: char0 landet bei 44 (< startX, muss geclippt werden).
  drawScrollLine(display, "ABCDE", 5, /*offsetPx=*/6, /*startX=*/50, /*y=*/53, /*screenWidth=*/128);

  const int drawn = countDrawTextCallsAndCheckNoLeftOfStart(display, 50);
  // 1. Kopie zeichnet 4 der 5 Zeichen (char0 geclippt), 2. Kopie alle 5.
  TEST_ASSERT_EQUAL_INT(9, drawn);

  // "A" (char0) darf nur EINMAL auftauchen -- aus der 2. Kopie bei
  // x=74, NICHT aus der 1. Kopie bei x=44 (die waere geclippt).
  int countA = 0;
  bool sawAAt74 = false;
  for (const auto& call : display.calls) {
    if (call.find(":A:1:0") != std::string::npos) {
      ++countA;
      if (call.rfind("drawText:74:", 0) == 0) {
        sawAAt74 = true;
      }
    }
  }
  TEST_ASSERT_EQUAL_INT(1, countA);
  TEST_ASSERT_TRUE(sawAAt74);
}

// Offsets, die sich um ein Vielfaches der Textbreite unterscheiden,
// muessen wegen des nahtlosen Umlaufs identische Ausgaben erzeugen.
static void test_draw_scroll_line_wraps_seamlessly() {
  FakeDisplay displayA;
  FakeDisplay displayB;
  // "ABCDE" -> totalWidthPx = 30. 34 mod 30 == 4.
  drawScrollLine(displayA, "ABCDE", 5, 4, 10, 53, 128);
  drawScrollLine(displayB, "ABCDE", 5, 34, 10, 53, 128);

  TEST_ASSERT_TRUE(displayA.calls == displayB.calls);
}

// Auch ein negativer Offset muss korrekt in [0, totalWidthPx)
// normalisiert werden (kein Absturz, konsistente Zeichenpositionen).
static void test_draw_scroll_line_negative_offset_wraps_consistently() {
  FakeDisplay displayA;
  FakeDisplay displayB;
  // -30 mod 30 == 0, also aequivalent zu Offset 0.
  drawScrollLine(displayA, "ABCDE", 5, 0, 10, 53, 128);
  drawScrollLine(displayB, "ABCDE", 5, -30, 10, 53, 128);

  TEST_ASSERT_TRUE(displayA.calls == displayB.calls);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_scroll_char_x_offset_zero);
  RUN_TEST(test_scroll_char_x_positive_offset_moves_left);
  RUN_TEST(test_scroll_char_x_negative_offset_moves_right);
  RUN_TEST(test_draw_scroll_line_clips_left_of_segment);
  RUN_TEST(test_draw_scroll_line_wraps_seamlessly);
  RUN_TEST(test_draw_scroll_line_negative_offset_wraps_consistently);
  return UNITY_END();
}
