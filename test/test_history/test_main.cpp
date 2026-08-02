// =============================================================
//  test_main.cpp — Unity-Tests fuer die Verschleiss-Historie
//  (lib/History/): tryUpdateMax(), HistoryTracker (gepuffertes
//  Schreiben) und den Persistenz-Roundtrip ueber FakeHistoryStore.
// =============================================================
#include <unity.h>
#include "History.h"
#include "HistoryTracker.h"
#include "FakeHistoryStore.h"
#include "SensorCal.h"
#include "config.h"

void setUp() {}
void tearDown() {}

static void test_try_update_max_below_threshold_no_update() {
  float stored = 40.0f;
  TEST_ASSERT_FALSE(tryUpdateMax(stored, 40.3f, 0.5f));  // 40.3 <= 40.0+0.5
  TEST_ASSERT_EQUAL_FLOAT(40.0f, stored);
}

static void test_try_update_max_real_record_updates() {
  float stored = 40.0f;
  TEST_ASSERT_TRUE(tryUpdateMax(stored, 41.0f, 0.5f));
  TEST_ASSERT_EQUAL_FLOAT(41.0f, stored);
}

// Testkonvention: candidate genau an der Schwelle (stored + eps)
// zaehlt noch NICHT als Rekord -- "candidate > stored + eps" ist
// strikt groesser, keine Gleichheit.
static void test_try_update_max_exactly_at_threshold_no_update() {
  float stored = 40.0f;
  TEST_ASSERT_FALSE(tryUpdateMax(stored, 40.5f, 0.5f));
  TEST_ASSERT_EQUAL_FLOAT(40.0f, stored);
}

// Historie basiert auf (rawSonde - rawAmb), NICHT auf dem kalibrierten
// Wert -- eine spaetere Nachkalibrierung von k darf den gespeicherten
// Max-Wert nicht veraendern.
static void test_history_uses_raw_delta_not_calibrated_value() {
  HistoryTracker tracker;
  const float rawSonde = 60.0f;
  const float rawAmb = 25.0f;

  tracker.updateChannel(0, rawSonde, rawAmb);
  const float deltaAfterFirstUpdate = tracker.data().maxDeltaC[0];

  // dieTempC() mit zwei unterschiedlichen k-Werten aufgerufen -- die
  // Historie bekommt davon nichts mit, sie nutzt weiterhin nur die
  // Rohwerte.
  const float calWithK1 = dieTempC(rawSonde, rawAmb, 0.1f);
  const float calWithK2 = dieTempC(rawSonde, rawAmb, 0.5f);
  TEST_ASSERT_TRUE(calWithK1 != calWithK2);  // k wirkt sich auf calC aus...

  TEST_ASSERT_EQUAL_FLOAT(rawSonde - rawAmb, deltaAfterFirstUpdate);
  TEST_ASSERT_EQUAL_FLOAT(deltaAfterFirstUpdate, tracker.data().maxDeltaC[0]);
}

// Persistenz-Roundtrip: save() gefolgt von load() auf einem frischen
// Objekt stellt exakt dieselben Werte wieder her.
static void test_persistence_roundtrip_via_fake_store() {
  FakeHistoryStore store;
  HistoryData written{ { 51.0f, 52.5f, 53.0f, 54.25f }, 30.0f };

  TEST_ASSERT_TRUE(store.save(written));

  HistoryData loaded{};
  TEST_ASSERT_TRUE(store.load(loaded));
  TEST_ASSERT_EQUAL_FLOAT(written.maxDeltaC[0], loaded.maxDeltaC[0]);
  TEST_ASSERT_EQUAL_FLOAT(written.maxDeltaC[1], loaded.maxDeltaC[1]);
  TEST_ASSERT_EQUAL_FLOAT(written.maxDeltaC[2], loaded.maxDeltaC[2]);
  TEST_ASSERT_EQUAL_FLOAT(written.maxDeltaC[3], loaded.maxDeltaC[3]);
  TEST_ASSERT_EQUAL_FLOAT(written.maxAmbC, loaded.maxAmbC);
}

// load() auf einem leeren Store liefert false -- Aufrufer startet mit
// Nullwerten (siehe HistoryTracker::begin()).
static void test_load_without_prior_save_returns_false() {
  FakeHistoryStore store;
  HistoryData loaded{};
  TEST_ASSERT_FALSE(store.load(loaded));
}

// Gepuffertes Schreiben: mehrere Rekorde innerhalb von
// HISTORY_COMMIT_MS erzeugen nur EINEN save()-Aufruf.
static void test_buffered_commit_coalesces_multiple_records() {
  FakeHistoryStore store;
  HistoryTracker tracker;
  tracker.begin(store);  // kein Store-Inhalt -> Nullwerte

  tracker.updateChannel(0, 45.0f, 20.0f);  // erster Rekord, dirty
  tracker.tick(store, 100);                // zu frueh, kein Commit
  TEST_ASSERT_EQUAL_INT(0, store.saveCount);

  tracker.updateChannel(1, 50.0f, 20.0f);  // zweiter Rekord, immer noch dirty
  tracker.tick(store, HISTORY_COMMIT_MS - 1);  // immer noch zu frueh
  TEST_ASSERT_EQUAL_INT(0, store.saveCount);

  tracker.tick(store, HISTORY_COMMIT_MS);  // jetzt faellig -> genau ein save()
  TEST_ASSERT_EQUAL_INT(1, store.saveCount);
  TEST_ASSERT_EQUAL_FLOAT(25.0f, store.stored.maxDeltaC[0]);
  TEST_ASSERT_EQUAL_FLOAT(30.0f, store.stored.maxDeltaC[1]);

  // Ohne neuen Rekord (nicht mehr dirty) committet ein weiterer Tick
  // nach Ablauf des Intervalls nicht erneut.
  tracker.tick(store, HISTORY_COMMIT_MS * 2);
  TEST_ASSERT_EQUAL_INT(1, store.saveCount);
}

// Boot-Vorbelegung: begin() liest vorhandene Werte aus dem Store, ein
// danach ankommender, kleinerer Wert darf den alten Rekord nicht
// verdraengen.
static void test_begin_preloads_existing_record_from_store() {
  FakeHistoryStore store;
  store.hasData = true;
  store.stored = HistoryData{ { 44.0f, 0.0f, 0.0f, 0.0f }, 0.0f };

  HistoryTracker tracker;
  tracker.begin(store);
  TEST_ASSERT_EQUAL_FLOAT(44.0f, tracker.data().maxDeltaC[0]);

  tracker.updateChannel(0, 30.0f, 10.0f);  // Delta 20 < 44 -> kein Rekord
  TEST_ASSERT_EQUAL_FLOAT(44.0f, tracker.data().maxDeltaC[0]);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_try_update_max_below_threshold_no_update);
  RUN_TEST(test_try_update_max_real_record_updates);
  RUN_TEST(test_try_update_max_exactly_at_threshold_no_update);
  RUN_TEST(test_history_uses_raw_delta_not_calibrated_value);
  RUN_TEST(test_persistence_roundtrip_via_fake_store);
  RUN_TEST(test_load_without_prior_save_returns_false);
  RUN_TEST(test_buffered_commit_coalesces_multiple_records);
  RUN_TEST(test_begin_preloads_existing_record_from_store);
  return UNITY_END();
}
