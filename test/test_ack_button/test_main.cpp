// =============================================================
//  test_main.cpp — Unity-Tests fuer den Ack-Taster-Zustandsautomaten.
// =============================================================
#include <string>
#include <vector>
#include <unity.h>
#include "AckButton.h"
#include "FakeHal.h"
#include "config.h"

void setUp() {}
void tearDown() {}

static void test_short_press_below_500ms_no_tone_no_event() {
  FakeHal hal;
  AckButton button;

  TEST_ASSERT_TRUE(AckEvent::None == button.tick(hal, true, 0));
  TEST_ASSERT_TRUE(AckEvent::None == button.tick(hal, true, 300));
  TEST_ASSERT_TRUE(AckEvent::None == button.tick(hal, false, 300));

  TEST_ASSERT_EQUAL_UINT(0, hal.calls.size());
}

static void test_release_between_500_and_1200ms_fires_ack_on_release() {
  FakeHal hal;
  AckButton button;

  TEST_ASSERT_TRUE(AckEvent::None == button.tick(hal, true, 0));
  TEST_ASSERT_TRUE(AckEvent::None == button.tick(hal, true, AKTION1_TIME));

  const std::vector<std::string> expectedAfterAktion1 = {
    "beepAsync:" + std::to_string(AKTION1_BEEP_FREQ) + ":" + std::to_string(AKTION1_BEEP_MS),
  };
  TEST_ASSERT_EQUAL_UINT(expectedAfterAktion1.size(), hal.calls.size());
  TEST_ASSERT_EQUAL_STRING(expectedAfterAktion1[0].c_str(), hal.calls[0].c_str());

  const AckEvent released = button.tick(hal, false, 800);
  TEST_ASSERT_TRUE(AckEvent::Ack == released);

  // Kein zweiter (Aktion2-)Ton, kein weiterer Aufruf durchs Loslassen.
  TEST_ASSERT_EQUAL_UINT(1, hal.calls.size());
}

// Kernanforderung: Die 1200-ms-Schwelle (AKTION2_TIME) wird rein anhand
// von `now` erkannt, unabhaengig von der Laenge des Aktion1-Tons bei
// 500 ms. Das ist nur garantiert, weil AckButton ausschliesslich
// hal.beepAsync() (nicht-blockierend) verwendet -- nie das blockierende
// hal.beep() aus dem Boot-Selbsttest. Wuerde stattdessen blockierend
// getoent, wuerde die Hauptschleife auf echter Hardware waehrend des
// Tons haengen und `tick()` liesse sich nicht mehr rechtzeitig mit
// aktuellem `now` aufrufen -- die 1200-ms-Erkennung liefe spaet.
static void test_1200ms_threshold_detected_independent_of_tone_length() {
  FakeHal hal;
  AckButton button;

  TEST_ASSERT_TRUE(AckEvent::None == button.tick(hal, true, 0));

  // Aktion1-Ton bei 500 ms (40 ms lang) -- rein async geloggt, hal.beep()
  // wird dabei nie aufgerufen.
  TEST_ASSERT_TRUE(AckEvent::None == button.tick(hal, true, AKTION1_TIME));

  // Kurz vor der 1200-ms-Schwelle: noch kein LatchReset, obwohl die
  // (asynchrone) Aktion1-Ton-Dauer von 40 ms laengst verstrichen waere,
  // wenn sie faelschlich blockierend eingerechnet wuerde.
  TEST_ASSERT_TRUE(AckEvent::None == button.tick(hal, true, AKTION2_TIME - 1));

  // Schwelle erreicht: LatchReset feuert exakt hier, nicht verzoegert.
  const AckEvent atThreshold = button.tick(hal, true, AKTION2_TIME);
  TEST_ASSERT_TRUE(AckEvent::LatchReset == atThreshold);

  // Weiteres Halten (z. B. bis 2500 ms) loest kein zweites LatchReset aus.
  TEST_ASSERT_TRUE(AckEvent::None == button.tick(hal, true, 2500));

  // Loslassen nach LatchReset liefert explizit KEIN Ack.
  TEST_ASSERT_TRUE(AckEvent::None == button.tick(hal, false, 3000));

  const std::vector<std::string> expected = {
    "beepAsync:" + std::to_string(AKTION1_BEEP_FREQ) + ":" + std::to_string(AKTION1_BEEP_MS),
    "beepAsync:" + std::to_string(AKTION2_BEEP_FREQ) + ":" + std::to_string(AKTION2_BEEP_MS),
  };
  TEST_ASSERT_EQUAL_UINT(expected.size(), hal.calls.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    TEST_ASSERT_EQUAL_STRING(expected[i].c_str(), hal.calls[i].c_str());
  }

  // Kein einziger Aufruf des blockierenden hal.beep() -- ausschliesslich
  // beepAsync(). Das ist die eigentliche Garantie fuer die
  // Schwellen-Unabhaengigkeit von der Tonlaenge.
  for (const auto& call : hal.calls) {
    TEST_ASSERT_TRUE(call.rfind("beep:", 0) != 0);
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_short_press_below_500ms_no_tone_no_event);
  RUN_TEST(test_release_between_500_and_1200ms_fires_ack_on_release);
  RUN_TEST(test_1200ms_threshold_detected_independent_of_tone_length);
  return UNITY_END();
}
