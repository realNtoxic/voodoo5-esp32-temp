// =============================================================
//  main.cpp — Reale HAL-Implementierung. Nur env:esp32, nicht
//  Teil des nativen Test-Builds (test_build_src = no, Default).
// =============================================================
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "config.h"
#include "Hal.h"
#include "BootSelfTest.h"

namespace {

// tone()/noTone() sind auf dem ESP32-Arduino-Core unzuverlaessig bei
// schnell aufeinanderfolgenden Aufrufen (zweiter Ton startet oft nicht
// neu), und die LEDC-API unterscheidet sich je nach Core-Version
// (ledcAttach(pin,...) vs. ledcSetup+ledcAttachPin(pin, channel)).
// Statt uns auf eine bestimmte Core-Version festzulegen, erzeugen wir
// die Rechteckwelle direkt per digitalWrite -- funktioniert ueberall
// identisch und haengt an keiner Tonerzeugungs-API. Wird sowohl vom
// blockierenden beep() als auch von der beepAsync()-Task genutzt.
void playSquareWave(uint8_t pin, uint16_t freqHz, uint16_t ms) {
  const uint32_t halfPeriodUs = 500000UL / freqHz;
  const uint32_t cycles = (static_cast<uint32_t>(ms) * 1000UL) / (2 * halfPeriodUs);
  for (uint32_t i = 0; i < cycles; ++i) {
    digitalWrite(pin, HIGH);
    delayMicroseconds(halfPeriodUs);
    digitalWrite(pin, LOW);
    delayMicroseconds(halfPeriodUs);
  }
}

struct BeepTaskParams {
  uint8_t pin;
  uint16_t freqHz;
  uint16_t ms;
};

// Laeuft als eigene FreeRTOS-Task (siehe Esp32Hal::beepAsync) und
// beendet sich danach selbst.
void beepTask(void* arg) {
  BeepTaskParams* params = static_cast<BeepTaskParams*>(arg);
  playSquareWave(params->pin, params->freqHz, params->ms);
  delete params;
  vTaskDelete(nullptr);
}

}  // namespace

class Esp32Hal : public Hal {
public:
  Esp32Hal() : display_(OLED_WIDTH, OLED_HEIGHT, &Wire, -1) {}

  void beep(uint16_t freqHz, uint16_t ms) override {
    playSquareWave(PIN_SPEAKER, freqHz, ms);
  }

  void beepAsync(uint16_t freqHz, uint16_t ms) override {
    // Laufbetrieb (Wiederhol-Alarm, Ack-Taster): darf den Regelkreis
    // nicht blockieren, siehe CLAUDE.md Abschnitt "Akustik". Deshalb
    // eigene Task auf Core 0 -- die Hauptschleife (Core 1, Arduino-Loop)
    // stoesst nur an und kehrt sofort zurueck.
    auto* params = new BeepTaskParams{ PIN_SPEAKER, freqHz, ms };
    xTaskCreatePinnedToCore(&beepTask, "beepAsync", 2048, params, 1, nullptr, 0);
  }

  void delayMs(uint32_t ms) override {
    delay(ms);
  }

  void i2cRecover(uint8_t sdaPin, uint8_t sclPin) override {
    // Bus-Recovery per Bit-Banging (I2C-Spec-Standardverfahren): Startet
    // der ESP32 mitten in einer Uebertragung neu, kann das durchgehend
    // versorgte SSD1306 SDA auf LOW haengen lassen, bis es sein
    // angefangenes Byte zu Ende getaktet bekommt. Muss VOR Wire.begin()
    // laufen, solange die Pins noch als einfache GPIOs ansprechbar sind
    // (nicht bereits vom I2C-Peripheriegeraet belegt).
    pinMode(sdaPin, INPUT_PULLUP);
    pinMode(sclPin, OUTPUT);
    digitalWrite(sclPin, HIGH);
    // Pull-up einschwingen lassen -- ohne das kann ein sofortiges
    // digitalRead() auf einem eigentlich gesunden, nur noch nicht
    // aufgeladenen Bus faelschlich LOW liefern und unnoetig Takte
    // erzeugen.
    delayMicroseconds(5);

    uint8_t pulses = 0;
    for (; pulses < 9 && digitalRead(sdaPin) == LOW; ++pulses) {
      digitalWrite(sclPin, LOW);
      delayMicroseconds(5);
      digitalWrite(sclPin, HIGH);
      delayMicroseconds(5);
    }
    Serial.printf("i2cRecover: %u Takt(e), SDA danach %s\n", pulses,
                  digitalRead(sdaPin) == HIGH ? "HIGH" : "LOW");

    // Explizite STOP-Bedingung: SDA LOW -> SCL HIGH -> SDA HIGH.
    pinMode(sdaPin, OUTPUT);
    digitalWrite(sdaPin, LOW);
    delayMicroseconds(5);
    digitalWrite(sclPin, HIGH);
    delayMicroseconds(5);
    digitalWrite(sdaPin, HIGH);
    delayMicroseconds(5);

    // Bus in definiertem Idle-High-Zustand uebergeben (nicht floatend!) --
    // Wire.begin() erwartet danach ein sauberes High auf beiden Leitungen.
    pinMode(sdaPin, INPUT_PULLUP);
    pinMode(sclPin, INPUT_PULLUP);
  }

  bool oledInit() override {
    // Wire.begin() lebt bewusst hier (statt einmalig in setup()), damit
    // jeder oledInit()-Aufruf -- auch ein periodischer Re-Init durch den
    // spaeteren Health-Monitor nach OLED-Ausfall -- denselben Ablauf
    // durchlaeuft: i2cRecover() (siehe selfTestOled()) laeuft dann immer
    // unmittelbar VOR diesem Wire.begin().
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(I2C_CLOCK_HZ);  // robuster bei langen Steckbrett-Leitungen
    Wire.setTimeOut(50);  // ms; verhindert unbegrenztes Haengen ohne OLED

    // Erst pruefen, ob ueberhaupt ein Geraet auf der Adresse antwortet.
    // Ohne angeschlossenes OLED haengen SDA/SCL frei -> ohne diesen
    // Vorab-Check und Wire.setTimeOut() kann der I2C-Treiber hier
    // unbegrenzt blockieren, statt sauber false zu liefern, und der Rest
    // des Selbsttests (Fehlerpiepse!) wuerde nie erreicht.
    Wire.beginTransmission(OLED_ADDR);
    const uint8_t i2cResult = Wire.endTransmission();
    Serial.printf("oledInit: Scan 0x%02X -> %u\n", OLED_ADDR, i2cResult);
    if (i2cResult != 0) {
      oledOk_ = false;
      return false;
    }

    oledOk_ = display_.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
    if (oledOk_) {
      display_.clearDisplay();
      display_.display();
    }
    return oledOk_;
  }

  void oledShowLine(uint8_t row, const char* text) override {
    if (!oledOk_) return;
    display_.setCursor(0, row * 8);
    display_.print(text);
    display_.display();
  }

private:
  Adafruit_SSD1306 display_;
  bool oledOk_ = false;
};

Esp32Hal hal;

void setup() {
  Serial.begin(115200);
  pinMode(PIN_SPEAKER, OUTPUT);

  runBootSelfTest(hal);
}

void loop() {
}
