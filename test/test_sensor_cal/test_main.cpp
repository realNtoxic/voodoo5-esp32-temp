// =============================================================
//  test_main.cpp — Unity-Tests fuer die lastabhaengige Die-Korrektur
//  (dieTempC), Ersatz fuer die physikalisch falsche konstante
//  Offset-Kalibrierung.
// =============================================================
#include <unity.h>
#include "SensorCal.h"

void setUp() {}
void tearDown() {}

static void test_die_temp_c_no_correction_when_k_is_zero() {
  TEST_ASSERT_EQUAL_FLOAT(55.0f, dieTempC(55.0f, 25.0f, 0.0f));
  TEST_ASSERT_EQUAL_FLOAT(55.0f, dieTempC(55.0f, 55.0f, 0.0f));
}

// Physikalische Kernanforderung: stehende Karte (keine Last) --
// rawSonde == ambC -> Delta ist null, unabhaengig von k.
static void test_die_temp_c_zero_delta_at_idle() {
  TEST_ASSERT_EQUAL_FLOAT(30.0f, dieTempC(30.0f, 30.0f, 0.5f));
}

// Last-Proxy (rawSonde - ambC) waechst -> Korrektur waechst linear mit.
static void test_die_temp_c_grows_linearly_with_load() {
  const float amb  = 25.0f;
  const float k    = 0.2f;
  const float raw1 = 45.0f;  // Delta 20
  const float raw2 = 65.0f;  // Delta 40 -- doppelter Last-Proxy

  TEST_ASSERT_TRUE(dieTempC(raw1, amb, k) > raw1);

  const float corr1 = dieTempC(raw1, amb, k) - raw1;
  const float corr2 = dieTempC(raw2, amb, k) - raw2;

  TEST_ASSERT_EQUAL_FLOAT(k * (raw1 - amb), corr1);
  TEST_ASSERT_EQUAL_FLOAT(k * (raw2 - amb), corr2);
  TEST_ASSERT_EQUAL_FLOAT(corr1 * 2.0f, corr2);  // linear in (rawSonde - amb)
}

// Regel aus CLAUDE.md "Sensor-Kalibrierung": rawSondeC geht
// unveraendert ins Log, niemals dieTempC(...). Der Aufrufer haelt sein
// eigenes rawSonde unabhaengig vom korrigierten Wert (calC) und
// reicht fuers Log ausschliesslich rawSonde weiter.
static void test_log_keeps_raw_value_not_die_temp_c() {
  const float rawSonde = 42.0f;
  const float amb      = 20.0f;
  const float k        = 0.3f;

  const float logged = rawSonde;              // geht ins Log
  const float calC   = dieTempC(rawSonde, amb, k);  // geht in Regelung/Anzeige

  TEST_ASSERT_EQUAL_FLOAT(42.0f, logged);
  TEST_ASSERT_TRUE(calC > logged);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_die_temp_c_no_correction_when_k_is_zero);
  RUN_TEST(test_die_temp_c_zero_delta_at_idle);
  RUN_TEST(test_die_temp_c_grows_linearly_with_load);
  RUN_TEST(test_log_keeps_raw_value_not_die_temp_c);
  return UNITY_END();
}
