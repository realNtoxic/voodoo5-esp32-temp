#include "FanControl.h"
#include "config.h"

uint8_t curveDuty(float tempC, bool fanWasOn) {
  if (tempC >= CURVE.fullC) {
    return CURVE.maxDuty;
  }
  if (tempC >= CURVE.onC) {
    const float t = (tempC - CURVE.onC) / (CURVE.fullC - CURVE.onC);
    return static_cast<uint8_t>(CURVE.minDuty + t * (CURVE.maxDuty - CURVE.minDuty));
  }
  if (fanWasOn && tempC >= CURVE.offC) {
    return CURVE.minDuty;  // Hysterese-Totband: laeuft weiter, wenn schon an
  }
  return 0;
}

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

uint8_t channelFailSafeDuty(uint8_t curveDuty, bool sensorOk) {
  return sensorOk ? curveDuty : CURVE.maxDuty;
}
