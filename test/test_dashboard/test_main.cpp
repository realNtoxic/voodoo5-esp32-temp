// =============================================================
//  test_main.cpp — Unity-Tests fuer das Dashboard (Layout, Invert-
//  Regeln, Lebenszeichen, Render-Reihenfolge).
// =============================================================
#include <cstdio>
#include <string>
#include <vector>
#include <unity.h>
#include "Dashboard.h"
#include "FakeDisplay.h"
#include "config.h"

void setUp() {}
void tearDown() {}

// ---------------------------------------------------------------
//  Reine Regeln
// ---------------------------------------------------------------

static void test_cell_inverted_rules() {
  TEST_ASSERT_TRUE(cellInverted(ChStatus::Warn, 0));
  TEST_ASSERT_TRUE(cellInverted(ChStatus::Warn, 1));
  TEST_ASSERT_FALSE(cellInverted(ChStatus::Err, 0));
  TEST_ASSERT_TRUE(cellInverted(ChStatus::Err, 1));
  TEST_ASSERT_FALSE(cellInverted(ChStatus::Ok, 0));
  TEST_ASSERT_FALSE(cellInverted(ChStatus::Ok, 1));
  TEST_ASSERT_FALSE(cellInverted(ChStatus::Idle, 0));
  TEST_ASSERT_FALSE(cellInverted(ChStatus::Idle, 1));
}

static void test_heartbeat_char_rules() {
  TEST_ASSERT_EQUAL_INT8(HEARTBEAT_B, heartbeatChar(0));
  TEST_ASSERT_EQUAL_INT8(HEARTBEAT_A, heartbeatChar(1));
}

static void test_invert_fill_width_never_exceeds_gap() {
  for (uint8_t col = 0; col < 4; ++col) {
    const uint8_t gap = (col + 1 < 4)
        ? static_cast<uint8_t>(COL_X[col + 1] - COL_X[col])
        : static_cast<uint8_t>(OLED_WIDTH - COL_X[col]);
    TEST_ASSERT_TRUE(invertFillWidth(col) <= gap);
    TEST_ASSERT_TRUE(invertFillWidth(col) <= CELL_W[col]);
  }
}

// ---------------------------------------------------------------
//  render()
// ---------------------------------------------------------------

static DashboardData allOkData() {
  DashboardData d{};
  d.vsa1 = { 30.0f, 0, ChStatus::Ok };
  d.vsa2 = { 35.0f, 150, ChStatus::Ok };
  d.amb  = { 20.0f, -1, ChStatus::Ok };
  std::snprintf(d.selfLine, sizeof(d.selfLine), "Heap 100k 50Hz");
  return d;
}

static void test_full_render_all_ok_phase0_exact_sequence() {
  FakeDisplay fd;
  Dashboard dash(fd);
  const DashboardData d = allOkData();

  dash.render(d, 0);

  const std::vector<std::string> expected = {
    "clear",
    "drawText:0:0:-:1:0",
    "drawText:27:0:#1:1:0",
    "drawText:54:0:#2:1:0",
    "drawText:81:0:#Amb:1:0",
    "hLine:12",
    "drawText:0:13:T C:1:0",
    "drawText:27:13:30:1:0",
    "drawText:54:13:35:1:0",
    "drawText:81:13:20:1:0",
    "hLine:24",
    "drawText:0:25:rpm:1:0",
    "drawText:27:25:0:1:0",
    "drawText:54:25:150:1:0",
    "drawText:81:25:-:1:0",
    "hLine:36",
    "drawText:0:37:Sta:1:0",
    "drawText:27:37:OK:1:0",
    "drawText:54:37:OK:1:0",
    "drawText:81:37:OK:1:0",
    "drawText:0:56:Heap 100k 50Hz:1:0",
    "present",
  };

  TEST_ASSERT_EQUAL_UINT(expected.size(), fd.calls.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    TEST_ASSERT_EQUAL_STRING(expected[i].c_str(), fd.calls[i].c_str());
  }
}

// Kernanforderung aus dem Ambient-Umbau: Der Status ist nicht mehr fest
// NA, sondern kann wie bei VSA1/VSA2 Warn/Err werden -- die rpm-Zelle
// bleibt trotzdem immer "-", weil Ambient keinen Luefter hat.
static void test_ambient_warn_is_rendered_inverted_rpm_stays_dash() {
  FakeDisplay fd;
  Dashboard dash(fd);
  DashboardData d = allOkData();
  d.amb.status = ChStatus::Warn;

  dash.render(d, 0);

  bool foundRpmDash = false;
  bool foundFillBeforeStatus = false;
  bool foundInvertedStatus = false;
  for (size_t i = 0; i < fd.calls.size(); ++i) {
    if (fd.calls[i] == "drawText:81:25:-:1:0") {
      foundRpmDash = true;
    }
    if (fd.calls[i] == "fillRect:81:36:24:9" && i + 1 < fd.calls.size() &&
        fd.calls[i + 1] == "drawText:81:37:WARN:1:1") {
      foundFillBeforeStatus = true;
      foundInvertedStatus = true;
    }
  }
  TEST_ASSERT_TRUE(foundRpmDash);
  TEST_ASSERT_TRUE(foundFillBeforeStatus);
  TEST_ASSERT_TRUE(foundInvertedStatus);
}

