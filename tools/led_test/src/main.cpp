// =============================================================
//  tools/led_test/src/main.cpp — Eigenstaendiger LED-Bring-up-Test.
//
//  Zweck: NUR pruefen, ob die LED an GPIO2 auf echter Hardware im
//  richtigen Muster blinkt -- Heartbeat (ruhiger Ein/Aus-Takt) und
//  Fault (schnelles Doppelblinken), abwechselnd alle
//  MODE_SWITCH_MS, ganz ohne Taster/Sensoren. Keine Regelung, kein
//  Hal/ILed aus der Hauptfirmware -- bewusst vollstaendig
//  eigenstaendig, analog zu tools/display_test/.
//
//  Die Muster-Logik (ledLevel/selectLedMode) ist 1:1 aus
//  lib/Led/LedPattern.cpp uebernommen, hier aber als lokale Kopie
//  ohne Include -- exakt dasselbe Prinzip wie beim Display-Test:
//  ein eigenstaendiges Tool darf nicht von einem Include-Pfad
//  ausserhalb seines eigenen Projekts abhaengen (siehe
//  tools/display_test/ -- genau das hat dort zuverlaessig Aerger
//  gemacht). Aendert sich das Muster in lib/Led/, muss diese Kopie
//  von Hand nachgezogen werden; die eigentliche, verbindliche Logik
//  bleibt lib/Led/LedPattern.cpp samt ihren Unity-Tests.
//
//  Bauen/Flashen: pio run -t upload (aus diesem Verzeichnis heraus)
// =============================================================
#include <Arduino.h>

namespace {

// --- Hardware -----------------------------------------------------
constexpr uint8_t PIN_LED = 2;

// --- Timing (aus config.h uebernommen) -----------------------------
constexpr uint16_t LED_FAULT_ON_MS    = 200;
constexpr uint16_t LED_FAULT_GAP_MS   = 200;
constexpr uint16_t LED_FAULT_PAUSE_MS = 600;
constexpr uint16_t BLINK_MS           = 500;   // Heartbeat-Halbperiode

// Wie lange dieses Tool jeweils in einem Modus bleibt, bevor es zur
// Vorfuehrung automatisch umschaltet -- nur fuers Tool, keine
// CLAUDE.md-Konstante.
constexpr uint32_t MODE_SWITCH_MS = 5000;

enum class LedMode : uint8_t { Heartbeat, Fault };

LedMode selectLedMode(bool faultActive) {
  return faultActive ? LedMode::Fault : LedMode::Heartbeat;
}

bool ledLevel(LedMode mode, uint8_t phase, uint32_t nowMs) {
  if (mode == LedMode::Heartbeat) {
    return phase != 0;
  }

  const uint32_t onGapOn = static_cast<uint32_t>(LED_FAULT_ON_MS)
                          + LED_FAULT_GAP_MS + LED_FAULT_ON_MS;
  const uint32_t period = onGapOn + LED_FAULT_PAUSE_MS;
  const uint32_t t = nowMs % period;

  if (t < LED_FAULT_ON_MS) return true;
  if (t < static_cast<uint32_t>(LED_FAULT_ON_MS) + LED_FAULT_GAP_MS) return false;
  if (t < onGapOn) return true;
  return false;
}

LedMode lastMode = LedMode::Heartbeat;

}  // namespace

void setup() {
  Serial.begin(115200);
  // GPIO2 ist ein Strapping-Pin, dessen Wert nur waehrend des
  // ROM-Bootloaders vor Arduino-setup() eine Rolle spielt -- der ist
  // hier bereits abgeschlossen, sobald setup() laeuft. Anders als in
  // der Hauptfirmware gibt es in diesem Tool keinen eigenen
  // Boot-Selbsttest, der vorher noch laufen muesste.
  pinMode(PIN_LED, OUTPUT);
  Serial.println("LED-Test aktiv");
  Serial.println("Heartbeat (ruhig) <-> Fault (Doppelblink), Wechsel alle 5s");
}

void loop() {
  const uint32_t now = millis();
  const bool faultActive = (now / MODE_SWITCH_MS) % 2 == 1;
  const LedMode mode = selectLedMode(faultActive);

  if (mode != lastMode) {
    lastMode = mode;
    Serial.println(mode == LedMode::Fault ? "-> Fault (Doppelblink)" : "-> Heartbeat");
  }

  const uint8_t phase = static_cast<uint8_t>((now / BLINK_MS) % 2);
  digitalWrite(PIN_LED, ledLevel(mode, phase, now) ? HIGH : LOW);
}
