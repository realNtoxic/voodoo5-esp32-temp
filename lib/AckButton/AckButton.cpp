#include "AckButton.h"
#include "config.h"

AckEvent AckButton::tick(Hal& hal, bool pressed, uint32_t now) {
  if (pressed && !wasPressed_) {
    // Neuer Tastendruck beginnt.
    wasPressed_ = true;
    pressStartMs_ = now;
    aktion1Fired_ = false;
    aktion2Fired_ = false;
    return AckEvent::None;
  }

  if (pressed) {
    const uint32_t heldMs = now - pressStartMs_;

    if (!aktion2Fired_ && heldMs >= AKTION2_TIME) {
      aktion2Fired_ = true;
      hal.beepAsync(AKTION2_BEEP_FREQ, AKTION2_BEEP_MS);
      return AckEvent::LatchReset;
    }

    if (!aktion1Fired_ && heldMs >= AKTION1_TIME) {
      aktion1Fired_ = true;
      hal.beepAsync(AKTION1_BEEP_FREQ, AKTION1_BEEP_MS);
    }

    return AckEvent::None;
  }

  // Taste losgelassen.
  if (wasPressed_) {
    wasPressed_ = false;
    const bool shouldAck = aktion1Fired_ && !aktion2Fired_;
    aktion1Fired_ = false;
    aktion2Fired_ = false;
    if (shouldAck) {
      return AckEvent::Ack;
    }
  }

  return AckEvent::None;
}
