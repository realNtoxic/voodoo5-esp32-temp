// =============================================================
//  test_main.cpp — Unity-Tests fuer die Sensor-Offset-Kalibrierung.
// =============================================================
#include <unity.h>
#include "SensorCal.h"
#include "config.h"

void setUp() {}
void tearDown() {}

static void test_apply_sensor_offset_adds_configured_delta() {
  const float raw = 55.0f;
  for (uint8_t i = 0; i < 5; ++i) {
    const float expected = raw + SENSOR_OFFSET_C[i];
    TEST_ASSERT_EQUAL_FLOAT(expected, applySensorOffset(raw, i));
  }
}

// Regel aus CLAUDE.md "Sensor-Kalibrierung": rawC geht unveraendert ins
// Log, calC nur in Regelung/Anzeige. applySensorOffset() darf den
// uebergebenen Rohwert nicht veraendern -- der Aufrufer muss ihn
// unabhaengig fuers Log weiterreichen koennen.
static void test_apply_sensor_offset_does_not_mutate_raw_value() {
  float raw = 42.0f;
  const float cal = applySensorOffset(raw, 0);
  TEST_ASSERT_EQUAL_FLOAT(42.0f, raw);
  (void)cal;
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_apply_sensor_offset_adds_configured_delta);
  RUN_TEST(test_apply_sensor_offset_does_not_mutate_raw_value);
  return UNITY_END();
}
