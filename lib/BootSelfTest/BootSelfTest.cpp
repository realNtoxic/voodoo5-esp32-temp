#include "BootSelfTest.h"
#include "config.h"

void selfTestStartBeep(Hal& hal) {
  hal.beep(BEEP_START_FREQ, BEEP_START_MS);
}

bool selfTestOled(Hal& hal) {
  const bool ok = hal.oledInit();
  if (!ok) {
    // Bei OLED-Fehler nur akustisch — die Anzeige selbst ist ja betroffen.
    for (uint8_t i = 0; i < BEEPS_OLED; ++i) {
      hal.beep(BEEP_ERR_FREQ, BEEP_ERR_MS);
      if (i + 1 < BEEPS_OLED) {
        hal.delayMs(BEEP_GAP_MS);
      }
    }
  }
  return ok;
}

SelfTestResult runBootSelfTest(Hal& hal) {
  selfTestStartBeep(hal);
  const bool oledOk = selfTestOled(hal);
  return SelfTestResult{ oledOk };
}
