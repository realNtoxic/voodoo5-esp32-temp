// =============================================================
//  tools/led_test/src/main.cpp — Eigenstaendiger RGB-LED-Bring-up-Test.
//
//  Zweck: NUR pruefen, ob eine adressierbare PL9823-F5 (WS2812B-
//  kompatibel) an GPIO16 korrekt angesteuert wird UND welche
//  Byte-Farbreihenfolge das konkrete Bauteil tatsaechlich erwartet
//  (siehe unten) -- keine Regelung, kein Hal/ILed aus der
//  Hauptfirmware, bewusst vollstaendig eigenstaendig, analog zu
//  tools/display_test/.
//
//  Ersetzt die fruehere einfarbige LED an GPIO2: die neue Hardware
//  ist eine adressierbare RGB-LED (eigenes Protokoll ueber einen
//  einzelnen Datenpin), kein einfacher Ein/Aus-Ausgang mehr.
//
//  Bauen/Flashen: pio run -t upload (aus diesem Verzeichnis heraus)
// =============================================================
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

namespace {

// --- Hardware -----------------------------------------------------
// PL9823-F5 (WS2812B-kompatibel), Datenpin (DIN) an GPIO16. VDD an
// eigener +5V-Quelle, GND ZWINGEND gemeinsam mit dem ESP32 -- sonst
// hat DIN keinen Bezugspunkt und das Signal ist undefiniert.
//
// DIN erwartet laut Datenblatt strenggenommen 5V-Logik, der ESP32
// liefert nur 3,3V. Bei EINER LED und kurzer Leitung funktioniert das
// in der Praxis meist trotzdem (3,3V liegt meist noch ueber der
// High-Schwelle der LED). Flackert die LED oder reagiert gar nicht:
// Pegelwandler (z. B. 74HCT125) zwischen GPIO16 und DIN einschleifen,
// oder die LED mit ~4V statt 5V versorgen (senkt die noetige
// High-Schwelle). Reines Hardware-Thema -- kein Codefix moeglich.
constexpr uint8_t PIN_LED_DATA = 16;
constexpr uint8_t NUM_PIXELS   = 1;

// Helligkeit begrenzen: eine 5mm-LED auf voller Helligkeit (255)
// blendet unangenehm und zieht unnoetig Strom. ~40% reicht locker, um
// alle Testfarben sicher zu unterscheiden.
constexpr uint8_t BRIGHTNESS = 100;  // von 255, ~40 %

// ACHTUNG Farbreihenfolge: NEO_GRB ist die Werkseinstellung fuer die
// meisten WS2812B/PL9823-Module, aber nicht garantiert -- manche
// PL9823-Chargen sind tatsaechlich RGB statt GRB. Genau DESHALB gibt
// dieser Test bei jeder Phase den SOLL-Farbnamen ueber Serial aus:
// leuchtet die LED bei "soll: ROT" z. B. gruen, steht hier die
// falsche Reihenfolge -- dann NEO_GRB durch NEO_RGB ersetzen (oder die
// passende Permutation, siehe Adafruit_NeoPixel.h fuer alle
// NEO_*-Konstanten). Ziel dieses Tests ist genau diese Reihenfolge
// fuer die spaetere Statuslogik festzunageln, nicht sie zu erraten.
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
  { "WEISS", 255, 255, 255 },
  { "AUS",   0,   0,   0   },
};
constexpr uint8_t  PHASE_COUNT = sizeof(PHASES) / sizeof(PHASES[0]);
constexpr uint32_t PHASE_MS    = 1000;  // ~1s je Farbe, siehe Aufgabenstellung

uint32_t lastPhaseSwitchMs = 0;
uint8_t  phaseIndex = 0;

void showPhase(uint8_t index) {
  const ColorPhase& p = PHASES[index];
  strip.setPixelColor(0, strip.Color(p.r, p.g, p.b));
  strip.show();
  Serial.print("soll: ");
  Serial.println(p.name);
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
