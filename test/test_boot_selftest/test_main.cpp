// =============================================================
//  test_main.cpp — Unity-Tests fuer den Boot-Selbsttest:
//  OLED-Stufe, ROM-Validierung, Sensor-Stufe, Luefter-Stufe,
//  Discovery-Gate.
// =============================================================
#include <string>
#include <vector>
#include <unity.h>
#include "BootSelfTest.h"
#include "FakeHal.h"
#include "config.h"

void setUp() {}
void tearDown() {}

// ---------------------------------------------------------------
//  OLED-Stufe
// ---------------------------------------------------------------

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

  for (const auto& call : hal.calls) {
    TEST_ASSERT_TRUE(call.rfind("oledShowLine", 0) != 0);
  }
}

// ---------------------------------------------------------------
//  ROM-Validierung (reine Logik, kein Hal)
// ---------------------------------------------------------------

static void test_sensor_rom_valid_accepts_correct_family_and_crc() {
  // family 0x28, restliche Bytes frei gewaehlt, CRC8 (Dallas/Maxim) korrekt berechnet.
  const uint8_t rom[8] = { 0x28, 0xAA, 0xBB, 0xCC, 0x11, 0x22, 0x33, 0x74 };
  TEST_ASSERT_TRUE(sensorRomValid(rom));
}

static void test_sensor_rom_invalid_wrong_family_byte() {
  const uint8_t rom[8] = { 0x10, 0xAA, 0xBB, 0xCC, 0x11, 0x22, 0x33, 0x74 };
  TEST_ASSERT_FALSE(sensorRomValid(rom));
}

static void test_sensor_rom_invalid_bad_crc() {
  const uint8_t rom[8] = { 0x28, 0xAA, 0xBB, 0xCC, 0x11, 0x22, 0x33, 0x00 };
  TEST_ASSERT_FALSE(sensorRomValid(rom));
}

static void test_sensor_rom_placeholder_from_config_is_invalid() {
  // Ausgelieferter Zustand von config.h: Nullen -> CRC schlaegt fehl.
  TEST_ASSERT_FALSE(sensorRomValid(SENSOR_ROM[ROLE_VSA1]));
  TEST_ASSERT_FALSE(sensorRomValid(SENSOR_ROM[ROLE_VSA2]));
  TEST_ASSERT_FALSE(sensorRomValid(SENSOR_ROM[ROLE_AMB]));
}

static void test_needs_discovery_true_with_shipped_placeholder_config() {
  // config.h wird bewusst mit ungueltigen ROM-Adressen ausgeliefert.
  TEST_ASSERT_TRUE(selfTestNeedsDiscovery());
}

// ---------------------------------------------------------------
//  Sensor-Stufe (direkt getestet, unabhaengig vom Discovery-Gate)
// ---------------------------------------------------------------

static void test_sensors_all_ok_no_beep_no_oled_line() {
  FakeHal hal;
  // Default sensorTemps = 25.0f fuer alle drei Rollen.

  const bool ok = selfTestSensors(hal);

  TEST_ASSERT_TRUE(ok);
  const std::vector<std::string> expected = {
    "sensorReadTempC:0", "sensorReadTempC:1", "sensorReadTempC:2"
  };
  TEST_ASSERT_EQUAL_UINT(expected.size(), hal.calls.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    TEST_ASSERT_EQUAL_STRING(expected[i].c_str(), hal.calls[i].c_str());
  }
}

static void test_sensor_vsa2_missing_beeps_and_shows_oled_line() {
  FakeHal hal;
  hal.sensorTemps[ROLE_VSA2] = -127.0f;  // DS18B20-Diskonnekt-Sentinel

  const bool ok = selfTestSensors(hal);

  TEST_ASSERT_FALSE(ok);

  const std::vector<std::string> expected = {
    "sensorReadTempC:0",
    "sensorReadTempC:1",
    "oledShowLine:1:VSA2 FEHLT",
    "beep:" + std::to_string(BEEP_ERR_FREQ) + ":" + std::to_string(BEEP_ERR_MS),
    "delayMs:" + std::to_string(BEEP_GAP_MS),
    "beep:" + std::to_string(BEEP_ERR_FREQ) + ":" + std::to_string(BEEP_ERR_MS),
    "delayMs:" + std::to_string(BEEP_GAP_MS),
    "beep:" + std::to_string(BEEP_ERR_FREQ) + ":" + std::to_string(BEEP_ERR_MS),
    "sensorReadTempC:2",
  };
  static_assert(BEEPS_SENSOR == 3, "Test erwartet BEEPS_SENSOR == 3 Fehlerpieps");

  TEST_ASSERT_EQUAL_UINT(expected.size(), hal.calls.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    TEST_ASSERT_EQUAL_STRING(expected[i].c_str(), hal.calls[i].c_str());
  }
}

static void test_sensor_out_of_range_value_counts_as_missing() {
  FakeHal hal;
  hal.sensorTemps[ROLE_VSA1] = 200.0f;  // ausserhalb TEMP_MAX_VALID

  const bool ok = selfTestSensors(hal);

  TEST_ASSERT_FALSE(ok);
  TEST_ASSERT_EQUAL_STRING("oledShowLine:0:VSA1 FEHLT", hal.calls[1].c_str());
}

// ---------------------------------------------------------------
//  Luefter-Stufe (direkt getestet, unabhaengig vom Discovery-Gate)
// ---------------------------------------------------------------

