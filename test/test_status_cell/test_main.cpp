// =============================================================
//  test_main.cpp — Unity-Tests fuer die gemeinsame Status-Zell-
//  Routine (lib/Dashboard/StatusCell.h): cellShowsBlock/Frame/Mark
//  und drawStatusCell() ueber FakeDisplay.
// =============================================================
#include <unity.h>
#include "StatusCell.h"
#include "FakeDisplay.h"

void setUp() {}
void tearDown() {}

// --- cellShowsBlock: Warn immer, Err nur waehrend phaseOn ---------
static void test_cell_shows_block_rules() {
  TEST_ASSERT_TRUE(cellShowsBlock(ChStatus::Warn, true));
  TEST_ASSERT_TRUE(cellShowsBlock(ChStatus::Warn, false));
  TEST_ASSERT_TRUE(cellShowsBlock(ChStatus::Err, true));
  TEST_ASSERT_FALSE(cellShowsBlock(ChStatus::Err, false));
  TEST_ASSERT_FALSE(cellShowsBlock(ChStatus::Ok, true));
  TEST_ASSERT_FALSE(cellShowsBlock(ChStatus::Idle, true));
}

// --- cellShowsFrame: nur unquittierte Err-Aus-Phase ---------------
static void test_cell_shows_frame_rules() {
  TEST_ASSERT_TRUE(cellShowsFrame(ChStatus::Err, false, false));
  TEST_ASSERT_FALSE(cellShowsFrame(ChStatus::Err, false, true));   // acked -> kein Rahmen
  TEST_ASSERT_FALSE(cellShowsFrame(ChStatus::Err, true, false));   // phaseOn -> Block statt Rahmen
  TEST_ASSERT_FALSE(cellShowsFrame(ChStatus::Warn, false, false));
  TEST_ASSERT_FALSE(cellShowsFrame(ChStatus::Ok, false, false));
}

// --- cellShowsMark: nur quittierter Err, unabhaengig von phaseOn --
static void test_cell_shows_mark_rules() {
  TEST_ASSERT_TRUE(cellShowsMark(ChStatus::Err, true));
  TEST_ASSERT_FALSE(cellShowsMark(ChStatus::Err, false));
  TEST_ASSERT_FALSE(cellShowsMark(ChStatus::Warn, true));
  TEST_ASSERT_FALSE(cellShowsMark(ChStatus::Ok, true));
}

// --- drawStatusCell(): sichtbares Verhalten ueber FakeDisplay -----

static void test_draw_status_cell_ok_is_plain_text_only() {
  FakeDisplay display;
  drawStatusCell(display, 10, 20, "OK", ChStatus::Ok, true, false, 24);

  TEST_ASSERT_EQUAL_UINT32(1, display.calls.size());
  TEST_ASSERT_EQUAL_STRING("drawText:10:20:OK:1:0", display.calls[0].c_str());
}

static void test_draw_status_cell_warn_is_static_block_regardless_of_phase() {
  FakeDisplay displayOn;
  drawStatusCell(displayOn, 10, 20, "WARN", ChStatus::Warn, true, false, 24);
  FakeDisplay displayOff;
  drawStatusCell(displayOff, 10, 20, "WARN", ChStatus::Warn, false, false, 24);

  const std::vector<std::string> expected = {
    "fillRect:10:19:24:9",
    "drawText:10:20:WARN:1:1",
  };
  TEST_ASSERT_TRUE(displayOn.calls == expected);
  TEST_ASSERT_TRUE(displayOff.calls == expected);  // Warn blinkt nicht -- gleiches Bild in beiden Phasen
}

static void test_draw_status_cell_err_phase_on_is_block() {
  FakeDisplay display;
  drawStatusCell(display, 10, 20, "ERR", ChStatus::Err, true, false, 24);

  const std::vector<std::string> expected = {
    "fillRect:10:19:24:9",
    "drawText:10:20:ERR:1:1",
  };
  TEST_ASSERT_TRUE(display.calls == expected);
}

static void test_draw_status_cell_err_unacked_off_phase_shows_frame() {
  FakeDisplay display;
  drawStatusCell(display, 10, 20, "ERR", ChStatus::Err, false, false, 24);

  const std::vector<std::string> expected = {
    "frameRect:10:19:24:9",
    "drawText:10:20:ERR:1:0",
  };
  TEST_ASSERT_TRUE(display.calls == expected);
}

static void test_draw_status_cell_err_acked_off_phase_no_frame_but_mark() {
  FakeDisplay display;
  drawStatusCell(display, 10, 20, "ERR", ChStatus::Err, false, true, 24);

  // Kein frameRect (quittiert), stattdessen die helle Eckmarke
  // (fillRect 3x3) in der Aus-Phase.
  const std::vector<std::string> expected = {
    "drawText:10:20:ERR:1:0",
    "fillRect:31:19:3:3",
  };
  TEST_ASSERT_TRUE(display.calls == expected);
}

static void test_draw_status_cell_err_acked_on_phase_block_plus_dark_mark() {
  FakeDisplay display;
  drawStatusCell(display, 10, 20, "ERR", ChStatus::Err, true, true, 24);

  // Block wie gewohnt, zusaetzlich die dunkle Eckmarke (clearRect)
  // waehrend der An-Phase -- gegenphasig zur hellen Marke oben.
  const std::vector<std::string> expected = {
    "fillRect:10:19:24:9",
    "drawText:10:20:ERR:1:1",
    "clearRect:31:19:3:3",
  };
  TEST_ASSERT_TRUE(display.calls == expected);
}

// Ambient-Segment nutzt exakt dieselbe Routine wie eine Kanalzelle --
// gleiche Erwartungen, nur andere Koordinaten/Text.
static void test_ambient_segment_uses_identical_routine_as_channel_cell() {
  FakeDisplay display;
  drawStatusCell(display, 0, 53, "Amb:26", ChStatus::Err, false, true, 18);

  const std::vector<std::string> expected = {
    "drawText:0:53:Amb:26:1:0",
    "fillRect:15:52:3:3",  // markX = 0 + 18 - 3
  };
  TEST_ASSERT_TRUE(display.calls == expected);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_cell_shows_block_rules);
  RUN_TEST(test_cell_shows_frame_rules);
  RUN_TEST(test_cell_shows_mark_rules);
  RUN_TEST(test_draw_status_cell_ok_is_plain_text_only);
  RUN_TEST(test_draw_status_cell_warn_is_static_block_regardless_of_phase);
  RUN_TEST(test_draw_status_cell_err_phase_on_is_block);
  RUN_TEST(test_draw_status_cell_err_unacked_off_phase_shows_frame);
  RUN_TEST(test_draw_status_cell_err_acked_off_phase_no_frame_but_mark);
  RUN_TEST(test_draw_status_cell_err_acked_on_phase_block_plus_dark_mark);
  RUN_TEST(test_ambient_segment_uses_identical_routine_as_channel_cell);
  return UNITY_END();
}
