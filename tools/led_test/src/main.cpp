// =============================================================
//  tools/led_test/src/main.cpp — Eigenstaendiger RGB-LED-Bring-up-Test.
//
//  Zweck: NUR pruefen, ob zwei in Reihe geschaltete, adressierbare
//  PL9823-F5 (WS2812B-kompatibel) an GPIO16 korrekt einzeln
//  angesteuert werden UND welche Byte-Farbreihenfolge das konkrete
//  Bauteil tatsaechlich erwartet (siehe unten) -- keine Regelung, kein
//  Hal/ILed aus der Hauptfirmware, bewusst vollstaendig eigenstaendig,
//  analog zu tools/display_test/.
//
//  LED2 zeigt immer die Phase, die LED1 einen Schritt zuvor hatte (fest
//  um 1 versetzt) -- so ist auf einen Blick sichtbar, dass beide LEDs
//  der Kette unabhaengig voneinander angesteuert werden, nicht nur
//  synchron dieselbe Farbe zeigen.
//
//  Ersetzt die fruehere einfarbige LED an GPIO2: die neue Hardware
//  sind adressierbare RGB-LEDs (eigenes Protokoll ueber einen
//  einzelnen Datenpin, in Reihe durchgeschleift), kein einfacher
//  Ein/Aus-Ausgang mehr.
//
//  Bauen/Flashen: pio run -t upload (aus diesem Verzeichnis heraus)
// =============================================================
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

namespace {

// --- Hardware -----------------------------------------------------
// 2x PL9823-F5 (WS2812B-kompatibel) in Reihe: GPIO16 -> DIN LED1,
// DOUT LED1 -> DIN LED2, DOUT LED2 bleibt frei (Ende der Kette). VDD
// beider LEDs an derselben +5V-Quelle, GND ZWINGEND gemeinsam mit dem
// ESP32 -- sonst hat DIN keinen Bezugspunkt und das Signal ist
// undefiniert.
//
// DIN bekommt vom ESP32 nur 3,3V-Logik, waehrend die LED strenggenommen
// 5V-Logik erwartet. Bei kurzer Leitung zu LED1 funktioniert das in der
// Praxis meist trotzdem (3,3V liegt meist noch ueber der High-Schwelle
// der LED); LED2 haengt ohnehin am 5V-Ausgangssignal von LED1, nicht
// mehr an GPIO16, ist also unkritisch. Flackert LED1 oder reagiert gar
// nicht: eine 1N4148 in Durchlassrichtung in die VDD-Leitung legen
// (LED laeuft dann an ~4,3V statt 5V, senkt die noetige High-Schwelle)
// oder einen Pegelwandler zwischen GPIO16 und DIN LED1 einschleifen.
// Reines Hardware-Thema -- kein Codefix moeglich.
constexpr uint8_t PIN_LED_DATA = 16;
constexpr uint8_t NUM_PIXELS   = 2;

// Helligkeit begrenzen: eine 5mm-LED auf voller Helligkeit (255)
// blendet unangenehm und zieht unnoetig Strom. ~30-40% reicht locker,
// um alle Testfarben sicher zu unterscheiden.
constexpr uint8_t BRIGHTNESS = 100;  // von 255, ~40 %

// ACHTUNG Farbreihenfolge: NEO_GRB ist die Werkseinstellung fuer die
// meisten WS2812B/PL9823-Module, aber nicht garantiert -- manche
// PL9823-Chargen sind tatsaechlich RGB statt GRB. Genau DESHALB gibt
// dieser Test bei jeder Phase je LED den SOLL-Farbnamen ueber Serial
// aus: leuchtet z. B. LED1 bei "LED1 soll: ROT" gruen, steht hier die
// falsche Reihenfolge -- dann NEO_GRB durch NEO_RGB ersetzen (oder die
// passende Permutation, siehe Adafruit_NeoPixel.h fuer alle
// NEO_*-Konstanten). Gilt fuer beide LEDs gleichermassen, es gibt nur
// eine strip-Instanz mit einer gemeinsamen Farbreihenfolge. Ziel dieses
// Tests ist genau diese Reihenfolge fuer die spaetere Statuslogik
// festzunageln, nicht sie zu erraten.
Adafruit_NeoPixel strip(NUM_PIXELS, PIN_LED_DATA, NEO_GRB + NEO_KHZ800);

struct ColorPhase {
  const char* name;
  uint8_t     r;
  uint8_t     g;
  uint8_t     b;
};

constexpr ColorPhase PHASES[] = {
  { "ROT",   255, 0,   0   },
  { "GRUEN", 0,   255, 0   },
  { "BLAU",  0,   0,   255 },
  { "AUS",   0,   0,   0   },
};
constexpr uint8_t  PHASE_COUNT = sizeof(PHASES) / sizeof(PHASES[0]);
constexpr uint32_t PHASE_MS    = 1000;  // ~1s je Farbe, siehe Aufgabenstellung

uint32_t lastPhaseSwitchMs = 0;
uint8_t  phaseIndex = 0;

void showPhase(uint8_t index) {
  // LED2 zeigt fest die vorherige Phase von LED1 (um genau 1 versetzt,
  // zyklisch) -- Modulo-Rueckwaertszaehlung statt "- 1", damit index=0
  // nicht in negative Zahlen bzw. Unterlauf laeuft (uint8_t).
  const uint8_t indexLed1 = index;
  const uint8_t indexLed2 = static_cast<uint8_t>((index + PHASE_COUNT - 1) % PHASE_COUNT);
  const ColorPhase& p1 = PHASES[indexLed1];
  const ColorPhase& p2 = PHASES[indexLed2];

  strip.setPixelColor(0, strip.Color(p1.r, p1.g, p1.b));
  strip.setPixelColor(1, strip.Color(p2.r, p2.g, p2.b));
  strip.show();

  Serial.print("LED1 soll: ");
  Serial.println(p1.name);
  Serial.print("LED2 soll: ");
  Serial.println(p2.name);
}

}  // namespace

void setup() {
  Serial.begin(115200);

  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  // Sofort einen definierten Zustand setzen (alle aus), BEVOR
  // irgendetwas anderes passiert -- der PL9823 hat nach dem Einschalten
  // einen zufaelligen/undefinierten Pixelinhalt, der sich sonst leicht
  // als Testergebnis missverstehen liesse.
  strip.clear();
  strip.show();

  Serial.println("LED-Test aktiv, Pin 16");

  lastPhaseSwitchMs = millis();
  showPhase(phaseIndex);
}

void loop() {
  // millis()-getaktet, kein delay() -- Projektstil (siehe CLAUDE.md
  // Architektur-Regel 4), obwohl dieses Wegwerf-Tool selbst keinen
  // Regelkreis hat.
  const uint32_t now = millis();
  if (now - lastPhaseSwitchMs >= PHASE_MS) {
    lastPhaseSwitchMs = now;
    phaseIndex = static_cast<uint8_t>((phaseIndex + 1) % PHASE_COUNT);
    showPhase(phaseIndex);
  }
}
