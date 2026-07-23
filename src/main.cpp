// =============================================================
//  main.cpp — Reale HAL-Implementierung. Nur env:esp32, nicht
//  Teil des nativen Test-Builds (test_build_src = no, Default).
// =============================================================
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <cstring>
#include "config.h"
#include "Hal.h"
#include "BootSelfTest.h"

class Esp32Hal : public Hal {
public:
  Esp32Hal()
      : display_(OLED_WIDTH, OLED_HEIGHT, &Wire, -1),
        oneWire_(PIN_ONEWIRE),
        sensors_(&oneWire_) {}

  void beginSensors() {
    sensors_.begin();
    sensors_.setResolution(SENSOR_RES_BITS);
  }

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

  float sensorReadTempC(const uint8_t rom[8]) override {
    uint8_t addr[8];
    std::memcpy(addr, rom, 8);
    sensors_.requestTemperaturesByAddress(addr);
    return sensors_.getTempC(addr);
  }

  void runOneWireDiscovery() override {
    Serial.println(F("Discovery-Modus: SENSOR_ROM in config.h eintragen:"));
    oneWire_.reset_search();
    uint8_t addr[8];
    while (oneWire_.search(addr)) {
      if (!sensorRomValid(addr)) {
        continue;
      }
      Serial.print(F("  { "));
      for (uint8_t i = 0; i < 8; ++i) {
        Serial.print(F("0x"));
        if (addr[i] < 0x10) {
          Serial.print('0');
        }
        Serial.print(addr[i], HEX);
        if (i < 7) {
          Serial.print(F(", "));
        }
      }
      Serial.println(F(" },"));
    }
    oneWire_.reset_search();
  }

private:
  Adafruit_SSD1306 display_;
  OneWire oneWire_;
  DallasTemperature sensors_;
  bool oledOk_ = false;
};

Esp32Hal hal;

void setup() {
  Serial.begin(115200);
  pinMode(PIN_SPEAKER, OUTPUT);
  Wire.begin();
  hal.beginSensors();

  runBootSelfTest(hal);
}

void loop() {
}
