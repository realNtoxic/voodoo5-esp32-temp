// =============================================================
//  main.cpp — Reale HAL-Implementierung. Nur env:esp32, nicht
//  Teil des nativen Test-Builds (test_build_src = no, Default).
// =============================================================
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"
#include "Hal.h"
#include "BootSelfTest.h"

class Esp32Hal : public Hal {
public:
  Esp32Hal() : display_(OLED_WIDTH, OLED_HEIGHT, &Wire, -1) {}

  void beep(uint16_t freqHz, uint16_t ms) override {
    // tone()/noTone() sind auf dem ESP32-Arduino-Core unzuverlaessig bei
    // schnell aufeinanderfolgenden Aufrufen (zweiter Ton startet oft nicht
    // neu), und die LEDC-API unterscheidet sich je nach Core-Version
    // (ledcAttach(pin,...) vs. ledcSetup+ledcAttachPin(pin, channel)).
    // Statt uns auf eine bestimmte Core-Version festzulegen, erzeugen wir
    // die Rechteckwelle direkt per digitalWrite -- funktioniert ueberall
    // identisch und haengt an keiner Tonerzeugungs-API.
    const uint32_t halfPeriodUs = 500000UL / freqHz;
    const uint32_t cycles = (static_cast<uint32_t>(ms) * 1000UL) / (2 * halfPeriodUs);
    for (uint32_t i = 0; i < cycles; ++i) {
      digitalWrite(PIN_SPEAKER, HIGH);
      delayMicroseconds(halfPeriodUs);
      digitalWrite(PIN_SPEAKER, LOW);
      delayMicroseconds(halfPeriodUs);
    }
  }

  void delayMs(uint32_t ms) override {
    delay(ms);
  }

  bool oledInit() override {
    // Erst pruefen, ob ueberhaupt ein Geraet auf der Adresse antwortet.
    // Ohne angeschlossenes OLED haengen SDA/SCL frei -> ohne diesen
    // Vorab-Check und Wire.setTimeOut() (siehe setup()) kann der
    // I2C-Treiber hier unbegrenzt blockieren, statt sauber false zu
    // liefern, und der Rest des Selbsttests (Fehlerpiepse!) wuerde nie
    // erreicht.
    Wire.beginTransmission(OLED_ADDR);
    if (Wire.endTransmission() != 0) {
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
  Wire.begin();
  Wire.setTimeOut(50);  // ms; verhindert unbegrenztes Haengen ohne OLED

  runBootSelfTest(hal);
}

void loop() {
}