// Err blinkt im Takt von `phase`: bei phase 0 normal, bei phase 1 invertiert.
static void test_ambient_err_blinks_with_phase() {
  FakeDisplay fdPhase0;
  Dashboard dashPhase0(fdPhase0);
  DashboardData d = allOkData();
  d.amb.status = ChStatus::Err;

  dashPhase0.render(d, 0);
  bool hasInvertedAtPhase0 = false;
  for (const auto& call : fdPhase0.calls) {
    if (call == "drawText:81:37:ERR:1:1") {
      hasInvertedAtPhase0 = true;
    }
  }
  TEST_ASSERT_FALSE(hasInvertedAtPhase0);

  FakeDisplay fdPhase1;
  Dashboard dashPhase1(fdPhase1);
  dashPhase1.render(d, 1);
  bool hasInvertedAtPhase1 = false;
  bool hasFillAtPhase1 = false;
  for (size_t i = 0; i < fdPhase1.calls.size(); ++i) {
    if (fdPhase1.calls[i] == "fillRect:81:36:24:9") {
      hasFillAtPhase1 = true;
    }
    if (fdPhase1.calls[i] == "drawText:81:37:ERR:1:1") {
      hasInvertedAtPhase1 = true;
    }
  }
  TEST_ASSERT_TRUE(hasFillAtPhase1);
  TEST_ASSERT_TRUE(hasInvertedAtPhase1);
}

static void test_selfdiag_row_drawn_as_free_line_at_x0() {
  FakeDisplay fd;
  Dashboard dash(fd);
  const DashboardData d = allOkData();

  dash.render(d, 0);

  const std::string expected =
      std::string("drawText:0:") + std::to_string(SELFDIAG_Y) + ":" + d.selfLine + ":1:0";
  bool found = false;
  for (const auto& call : fd.calls) {
    if (call == expected) {
      found = true;
    }
  }
  TEST_ASSERT_TRUE(found);
}

// ---------------------------------------------------------------
//  renderFinal() -- finales 4-Kanal-Layout, additiv neben render().
//  Aendert nichts an obigen render()-Tests, siehe Dashboard.h.
// ---------------------------------------------------------------

static FinalDashboardData allOkFinalData() {
  FinalDashboardData d{};
  d.channels[0] = { 41.0f, 0,    ChStatus::Ok, false };
  d.channels[1] = { 44.0f, 3090, ChStatus::Ok, false };
  d.channels[2] = { 40.0f, 2800, ChStatus::Ok, false };
  d.channels[3] = { 43.0f, 0,    ChStatus::Ok, false };
  d.ambient     = { 26.0f, -1,   ChStatus::Ok, false };
  d.scrollText = "Hi";
  d.scrollTextLen = 2;
  return d;
}

static void test_render_final_all_ok_phase_off_exact_sequence() {
  FakeDisplay fd;
  Dashboard dash(fd);
  const FinalDashboardData d = allOkFinalData();

  dash.renderFinal(d, /*phaseOn=*/false, /*scrollOffsetPx=*/0, /*debugMode=*/false);

  const std::vector<std::string> expected = {
    "clear",
    "drawText:0:0:-:1:0",
    "drawText:21:0:#1:1:0",
    "drawText:48:0:#2:1:0",
    "drawText:75:0:#3:1:0",
    "drawText:102:0:#4:1:0",
    "drawText:0:10:T:1:0",
    "drawText:21:10:41:1:0",
    "drawText:48:10:44:1:0",
    "drawText:75:10:40:1:0",
    "drawText:102:10:43:1:0",
    "hLine:21",
    "drawText:0:22:rpm:1:0",
    "drawText:21:22:0:1:0",
    "drawText:48:22:3090:1:0",
    "drawText:75:22:2800:1:0",
    "drawText:102:22:0:1:0",
    "hLine:33",
    "drawText:0:34:Sta:1:0",
    "drawText:21:34:OK:1:0",
    "drawText:48:34:OK:1:0",
    "drawText:75:34:OK:1:0",
    "drawText:102:34:OK:1:0",
    "vLine:19:42",
    "vLine:46:42",
    "vLine:73:42",
    "vLine:100:42",
    "drawText:0:53:Amb:26:1:0",
    "drawText:40:53:H:1:0",
    "drawText:46:53:i:1:0",
    "drawText:52:53:H:1:0",
    "drawText:58:53:i:1:0",
    "present",
  };

  TEST_ASSERT_EQUAL_UINT(expected.size(), fd.calls.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    TEST_ASSERT_EQUAL_STRING(expected[i].c_str(), fd.calls[i].c_str());
  }
}

