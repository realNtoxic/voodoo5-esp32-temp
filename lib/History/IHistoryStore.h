// =============================================================
//  IHistoryStore.h — Persistenz-Schnittstelle fuer die
//  Max-Temperatur-Historie (siehe History.h, CLAUDE.md
//  "Verschleiss-Historie"). Reine Schnittstelle, KEIN Arduino.h --
//  reale Implementierung (NVS ueber Preferences) lebt in
//  src/main.cpp, Test-Double FakeHistoryStore in den jeweiligen
//  Testsuiten.
// =============================================================
#pragma once
#include "History.h"

class IHistoryStore {
public:
  virtual ~IHistoryStore() = default;

  // false = keine (gueltigen) Daten gefunden, z. B. erster Boot nach
  // einem Chip-Erase -- Aufrufer startet dann mit Nullwerten.
  virtual bool load(HistoryData& data) = 0;

  // false = Schreibfehler. Persistenz ist Komfort/Diagnose, kein
  // sicherheitskritischer Pfad -- ein fehlgeschlagener save() darf
  // die Regelung nicht beeinflussen (Aufrufer faengt das ab und
  // ignoriert es, siehe CLAUDE.md).
  virtual bool save(const HistoryData& data) = 0;
};
