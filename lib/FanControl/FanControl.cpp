#include "FanControl.h"
#include "config.h"

bool ambientPanicActive(float ambTempC, bool wasPanicActive) {
  if (wasPanicActive) {
    return ambTempC > AMB_WARN_OFF_C;  // bleibt aktiv, bis unter die Off-Schwelle
  }
  return ambTempC >= AMB_WARN_C;  // wird aktiv ab der Warn-Schwelle
}

uint8_t effectiveDuty(uint8_t curveDuty, bool panicActive) {
  const uint8_t panicDuty = panicActive ? CURVE.maxDuty : 0;
  return curveDuty > panicDuty ? curveDuty : panicDuty;
}
