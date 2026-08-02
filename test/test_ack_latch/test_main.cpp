// =============================================================
//  test_main.cpp — Unity-Tests fuer die Ack-Zustandslogik pro
//  Fehler-Latch (lib/Health/AckLatch.h): canAck() (Karenzzone) und
//  ackAllActive() ("alle auf einmal", pro-Latch-Flag).
// =============================================================
#include <unity.h>
#include "AckLatch.h"
#include "config.h"

void setUp() {}
void tearDown() {}

static void test_can_ack_below_grace_period_is_false() {
  TEST_ASSERT_FALSE(canAck(ACK_GRACE_MS - 1));
  TEST_ASSERT_FALSE(canAck(0));
}

static void test_can_ack_at_or_above_grace_period_is_true() {
  TEST_ASSERT_TRUE(canAck(ACK_GRACE_MS));
  TEST_ASSERT_TRUE(canAck(ACK_GRACE_MS + 1000));
}

// "Alle auf einmal": quittiert nur aktive, unquittierte Latches, die
// nicht mehr in der Karenzzone sind. Inaktive und bereits quittierte
// Latches bleiben unangetastet.
static void test_ack_all_active_only_acks_eligible_latches() {
  const uint32_t now = 10000;
  ChannelLatch latches[4] = {
    { true,  false, 0 },     // errAge=10000 >= ACK_GRACE_MS -> wird acked
    { true,  false, 9000 },  // errAge=1000  <  ACK_GRACE_MS -> bleibt unacked (Karenz)
    { false, false, 0 },     // nicht aktiv -> unangetastet
    { true,  true,  0 },     // schon acked -> unangetastet (bleibt true)
  };

  ackAllActive(latches, 4, now);

  TEST_ASSERT_TRUE(latches[0].acked);
  TEST_ASSERT_FALSE(latches[1].acked);
  TEST_ASSERT_FALSE(latches[2].acked);
  TEST_ASSERT_TRUE(latches[3].acked);
}

// Neue Fehler nach einem Ack sind eigenstaendige Ereignisse: starten
// acknowledged=false und durchlaufen die Karenzzone erneut, auch wenn
// kurz zuvor "alle" quittiert wurden.
static void test_new_error_after_ack_starts_unacked_and_respects_grace() {
  ChannelLatch latch{ true, false, 0 };
  ackAllActive(&latch, 1, /*now=*/ACK_GRACE_MS);
  TEST_ASSERT_TRUE(latch.acked);

  // Neuer, eigenstaendiger Fehler auf demselben Kanal etwas spaeter.
  const uint32_t newErrorAtMs = ACK_GRACE_MS + 5000;
  latch = ChannelLatch{ true, false, newErrorAtMs };

  // Sofortiger Ack-Versuch im selben Moment -> noch in der Karenzzone.
  ackAllActive(&latch, 1, newErrorAtMs);
  TEST_ASSERT_FALSE(latch.acked);

  // Erst nach Ablauf der Karenzzone laesst er sich quittieren.
  ackAllActive(&latch, 1, newErrorAtMs + ACK_GRACE_MS);
  TEST_ASSERT_TRUE(latch.acked);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_can_ack_below_grace_period_is_false);
  RUN_TEST(test_can_ack_at_or_above_grace_period_is_true);
  RUN_TEST(test_ack_all_active_only_acks_eligible_latches);
  RUN_TEST(test_new_error_after_ack_starts_unacked_and_respects_grace);
  return UNITY_END();
}
