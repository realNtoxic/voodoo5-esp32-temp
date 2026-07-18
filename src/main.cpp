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

  void beginFans() {
    for (uint8_t i = 0; i < 2; ++i) {
      ledcAttach(FAN[i].pwmPin, FAN_PWM_FREQ_HZ, FAN_PWM_RESOLUTION);
      pinMode(FAN[i].tachoPin, INPUT_PULLUP);
    }
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

  void fanSetDutyPercent(uint8_t fanIndex, uint8_t dutyPercent) override {
    const uint32_t maxDuty = (1u << FAN_PWM_RESOLUTION) - 1;
    const uint32_t duty = (maxDuty * dutyPercent) / 100;
    ledcWrite(FAN[fanIndex].pwmPin, duty);
  }

  uint16_t fanReadRpm(uint8_t fanIndex) override {
    tachoEdgeCount_[fanIndex] = 0;
    const uint8_t pin = FAN[fanIndex].tachoPin;
    attachInterruptArg(digitalPinToInterrupt(pin), &Esp32Hal::onTachoEdge,
                        (void*)&tachoEdgeCount_[fanIndex], FALLING);
    delay(TACHO_WIN_MS);
    detachInterrupt(digitalPinToInterrupt(pin));

    const uint32_t edges = tachoEdgeCount_[fanIndex];
    return static_cast<uint16_t>(
        (edges * 60000UL) / (static_cast<uint32_t>(TACHO_PULSES_PER_REV) * TACHO_WIN_MS));
  }

private:
  static void IRAM_ATTR onTachoEdge(void* arg) {
    volatile uint32_t* counter = static_cast<volatile uint32_t*>(arg);
    ++(*counter);
  }

  Adafruit_SSD1306 display_;
  OneWire oneWire_;
  DallasTemperature sensors_;
  bool oledOk_ = false;
  volatile uint32_t tachoEdgeCount_[2] = { 0, 0 };
};

Esp32Hal hal;

void setup() {
  Serial.begin(115200);
  pinMode(PIN_SPEAKER, OUTPUT);
  Wire.begin();
  hal.beginSensors();
  hal.beginFans();

  runBootSelfTest(hal);
}

void loop() {
}
