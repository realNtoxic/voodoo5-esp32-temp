#include "BootSelfTest.h"
#include "config.h"

void selfTestStartBeep(Hal& hal) {
  hal.beep(BEEP_START_FREQ, BEEP_START_MS);
}

bool selfTestOled(Hal& hal) {
  // Muss vor dem ersten I2C-Zugriff (oledInit() -> Wire.begin()) laufen:
  // Ein Reset mitten in einer Uebertragung kann den Bus mit SDA auf LOW
  // haengen lassen, siehe CLAUDE.md "Fallen".
  hal.i2cRecover(PIN_I2C_SDA, PIN_I2C_SCL);

  const bool ok = hal.oledInit();
  if (!ok) {
    // Bei OLED-Fehler nur akustisch — die Anzeige selbst ist ja betroffen.
    for (uint8_t i = 0; i < BEEPS_OLED; ++i) {
      hal.beep(BEEP_ERR_FREQ, BEEP_ERR_MS);
      if (i + 1 < BEEPS_OLED) {
        hal.delayMs(BEEP_GAP_MS);
      }
    }
  } else {
    // Sichtbare Bestaetigung, dass das Display tatsaechlich lebt --
    // bisher blieb der Bildschirm bei Erfolg nur leer (clearDisplay()).
    hal.oledShowLine(0, "OLED OK");
  }
  return ok;
}

SelfTestResult runBootSelfTest(Hal& hal) {
  selfTestStartBeep(hal);
  const bool oledOk = selfTestOled(hal);
  return SelfTestResult{ oledOk };
}
