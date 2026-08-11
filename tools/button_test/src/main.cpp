// =============================================================
//  tools/button_test/src/main.cpp — Eigenstaendiger Taster-Bring-up-Test.
//
//  Zweck: NUR pruefen, ob der Ack-Taster an GPIO27 sauber entprellt
//  erkannt wird und dabei genau EIN kurzer Piep pro Druck ausgeloest
//  wird -- keine Regelung, kein Hal/AckButton aus der Hauptfirmware,
//  bewusst vollstaendig eigenstaendig, analog zu tools/display_test/,
//  tools/led_test/, tools/sensor_test/, tools/fan_test/.
//
//  Der bit-gebangte Rechteckton ist 1:1 aus src/main.cpp
//  (Esp32Hal::beep()) uebernommen, hier aber als lokale Kopie ohne
//  Include -- dasselbe Prinzip wie bei den anderen Tools: ein
//  eigenstaendiges Tool haengt an keinem Include-Pfad ausserhalb
//  seines eigenen Projekts. Grund fuer bit-banging statt tone()/
//  noTone(): auf dem ESP32 startet der zweite Ton nach kurzer Pause
//  damit oft nicht zuverlaessig neu (siehe CLAUDE.md "Fallen").
//
//  Bauen/Flashen: pio run -e buttontest -t upload
// =============================================================
#include <Arduino.h>

namespace {

// --- Hardware -------------------------------------------------
constexpr uint8_t PIN_BUTTON  = 27;  // gegen GND, interner Pull-up
constexpr uint8_t PIN_SPEAKER = 25;  // BC547-Treiberstufe, wie im Projekt

// --- Entprellung ------------------------------------------------
// Ein mechanischer Taster prellt einige Millisekunden nach jeder
// Flanke. Erst wenn der Pin fuer DEBOUNCE_MS ununterbrochen denselben
// Pegel haelt, gilt dieser Pegel als "stabil" -- so zaehlt ein
// einzelner Druck als EIN Ereignis statt als mehrere durch
// Kontaktzittern.
constexpr uint16_t DEBOUNCE_MS = 40;

// --- Piep ---------------------------------------------------------
constexpr uint16_t BEEP_FREQ_HZ = 2000;
constexpr uint16_t BEEP_MS      = 80;  // kurz: 50-100 ms lt. Aufgabenstellung

// Bit-gebangter Rechteckton -- blockiert fuer BEEP_MS. Fuer dieses
// Wegwerf-Tool unkritisch (kein Regelkreis, der darunter leiden
// koennte): 80 ms Pause je Tastendruck faellt beim Pollen nicht auf.
void beep(uint16_t freqHz, uint16_t ms) {
  const uint32_t halfPeriodUs = 500000UL / freqHz;
  const uint32_t cycles = (static_cast<uint32_t>(ms) * 1000UL) / (2 * halfPeriodUs);
  for (uint32_t i = 0; i < cycles; ++i) {
    digitalWrite(PIN_SPEAKER, HIGH);
    delayMicroseconds(halfPeriodUs);
    digitalWrite(PIN_SPEAKER, LOW);
    delayMicroseconds(halfPeriodUs);
  }
}

// --- Entprellter Zustand -----------------------------------------
int      lastRawState  = HIGH;  // INPUT_PULLUP: HIGH = losgelassen
int      stableState   = HIGH;
uint32_t lastChangeMs   = 0;
uint32_t pressCount     = 0;

}  // namespace

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_SPEAKER, OUTPUT);
  digitalWrite(PIN_SPEAKER, LOW);

  Serial.println("Taster-Test aktiv, Pin 27 (INPUT_PULLUP), Speaker Pin 25");

  lastRawState = digitalRead(PIN_BUTTON);
  stableState  = lastRawState;
  lastChangeMs = millis();
}

void loop() {
  const uint32_t now = millis();
  const int rawState = digitalRead(PIN_BUTTON);

  // Jede rohe Pegelaenderung setzt den Entprell-Timer zurueck --
  // waehrend des Prellens wechselt raw staendig, lastChangeMs bleibt
  // dabei staendig "frisch" und stableState aendert sich nicht.
  if (rawState != lastRawState) {
    lastRawState = rawState;
    lastChangeMs = now;
  }

  // Erst wenn der Pin DEBOUNCE_MS lang ununterbrochen denselben Pegel
  // haelt, wird dieser Pegel als neuer stabiler Zustand uebernommen --
  // und zwar genau EINMAL beim Uebergang, nicht wiederholt waehrend
  // der Taster gehalten wird. Deshalb loest ein gehaltener Taster nur
  // einen Piep aus (Uebergang losgelassen->gedrueckt), keinen
  // Dauerton, und der Zaehler steigt nicht weiter.
  if ((now - lastChangeMs) >= DEBOUNCE_MS && rawState != stableState) {
    stableState = rawState;

    if (stableState == LOW) {
      ++pressCount;
      Serial.print("Taster gedrueckt #");
      Serial.println(pressCount);
      beep(BEEP_FREQ_HZ, BEEP_MS);
    } else {
      Serial.println("losgelassen");
    }
  }
}
