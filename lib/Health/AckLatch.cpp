#include "AckLatch.h"
#include "config.h"

bool canAck(uint32_t errAgeMs) {
  return errAgeMs >= ACK_GRACE_MS;
}

void ackAllActive(ChannelLatch* latches, uint8_t count, uint32_t now) {
  for (uint8_t i = 0; i < count; ++i) {
    ChannelLatch& latch = latches[i];
    if (!latch.active || latch.acked) {
      continue;
    }
    const uint32_t errAgeMs = now - latch.erroredAtMs;
    if (canAck(errAgeMs)) {
      latch.acked = true;
    }
  }
}
