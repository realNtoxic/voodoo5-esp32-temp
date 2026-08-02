// =============================================================
//  IDisplay.h — Schmale Zeichen-Abstraktion fuer die Dashboard-
//  Anzeige. Reine Schnittstelle, KEIN Arduino.h. Die Logik in
//  Dashboard.cpp ruft niemals direkt Adafruit_SSD1306 auf, nur
//  diese primitiven Operationen -- damit sie nativ mit
//  FakeDisplay testbar bleibt.
// =============================================================
#pragma once
#include <cstdint>

class IDisplay {
public:
  virtual ~IDisplay() = default;

  virtual void clear() = 0;

  // inverted: Text in Hintergrundfarbe zeichnen (fuer bereits per
  // fillRect gefuellte Zellen). Das Fuellen selbst uebernimmt die
  // Dashboard-Logik per fillRect(), nicht drawText().
  virtual void drawText(uint8_t x, uint8_t y, const char* s, uint8_t size,
                         bool inverted) = 0;

  virtual void fillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h) = 0;

  // Nur der Rahmen (4 Kanten) eines Rechtecks, ohne Fuellung -- fuer
  // die unquittierte Err-Aus-Phase einer Statuszelle (siehe
  // lib/Dashboard/StatusCell.h).
  virtual void frameRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h) = 0;

  // Gegenstueck zu fillRect(): fuellt mit der Hintergrundfarbe. Fuer
  // die gegenphasige Ack-Eckmarke innerhalb einer bereits
  // invertierten Zelle (siehe lib/Dashboard/StatusCell.h).
  virtual void clearRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h) = 0;

  virtual void hLine(uint8_t y) = 0;  // volle Breite
  virtual void vLine(uint8_t x) = 0;  // volle Hoehe

  virtual void present() = 0;
};