static void test_fans_all_ok_no_beep_no_oled_line() {
  FakeHal hal;
  // Default fanRpms = 1000 fuer beide Luefter.

  const bool ok = selfTestFans(hal);

  TEST_ASSERT_TRUE(ok);
  const std::vector<std::string> expected = {
    "fanSetDutyPercent:0:100",
    "delayMs:" + std::to_string(FAN_STARTUP_MS),
    "fanReadRpm:0",
    "fanSetDutyPercent:0:0",
    "fanSetDutyPercent:1:100",
    "delayMs:" + std::to_string(FAN_STARTUP_MS),
    "fanReadRpm:1",
    "fanSetDutyPercent:1:0",
  };
  TEST_ASSERT_EQUAL_UINT(expected.size(), hal.calls.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    TEST_ASSERT_EQUAL_STRING(expected[i].c_str(), hal.calls[i].c_str());
  }
}

static void test_fan2_does_not_spin_while_fan1_ran_before() {
  FakeHal hal;
  hal.fanRpms[1] = 0;  // Luefter 2 dreht nicht, trotz Duty 100 %.

  const bool ok = selfTestFans(hal);

  TEST_ASSERT_FALSE(ok);

  std::vector<std::string> expected = {
    // Luefter 1: laeuft vorher erfolgreich durch.
    "fanSetDutyPercent:0:100",
    "delayMs:" + std::to_string(FAN_STARTUP_MS),
    "fanReadRpm:0",
    "fanSetDutyPercent:0:0",
    // Luefter 2: Anlauf, aber keine Drehzahl -> Fehler.
    "fanSetDutyPercent:1:100",
    "delayMs:" + std::to_string(FAN_STARTUP_MS),
    "fanReadRpm:1",
    "fanSetDutyPercent:1:0",
    "oledShowLine:4:LUEFTER2 FEHLT",
  };
  for (uint8_t i = 0; i < BEEPS_FAN; ++i) {
    expected.push_back("beep:" + std::to_string(BEEP_ERR_FREQ) + ":" + std::to_string(BEEP_ERR_MS));
    if (i + 1 < BEEPS_FAN) {
      expected.push_back("delayMs:" + std::to_string(BEEP_GAP_MS));
    }
  }

  TEST_ASSERT_EQUAL_UINT(expected.size(), hal.calls.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    TEST_ASSERT_EQUAL_STRING(expected[i].c_str(), hal.calls[i].c_str());
  }
}

static void test_fan_rpm_below_min_counts_as_error() {
  FakeHal hal;
  hal.fanRpms[0] = FAN_MIN_RPM - 1;

  const bool ok = selfTestFans(hal);

  TEST_ASSERT_FALSE(ok);
  TEST_ASSERT_EQUAL_STRING("oledShowLine:3:LUEFTER1 FEHLT", hal.calls[4].c_str());
}

// ---------------------------------------------------------------
//  Gesamtablauf runBootSelfTest()
// ---------------------------------------------------------------

static void test_full_selftest_enters_discovery_with_shipped_config() {
  FakeHal hal;
  hal.oledInitResult = true;

  const SelfTestResult result = runBootSelfTest(hal);

  TEST_ASSERT_TRUE(result.oledOk);
  TEST_ASSERT_TRUE(result.discoveryMode);
  TEST_ASSERT_FALSE(result.sensorsOk);
  TEST_ASSERT_FALSE(result.fansOk);

  const std::vector<std::string> expected = {
    "beep:" + std::to_string(BEEP_START_FREQ) + ":" + std::to_string(BEEP_START_MS),
    "oledInit",
    "oledShowLine:0:SETUP",
    "runOneWireDiscovery",
  };
  TEST_ASSERT_EQUAL_UINT(expected.size(), hal.calls.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    TEST_ASSERT_EQUAL_STRING(expected[i].c_str(), hal.calls[i].c_str());
  }
}

static void test_full_selftest_discovery_stays_acoustic_only_when_oled_down() {
  FakeHal hal;
  hal.oledInitResult = false;

  const SelfTestResult result = runBootSelfTest(hal);

  TEST_ASSERT_FALSE(result.oledOk);
  TEST_ASSERT_TRUE(result.discoveryMode);

  // Kein "SETUP"-Aufruf, da OLED nicht verfuegbar.
  for (const auto& call : hal.calls) {
    TEST_ASSERT_TRUE(call.rfind("oledShowLine", 0) != 0);
  }
  TEST_ASSERT_EQUAL_STRING("runOneWireDiscovery", hal.calls.back().c_str());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_oled_ok_only_init_no_error_beep);
  RUN_TEST(test_oled_fail_beeps_error_code_and_stays_acoustic_only);

  RUN_TEST(test_sensor_rom_valid_accepts_correct_family_and_crc);
  RUN_TEST(test_sensor_rom_invalid_wrong_family_byte);
  RUN_TEST(test_sensor_rom_invalid_bad_crc);
  RUN_TEST(test_sensor_rom_placeholder_from_config_is_invalid);
  RUN_TEST(test_needs_discovery_true_with_shipped_placeholder_config);

  RUN_TEST(test_sensors_all_ok_no_beep_no_oled_line);
  RUN_TEST(test_sensor_vsa2_missing_beeps_and_shows_oled_line);
  RUN_TEST(test_sensor_out_of_range_value_counts_as_missing);

  RUN_TEST(test_fans_all_ok_no_beep_no_oled_line);
  RUN_TEST(test_fan2_does_not_spin_while_fan1_ran_before);
  RUN_TEST(test_fan_rpm_below_min_counts_as_error);

  RUN_TEST(test_full_selftest_enters_discovery_with_shipped_config);
  RUN_TEST(test_full_selftest_discovery_stays_acoustic_only_when_oled_down);
  return UNITY_END();
}
