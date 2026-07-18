// =============================================================
//  test_main.cpp — Unity-Tests fuer die OLED-Stufe des
//  Boot-Selbsttests (Inbetriebnahme-Schritt 1+2: Speaker, OLED).
// =============================================================
#include <string>
#include <vector>
#include <unity.h>
#include "BootSelfTest.h"
#include "FakeHal.h"
#include "config.h"

void setUp() {}
void tearDown() {}

static void test_oled_ok_only_init_no_error_beep() {
  FakeHal hal;
  hal.oledInitResult = true;

  const bool ok = selfTestOled(hal);

  TEST_ASSERT_TRUE(ok);
  const std::vector<std::string> expected = { "oledInit" };
  TEST_ASSERT_EQUAL_UINT(expected.size(), hal.calls.size());
  TEST_ASSERT_EQUAL_STRING(expected[0].c_str(), hal.calls[0].c_str());
}

static void test_oled_fail_beeps_error_code_and_stays_acoustic_only() {
  FakeHal hal;
  hal.oledInitResult = false;

  const bool ok = selfTestOled(hal);

  TEST_ASSERT_FALSE(ok);

  std::vector<std::string> expected = { "oledInit" };
  for (uint8_t i = 0; i < BEEPS_OLED; ++i) {
    expected.push_back("beep:" + std::to_string(BEEP_ERR_FREQ) + ":" + std::to_string(BEEP_ERR_MS));
    if (i + 1 < BEEPS_OLED) {
      expected.push_back("delayMs:" + std::to_string(BEEP_GAP_MS));
    }
  }

  TEST_ASSERT_EQUAL_UINT(expected.size(), hal.calls.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    TEST_ASSERT_EQUAL_STRING(expected[i].c_str(), hal.calls[i].c_str());
  }

  // Bei OLED-Fehler nur akustisch -> keine oledShowLine-Aufrufe.
  for (const auto& call : hal.calls) {
    TEST_ASSERT_TRUE(call.rfind("oledShowLine", 0) != 0);
  }
}

static void test_full_selftest_order_is_beep_then_oled() {
  FakeHal hal;
  hal.oledInitResult = true;

  const SelfTestResult result = runBootSelfTest(hal);

  TEST_ASSERT_TRUE(result.oledOk);
  TEST_ASSERT_EQUAL_UINT(2, hal.calls.size());

  const std::string expectedStartBeep =
      "beep:" + std::to_string(BEEP_START_FREQ) + ":" + std::to_string(BEEP_START_MS);
  TEST_ASSERT_EQUAL_STRING(expectedStartBeep.c_str(), hal.calls[0].c_str());
  TEST_ASSERT_EQUAL_STRING("oledInit", hal.calls[1].c_str());
}

static void test_full_selftest_propagates_oled_failure() {
  FakeHal hal;
  hal.oledInitResult = false;

  const SelfTestResult result = runBootSelfTest(hal);

  TEST_ASSERT_FALSE(result.oledOk);
  // Start-Piep + oledInit + BEEPS_OLED Fehlerpieps + (BEEPS_OLED - 1) Pausen.
  const size_t expectedCalls = 1 + 1 + BEEPS_OLED + (BEEPS_OLED - 1);
  TEST_ASSERT_EQUAL_UINT(expectedCalls, hal.calls.size());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_oled_ok_only_init_no_error_beep);
  RUN_TEST(test_oled_fail_beeps_error_code_and_stays_acoustic_only);
  RUN_TEST(test_full_selftest_order_is_beep_then_oled);
  RUN_TEST(test_full_selftest_propagates_oled_failure);
  return UNITY_END();
}
