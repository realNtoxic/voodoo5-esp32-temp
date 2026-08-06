// =============================================================
//  test_main.cpp — Unity-Tests fuer die Ambient-Panik-
//  Uebersteuerung (lib/FanControl).
// =============================================================
#include <unity.h>
#include "FanControl.h"
#include "config.h"

void setUp() {}
void tearDown() {}

static void test_ambient_panic_activates_at_warn_threshold() {
  TEST_ASSERT_FALSE(ambientPanicActive(AMB_WARN_C - 0.1f, false));
  TEST_ASSERT_TRUE(ambientPanicActive(AMB_WARN_C, false));
  TEST_ASSERT_TRUE(ambientPanicActive(AMB_WARN_C + 5.0f, false));
}

// Kernanforderung: Hysterese verhindert Pumpen genau an der Schwelle.
// Einmal aktiv, bleibt die Panik bestehen, bis die Temperatur unter
// AMB_WARN_OFF_C faellt -- nicht schon unter AMB_WARN_C.
static void test_ambient_panic_hysteresis_prevents_pumping_at_threshold() {
  const bool wasActive = true;
  TEST_ASSERT_TRUE(ambientPanicActive(AMB_WARN_C - 0.1f, wasActive));
  TEST_ASSERT_TRUE(ambientPanicActive(AMB_WARN_OFF_C + 0.1f, wasActive));
  TEST_ASSERT_FALSE(ambientPanicActive(AMB_WARN_OFF_C, wasActive));
  TEST_ASSERT_FALSE(ambientPanicActive(AMB_WARN_OFF_C - 5.0f, wasActive));
}

static void test_effective_duty_is_max_of_curve_and_panic() {
  TEST_ASSERT_EQUAL_UINT8(50, effectiveDuty(50, false));
  TEST_ASSERT_EQUAL_UINT8(CURVE.maxDuty, effectiveDuty(50, true));
  // Panik gewinnt nie gegen eine ohnehin schon hoehere Kennlinien-Duty
  // (kann bei maxDuty=100 nicht vorkommen, Grenzfall trotzdem geprueft).
  TEST_ASSERT_EQUAL_UINT8(CURVE.maxDuty, effectiveDuty(CURVE.maxDuty, true));
  TEST_ASSERT_EQUAL_UINT8(0, effectiveDuty(0, false));
}

// Testkonvention aus CLAUDE.md: "Hysterese: 42 C mit fanWasOn = false
// -> 0; mit true -> laufender Duty." Die 42 C aus dem Beispiel gehoeren
// zur damaligen Produktivkennlinie (Totband 40-45); hier bewusst als
// Mittelpunkt von offC/onC gerechnet, damit der Test unabhaengig von
// den tatsaechlich in config.h eingetragenen Kennlinienwerten
// (Produktiv- oder DEBUG-Kennlinie, siehe CURVE) gueltig bleibt.
static void test_curve_duty_hysteresis_matches_testkonvention() {
  const float midBand = (CURVE.offC + CURVE.onC) / 2.0f;
  TEST_ASSERT_EQUAL_UINT8(0, curveDuty(midBand, false));
  TEST_ASSERT_EQUAL_UINT8(CURVE.minDuty, curveDuty(midBand, true));
}

static void test_curve_duty_off_below_offC_regardless_of_fan_state() {
  const float belowOff = CURVE.offC - 5.0f;
  TEST_ASSERT_EQUAL_UINT8(0, curveDuty(belowOff, false));
  TEST_ASSERT_EQUAL_UINT8(0, curveDuty(belowOff, true));
}

static void test_curve_duty_ramps_linearly_between_on_and_full() {
  const float span = CURVE.fullC - CURVE.onC;
  const float t25 = CURVE.onC + 0.25f * span;
  const float t50 = CURVE.onC + 0.50f * span;
  const auto expectedDuty = [](float t) {
    return static_cast<uint8_t>(CURVE.minDuty + t * (CURVE.maxDuty - CURVE.minDuty));
  };

  TEST_ASSERT_EQUAL_UINT8(CURVE.minDuty, curveDuty(CURVE.onC, false));
  TEST_ASSERT_EQUAL_UINT8(expectedDuty(0.25f), curveDuty(t25, false));
  TEST_ASSERT_EQUAL_UINT8(expectedDuty(0.50f), curveDuty(t50, false));
  TEST_ASSERT_EQUAL_UINT8(CURVE.maxDuty, curveDuty(CURVE.fullC, false));
}

