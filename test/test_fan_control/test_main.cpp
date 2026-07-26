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

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_ambient_panic_activates_at_warn_threshold);
  RUN_TEST(test_ambient_panic_hysteresis_prevents_pumping_at_threshold);
  RUN_TEST(test_effective_duty_is_max_of_curve_and_panic);
  return UNITY_END();
}
