// =============================================================
//  test_main.cpp — Unity-Tests fuer den Heartbeat-Grundzustand der
//  Status-LED.
// =============================================================
#include <unity.h>
#include "StatusLed.h"

void setUp() {}
void tearDown() {}

static void test_heartbeat_led_follows_phase() {
  TEST_ASSERT_FALSE(heartbeatLedOn(0));
  TEST_ASSERT_TRUE(heartbeatLedOn(1));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_heartbeat_led_follows_phase);
  return UNITY_END();
}
