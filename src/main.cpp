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
    tone(PIN_SPEAKER, freqHz);
    delay(ms);
    noTone(PIN_SPEAKER);
  }

  void delayMs(uint32_t ms) override {
    delay(ms);
  }

  bool oledInit() override {
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

  runBootSelfTest(hal);
}

void loop() {
}
