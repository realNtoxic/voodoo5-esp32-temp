// =============================================================
//  FakeHistoryStore.h — Test-Double fuer IHistoryStore. Haelt die
//  "persistierten" Werte im Speicher und zaehlt save()-Aufrufe, damit
//  Tests Schreib-Haeufigkeit und Werte pruefen koennen (analog zu
//  FakeDisplay/FakeLed in den anderen Testsuiten).
// =============================================================
#pragma once
#include "IHistoryStore.h"

class FakeHistoryStore : public IHistoryStore {
public:
  HistoryData stored{ { 0.0f, 0.0f, 0.0f, 0.0f }, 0.0f };
  bool hasData = false;
  int saveCount = 0;

  bool load(HistoryData& data) override {
    if (!hasData) {
      return false;
    }
    data = stored;
    return true;
  }

  bool save(const HistoryData& data) override {
    stored = data;
    hasData = true;
    ++saveCount;
    return true;
  }
};
