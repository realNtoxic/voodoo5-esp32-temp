#include "LedPattern.h"
#include "config.h"

LedMode selectLedMode(bool faultActive) {
  return faultActive ? LedMode::Fault : LedMode::Heartbeat;
}

bool ledLevel(LedMode mode, uint8_t phase, uint32_t nowMs) {
  if (mode == LedMode::Heartbeat) {
    return phase != 0;
  }

  // Fault: Doppelblink ueber ein festes Zeitfenster, siehe config.h
  // LED_FAULT_*. Reihenfolge: an / aus / an / aus (lang), dann von
  // vorn -- klar unterscheidbar vom ruhigen Heartbeat.
  const uint32_t onGapOn = static_cast<uint32_t>(LED_FAULT_ON_MS)
                          + LED_FAULT_GAP_MS + LED_FAULT_ON_MS;
  const uint32_t period = onGapOn + LED_FAULT_PAUSE_MS;
  const uint32_t t = nowMs % period;

  if (t < LED_FAULT_ON_MS) return true;
  if (t < static_cast<uint32_t>(LED_FAULT_ON_MS) + LED_FAULT_GAP_MS) return false;
  if (t < onGapOn) return true;
  return false;
}
