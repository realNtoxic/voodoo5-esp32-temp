#include "BootSelfTest.h"
#include "config.h"
#include <cstdio>

namespace {

constexpr uint8_t DS18B20_FAMILY_CODE = 0x28;

// Dallas/Maxim 1-Wire CRC8, Polynom x^8+x^5+x^4+1 (0x8C, LSB-first).
uint8_t crc8Dallas(const uint8_t* data, uint8_t len) {
  uint8_t crc = 0;
  for (uint8_t i = 0; i < len; ++i) {
    uint8_t inByte = data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      const uint8_t mix = (crc ^ inByte) & 0x01;
      crc >>= 1;
      if (mix) {
        crc ^= 0x8C;
      }
      inByte >>= 1;
    }
  }
  return crc;
}

const char* roleLabel(uint8_t role) {
  switch (role) {
    case ROLE_VSA1: return "VSA1";
    case ROLE_VSA2: return "VSA2";
    case ROLE_AMB:  return "AMB";
    default:        return "?";
  }
}

const char* fanLabel(uint8_t fanIndex) {
  return fanIndex == 0 ? "LUEFTER1" : "LUEFTER2";
}

// OLED-Zeilen 0-2 sind waehrend des Selbsttests fuer die Sensoren belegt.
constexpr uint8_t OLED_ROW_FAN_BASE = 3;

void beepErrorCode(Hal& hal, uint8_t count) {
  for (uint8_t i = 0; i < count; ++i) {
    hal.beep(BEEP_ERR_FREQ, BEEP_ERR_MS);
    if (i + 1 < count) {
      hal.delayMs(BEEP_GAP_MS);
    }
  }
}

}  // namespace

void selfTestStartBeep(Hal& hal) {
  hal.beep(BEEP_START_FREQ, BEEP_START_MS);
}

bool selfTestOled(Hal& hal) {
  const bool ok = hal.oledInit();
  if (!ok) {
    // Bei OLED-Fehler nur akustisch — die Anzeige selbst ist ja betroffen.
    beepErrorCode(hal, BEEPS_OLED);
  }
  return ok;
}

bool sensorRomValid(const uint8_t rom[8]) {
  if (rom[0] != DS18B20_FAMILY_CODE) {
    return false;
  }
  return crc8Dallas(rom, 7) == rom[7];
}

bool selfTestNeedsDiscovery() {
  if (FORCE_DISCOVERY) {
    return true;
  }
  for (uint8_t role = 0; role < 3; ++role) {
    if (!sensorRomValid(SENSOR_ROM[role])) {
      return true;
    }
  }
  return false;
}

bool selfTestSensors(Hal& hal) {
  bool allOk = true;
  for (uint8_t role = 0; role < 3; ++role) {
    const float tempC = hal.sensorReadTempC(SENSOR_ROM[role]);
    const bool ok = tempC >= TEMP_MIN_VALID && tempC <= TEMP_MAX_VALID;
    if (!ok) {
      allOk = false;
      char line[16];
      std::snprintf(line, sizeof(line), "%s FEHLT", roleLabel(role));
      hal.oledShowLine(role, line);
      beepErrorCode(hal, BEEPS_SENSOR);
    }
  }
  return allOk;
}

bool selfTestFans(Hal& hal) {
  bool allOk = true;
  for (uint8_t fanIndex = 0; fanIndex < 2; ++fanIndex) {
    hal.fanSetDutyPercent(fanIndex, 100);
    hal.delayMs(FAN_STARTUP_MS);
    const uint16_t rpm = hal.fanReadRpm(fanIndex);
    hal.fanSetDutyPercent(fanIndex, 0);

    const bool ok = rpm >= FAN_MIN_RPM;
    if (!ok) {
      allOk = false;
      char line[16];
      std::snprintf(line, sizeof(line), "%s FEHLT", fanLabel(fanIndex));
      hal.oledShowLine(OLED_ROW_FAN_BASE + fanIndex, line);
      beepErrorCode(hal, BEEPS_FAN);
    }
  }
  return allOk;
}

SelfTestResult runBootSelfTest(Hal& hal) {
  selfTestStartBeep(hal);
  const bool oledOk = selfTestOled(hal);

  if (selfTestNeedsDiscovery()) {
    if (oledOk) {
      // Bei OLED-Fehler nur akustisch, wie schon bei selfTestOled().
      hal.oledShowLine(0, "SETUP");
    }
    hal.runOneWireDiscovery();
    return SelfTestResult{ oledOk, true, false, false };
  }

  const bool sensorsOk = selfTestSensors(hal);
  const bool fansOk = selfTestFans(hal);
  return SelfTestResult{ oledOk, false, sensorsOk, fansOk };
}