// Kanal #3 (Index 2) mit unquittiertem Err in der Aus-Phase muss den
// Rahmen zeigen -- dieselbe StatusCell-Routine wie in test_status_cell,
// hier als Nachweis, dass renderFinal() sie tatsaechlich benutzt.
static void test_render_final_unacked_err_channel_shows_frame() {
  FakeDisplay fd;
  Dashboard dash(fd);
  FinalDashboardData d = allOkFinalData();
  d.channels[2].status = ChStatus::Err;
  d.channels[2].acked = false;

  dash.renderFinal(d, /*phaseOn=*/false, /*scrollOffsetPx=*/0, /*debugMode=*/false);

  bool hasFrame = false;
  for (size_t i = 0; i + 1 < fd.calls.size(); ++i) {
    if (fd.calls[i] == "frameRect:75:33:24:9" && fd.calls[i + 1] == "drawText:75:34:ERR:1:0") {
      hasFrame = true;
    }
  }
  TEST_ASSERT_TRUE(hasFrame);
}

// Takt kommt ausschliesslich aus phaseOn/scrollOffsetPx -- kein
// interner Timer: gleiche Eingaben muessen bei zwei Aufrufen exakt
// dieselbe Ausgabe erzeugen.
static void test_render_final_is_deterministic_for_same_inputs() {
  const FinalDashboardData d = allOkFinalData();

  FakeDisplay fdFirst;
  Dashboard dashFirst(fdFirst);
  dashFirst.renderFinal(d, true, 17, false);

  FakeDisplay fdSecond;
  Dashboard dashSecond(fdSecond);
  dashSecond.renderFinal(d, true, 17, false);

  TEST_ASSERT_TRUE(fdFirst.calls == fdSecond.calls);
}

// Debug-Modus (siehe config.h DEBUG_SINGLE_CHANNEL): Zelle 0;0 zeigt
// ein "D" statt des Lebenszeichens, invertiert im Blink-Takt. "Gross"
// heisst: die ganze Kopfzelle wird als Block gefuellt (Breite ueber
// chFillWidth() an CH_CELL_W[0]=18 geclippt, Hoehe 8*TXT_SIZE_HEAD+1=9,
// beides innerhalb der 10 px, die CH_ROW_Y[1]-CH_ROW_Y[0] hergeben) --
// NICHT ein vergroesserter Font, der in die Temp-Zeile darunter liefe.
static void test_render_final_debug_mode_shows_inverted_d_in_phase() {
  const FinalDashboardData d = allOkFinalData();

  FakeDisplay fdOn;
  Dashboard dashOn(fdOn);
  dashOn.renderFinal(d, /*phaseOn=*/true, 0, /*debugMode=*/true);
  TEST_ASSERT_EQUAL_STRING("clear", fdOn.calls[0].c_str());
  TEST_ASSERT_EQUAL_STRING("fillRect:0:0:18:9", fdOn.calls[1].c_str());
  TEST_ASSERT_EQUAL_STRING("drawText:0:0:D:1:1", fdOn.calls[2].c_str());

  FakeDisplay fdOff;
  Dashboard dashOff(fdOff);
  dashOff.renderFinal(d, /*phaseOn=*/false, 0, /*debugMode=*/true);
  TEST_ASSERT_EQUAL_STRING("clear", fdOff.calls[0].c_str());
  TEST_ASSERT_EQUAL_STRING("drawText:0:0:D:1:0", fdOff.calls[1].c_str());
}

// Debug-Modus aendert nichts ausserhalb von Zelle 0;0 -- derselbe
// Kanalinhalt wie im Nicht-Debug-Fall (nur Zeilen 0-2 hier verglichen,
// der Rest ist identisch zur exakten Sequenz oben).
static void test_render_final_debug_mode_off_matches_heartbeat() {
  const FinalDashboardData d = allOkFinalData();

  FakeDisplay fd;
  Dashboard dash(fd);
  dash.renderFinal(d, /*phaseOn=*/false, 0, /*debugMode=*/false);

  TEST_ASSERT_EQUAL_STRING("drawText:0:0:-:1:0", fd.calls[1].c_str());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_cell_inverted_rules);
  RUN_TEST(test_heartbeat_char_rules);
  RUN_TEST(test_invert_fill_width_never_exceeds_gap);
  RUN_TEST(test_full_render_all_ok_phase0_exact_sequence);
  RUN_TEST(test_ambient_warn_is_rendered_inverted_rpm_stays_dash);
  RUN_TEST(test_ambient_err_blinks_with_phase);
  RUN_TEST(test_selfdiag_row_drawn_as_free_line_at_x0);
  RUN_TEST(test_render_final_all_ok_phase_off_exact_sequence);
  RUN_TEST(test_render_final_unacked_err_channel_shows_frame);
  RUN_TEST(test_render_final_is_deterministic_for_same_inputs);
  RUN_TEST(test_render_final_debug_mode_shows_inverted_d_in_phase);
  RUN_TEST(test_render_final_debug_mode_off_matches_heartbeat);
  return UNITY_END();
}
