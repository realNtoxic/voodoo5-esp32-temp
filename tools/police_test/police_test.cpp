// tools/police_test/police_test.ino  (bzw. .cpp)
// Wegwerf-Test: rot-blaues Polizei-Blitzmuster mit 2 PL9823 an GPIO16.
// LED0 = rote Seite, LED1 = blaue Seite. millis()-basiert, kein delay().

#include <Adafruit_NeoPixel.h>

#define LED_PIN   16
#define NUM_LEDS  2
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_RGB + NEO_KHZ800);

// --- Muster-Definition -------------------------------------------------
// Jeder Schritt: Dauer (ms), Zustand LED0 (rot an?), Zustand LED1 (blau an?)
struct Step { uint16_t ms; bool red; bool blue; };

const Step SEQ[] = {
  // rote Seite: 3 schnelle Blitze
  { 60, true,  false }, { 60, false, false },
  { 60, true,  false }, { 60, false, false },
  { 60, true,  false }, { 120, false, false },  // kurze Pause
  // blaue Seite: 3 schnelle Blitze
  { 60, false, true  }, { 60, false, false },
  { 60, false, true  }, { 60, false, false },
  { 60, false, true  }, { 120, false, false },  // kurze Pause
  // schneller Wechsel-Doppelschlag (beide Seiten im Stakkato)
  { 50, true,  false }, { 50, false, true  },
  { 50, true,  false }, { 50, false, true  },
  { 150, false, false },                         // Atempause
};
const uint8_t SEQ_LEN = sizeof(SEQ) / sizeof(SEQ[0]);

uint8_t  stepIdx    = 0;
uint32_t stepStart  = 0;

const uint8_t BRIGHT = 120;  // ~47% – blendet nicht, gut sichtbar

void applyStep(const Step& s) {
  strip.setPixelColor(0, s.red  ? strip.Color(255, 0, 0) : 0);  // LED0 rot
  strip.setPixelColor(1, s.blue ? strip.Color(0, 0, 255) : 0);  // LED1 blau
  strip.show();
}

void setup() {
  Serial.begin(115200);
  Serial.println("Police-Test aktiv, Pin 16, 2 LEDs");
  strip.begin();
  strip.setBrightness(BRIGHT);
  strip.clear();
  strip.show();               // definierter Startzustand: aus
  stepStart = millis();
  applyStep(SEQ[0]);
}

void loop() {
  uint32_t now = millis();
  if (now - stepStart >= SEQ[stepIdx].ms) {
    stepIdx   = (stepIdx + 1) % SEQ_LEN;
    stepStart = now;
    applyStep(SEQ[stepIdx]);
  }
  // hier bliebe Platz für andere nicht-blockierende Aufgaben
}
