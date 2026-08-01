// =============================================================
//  test_main.cpp — Unity-Tests fuer die LED-Musterlogik
//  (lib/Led/LedPattern) und den FakeLed-Mitschnitt.
//
//  Grenze (siehe CLAUDE.md "Testkonventionen"): geprueft wird die
//  MUSTER-LOGIK, nicht ob die LED physisch leuchtet. LED ist wie der
//  Speaker fire-and-forget und in Software nicht verifizierbar --
//  das bleibt Sichtpruefung am Board.
// =============================================================
#include <unity.h>
#include "LedPattern.h"
#include "FakeLed.h"
#include "config.h"

void setUp() {}
void tearDown() {}

// Heartbeat folgt ausschliesslich der Regelkreis-`phase`, kein
// eigener Timer -- nowMs darf das Ergebnis nicht beeinflussen.
static void test_heartbeat_follows_phase_not_time() {
  TEST_ASSERT_FALSE(ledLevel(LedMode::Heartbeat, 0, 0));
  TEST_ASSERT_FALSE(ledLevel(LedMode::Heartbeat, 0, 999999));
  TEST_ASSERT_TRUE(ledLevel(LedMode::Heartbeat, 1, 0));
  TEST_ASSERT_TRUE(ledLevel(LedMode::Heartbeat, 1, 999999));
}

// Fault: Doppelblink an/aus/an/aus(lang) ueber LED_FAULT_* -- phase
// darf hier keine Rolle spielen, nur nowMs.
static void test_fault_double_blink_pattern_matches_config() {
  const uint32_t onEnd    = LED_FAULT_ON_MS;
  const uint32_t gapEnd   = onEnd + LED_FAULT_GAP_MS;
  const uint32_t onEnd2   = gapEnd + LED_FAULT_ON_MS;
  const uint32_t periodMs = onEnd2 + LED_FAULT_PAUSE_MS;

  TEST_ASSERT_TRUE(ledLevel(LedMode::Fault, 0, 0));                   // 1. Puls an
  TEST_ASSERT_TRUE(ledLevel(LedMode::Fault, 1, onEnd - 1));           // kurz vor Ende 1. Puls
  TEST_ASSERT_FALSE(ledLevel(LedMode::Fault, 0, onEnd));              // Luecke
  TEST_ASSERT_FALSE(ledLevel(LedMode::Fault, 1, gapEnd - 1));
  TEST_ASSERT_TRUE(ledLevel(LedMode::Fault, 0, gapEnd));              // 2. Puls an
  TEST_ASSERT_TRUE(ledLevel(LedMode::Fault, 1, onEnd2 - 1));
  TEST_ASSERT_FALSE(ledLevel(LedMode::Fault, 0, onEnd2));             // lange Pause
  TEST_ASSERT_FALSE(ledLevel(LedMode::Fault, 1, periodMs - 1));

  // Nach einer vollen Periode wiederholt sich das Muster.
  TEST_ASSERT_TRUE(ledLevel(LedMode::Fault, 0, periodMs));
  TEST_ASSERT_TRUE(ledLevel(LedMode::Fault, 0, periodMs + onEnd - 1));
}

// Fault muss ueber ein volles Fenster hinweg klar vom ruhigen
// Heartbeat unterscheidbar sein: mindestens ein Wechsel mehr als der
// einfache Ein/Aus-Takt des Heartbeats.
static void test_fault_pattern_differs_from_heartbeat() {
  bool sawOn = false;
  bool sawOff = false;
  int transitions = 0;
  bool prev = ledLevel(LedMode::Fault, 0, 0);
  for (uint32_t t = 1; t < 1200; ++t) {
    const bool cur = ledLevel(LedMode::Fault, 0, t);
    if (cur) sawOn = true; else sawOff = true;
    if (cur != prev) ++transitions;
    prev = cur;
  }
  TEST_ASSERT_TRUE(sawOn);
  TEST_ASSERT_TRUE(sawOff);
  // Doppelblink + Pause -> mindestens 3 Flanken pro Periode (an->aus,
  // aus->an, an->aus), ein einfacher Heartbeat-Takt haette hoechstens 2.
  TEST_ASSERT_GREATER_OR_EQUAL(3, transitions);
}

static void test_select_led_mode_from_fault_latch() {
  TEST_ASSERT_TRUE(selectLedMode(true) == LedMode::Fault);
  TEST_ASSERT_TRUE(selectLedMode(false) == LedMode::Heartbeat);
}

// FakeLed protokolliert die set()-Aufrufe mit Zeitstempel -- Test
// simuliert eine kleine Loop-Sequenz und prueft die Sequenz danach
// als Ganzes.
static void test_fake_led_records_generated_sequence() {
  FakeLed led;
  const uint32_t samples[] = { 0, LED_FAULT_ON_MS, 999999 };
  const LedMode  mode = selectLedMode(true);  // Fehler aktiv -> Fault

  for (uint32_t t : samples) {
    led.simulatedNowMs = t;
    led.set(ledLevel(mode, 0, t));
  }

  TEST_ASSERT_EQUAL_UINT32(3, led.calls.size());
  TEST_ASSERT_TRUE(led.calls[0].level);   // t=0: 1. Puls an
  TEST_ASSERT_EQUAL_UINT32(0, led.calls[0].atMs);
  TEST_ASSERT_FALSE(led.calls[1].level);  // t=LED_FAULT_ON_MS: Luecke
  TEST_ASSERT_EQUAL_UINT32(LED_FAULT_ON_MS, led.calls[1].atMs);
  TEST_ASSERT_EQUAL_UINT32(999999, led.calls[2].atMs);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_heartbeat_follows_phase_not_time);
  RUN_TEST(test_fault_double_blink_pattern_matches_config);
  RUN_TEST(test_fault_pattern_differs_from_heartbeat);
  RUN_TEST(test_select_led_mode_from_fault_latch);
  RUN_TEST(test_fake_led_records_generated_sequence);
  return UNITY_END();
}