static void test_curve_duty_full_at_and_above_fullC() {
  TEST_ASSERT_EQUAL_UINT8(CURVE.maxDuty, curveDuty(CURVE.fullC, false));
  TEST_ASSERT_EQUAL_UINT8(CURVE.maxDuty, curveDuty(CURVE.fullC + 20.0f, false));
}

// Kernanforderung aus dem Regelmodell: 4 unabhaengige Regelkreise, kein
// Zonen-Mischen. curveDuty() ist pur/zustandslos zwischen Aufrufen --
// die Reihenfolge, in der die vier Kreise abgefragt werden, darf das
// Ergebnis eines einzelnen Kreises nicht beeinflussen.
static void test_four_independent_circuits_no_cross_coupling() {
  const float ch1Temp = CURVE.fullC + 20.0f;               // ueber fullC -> maxDuty
  const bool  ch2FanWasOn = true;
  const float ch2Temp = (CURVE.offC + CURVE.onC) / 2.0f;   // Totband, an -> minDuty
  const float ch3Temp = CURVE.offC - 5.0f;                 // unter offC -> 0
  const float ch4Temp = CURVE.onC + 0.50f * (CURVE.fullC - CURVE.onC);  // Rampe, t=0.5
  const uint8_t ch4Expected =
      static_cast<uint8_t>(CURVE.minDuty + 0.50f * (CURVE.maxDuty - CURVE.minDuty));

  const uint8_t ch1 = curveDuty(ch1Temp, false);
  const uint8_t ch2 = curveDuty(ch2Temp, ch2FanWasOn);
  const uint8_t ch3 = curveDuty(ch3Temp, false);
  const uint8_t ch4 = curveDuty(ch4Temp, false);

  // Dieselben vier Aufrufe in umgekehrter Reihenfolge muessen exakt
  // dieselben Einzelergebnisse liefern.
  const uint8_t ch4Again = curveDuty(ch4Temp, false);
  const uint8_t ch3Again = curveDuty(ch3Temp, false);
  const uint8_t ch2Again = curveDuty(ch2Temp, ch2FanWasOn);
  const uint8_t ch1Again = curveDuty(ch1Temp, false);

  TEST_ASSERT_EQUAL_UINT8(CURVE.maxDuty, ch1);
  TEST_ASSERT_EQUAL_UINT8(CURVE.minDuty, ch2);
  TEST_ASSERT_EQUAL_UINT8(0, ch3);
  TEST_ASSERT_EQUAL_UINT8(ch4Expected, ch4);

  TEST_ASSERT_EQUAL_UINT8(ch1, ch1Again);
  TEST_ASSERT_EQUAL_UINT8(ch2, ch2Again);
  TEST_ASSERT_EQUAL_UINT8(ch3, ch3Again);
  TEST_ASSERT_EQUAL_UINT8(ch4, ch4Again);
}

// Fail-Safe pro Kreis (nicht mehr "der andere Luefter" wie in der alten
// 2-Kanal-Logik): faellt Sensor k aus, geht NUR Luefter k auf 100 %.
static void test_channel_failsafe_affects_only_its_own_channel() {
  const uint8_t curveDutyPerChannel[4] = { 20, 45, 60, 0 };
  bool sensorOk[4] = { true, true, false, true };  // Kreis 2 (Index 2) faellt aus

  uint8_t result[4];
  for (uint8_t i = 0; i < 4; ++i) {
    result[i] = channelFailSafeDuty(curveDutyPerChannel[i], sensorOk[i]);
  }

  TEST_ASSERT_EQUAL_UINT8(curveDutyPerChannel[0], result[0]);
  TEST_ASSERT_EQUAL_UINT8(curveDutyPerChannel[1], result[1]);
  TEST_ASSERT_EQUAL_UINT8(CURVE.maxDuty, result[2]);  // ausgefallener Kreis -> 100 %
  TEST_ASSERT_EQUAL_UINT8(curveDutyPerChannel[3], result[3]);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_ambient_panic_activates_at_warn_threshold);
  RUN_TEST(test_ambient_panic_hysteresis_prevents_pumping_at_threshold);
  RUN_TEST(test_effective_duty_is_max_of_curve_and_panic);
  RUN_TEST(test_curve_duty_hysteresis_matches_testkonvention);
  RUN_TEST(test_curve_duty_off_below_offC_regardless_of_fan_state);
  RUN_TEST(test_curve_duty_ramps_linearly_between_on_and_full);
  RUN_TEST(test_curve_duty_full_at_and_above_fullC);
  RUN_TEST(test_four_independent_circuits_no_cross_coupling);
  RUN_TEST(test_channel_failsafe_affects_only_its_own_channel);
  return UNITY_END();
}
