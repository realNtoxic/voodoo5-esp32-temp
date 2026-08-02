#include "HistoryTracker.h"
#include "config.h"

void HistoryTracker::begin(IHistoryStore& store) {
  store.load(data_);
}

void HistoryTracker::updateChannel(uint8_t channel, float rawSondeC, float rawAmbC) {
  const float deltaC = rawSondeC - rawAmbC;
  if (tryUpdateMax(data_.maxDeltaC[channel], deltaC, HISTORY_EPSILON_C)) {
    dirty_ = true;
  }
}

void HistoryTracker::updateAmbient(float rawAmbC) {
  if (tryUpdateMax(data_.maxAmbC, rawAmbC, HISTORY_EPSILON_C)) {
    dirty_ = true;
  }
}

void HistoryTracker::tick(IHistoryStore& store, uint32_t now) {
  if (!dirty_) {
    return;
  }
  if (now - lastCommitMs_ < HISTORY_COMMIT_MS) {
    return;
  }
  store.save(data_);  // Fehler bewusst ignoriert, siehe IHistoryStore.h
  dirty_ = false;
  lastCommitMs_ = now;
}
