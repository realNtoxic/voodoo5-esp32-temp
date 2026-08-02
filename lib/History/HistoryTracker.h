// =============================================================
//  HistoryTracker.h — Haelt die Max-Uebertemperatur-Werte im RAM und
//  committet sie gepuffert ueber IHistoryStore (siehe History.h,
//  CLAUDE.md "Verschleiss-Historie" Abschnitt "Flash-Verschleiss-
//  Schutz"). Reine Logik, KEIN Arduino.h -> nativ testbar. Kein
//  internes millis() -- der aktuelle Zeitstempel wird in tick()
//  hereingereicht, analog zu AckButton::tick().
// =============================================================
#pragma once
#include <cstdint>
#include "History.h"
#include "IHistoryStore.h"

class HistoryTracker {
public:
  // Laedt vorhandene Werte aus dem Store, um die RAM-Maxima beim Boot
  // vorzubelegen (siehe CLAUDE.md). Liefert der Store keine gueltigen
  // Daten (z. B. erster Boot), bleibt data() bei Nullwerten.
  void begin(IHistoryStore& store);

  // Aktualisiert den Rekord fuer einen VSA-Kanal (0..3) mit der
  // Uebertemperatur (rawSondeC - rawAmbC). Setzt bei einem echten
  // Rekord intern das dirty-Flag, schreibt aber noch nichts.
  void updateChannel(uint8_t channel, float rawSondeC, float rawAmbC);

  // Aktualisiert den Ambient-Rekord (roh).
  void updateAmbient(float rawAmbC);

  // Committet nur, wenn seit der letzten Aktualisierung ein echter
  // Rekord aufgetreten ist (dirty) UND seit dem letzten Commit
  // mindestens HISTORY_COMMIT_MS vergangen sind. Ein Schreibfehler
  // wird abgefangen und ignoriert -- die Regelung darf davon nie
  // beeinflusst werden.
  void tick(IHistoryStore& store, uint32_t now);

  const HistoryData& data() const { return data_; }

private:
  HistoryData data_{ { 0.0f, 0.0f, 0.0f, 0.0f }, 0.0f };
  bool dirty_ = false;
  uint32_t lastCommitMs_ = 0;
};
