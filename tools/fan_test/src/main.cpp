// =============================================================
//  tools/fan_test/src/main.cpp — Eigenstaendiger Sensor+Fan-Bring-up-
//  Test.
//
//  Zweck: EIN DS18B20 misst, EIN Noctua-Luefter wird per 25-kHz-PWM
//  ueber eine Test-Kennlinie geregelt, Temperatur/Duty/RPM auf dem
//  echten SSD1306. Keine Regelungs-Firmware, kein Hal -- bewusst
//  vollstaendig eigenstaendig, analog zu tools/display_test/,
//  tools/led_test/ und tools/sensor_test/.
//
//  Verkabelung:
//    1-Wire (DS18B20) : GPIO4,  4,7k Pull-up -> 3V3
//    PWM (Luefter blau): GPIO18, 25 kHz
//    Tacho (Luefter gruen): GPIO32, EXTERNER 10k Pull-up -> 3V3
//    OLED SSD1306     : SDA=21, SCL=22, 0x3C
//  Der Luefter haengt an einem SEPARATEN 5V-Netzteil, NICHT am
//  ESP32/USB. WICHTIG: GND von Netzteil und ESP32 MUESSEN verbunden
//  sein -- sonst haben PWM und Tacho keinen gemeinsamen Massebezug
//  und das Tacho-Signal ist unbrauchbar/undefiniert.
//
//  Bauen/Flashen: pio run -t upload (aus diesem Verzeichnis heraus)
// =============================================================
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <cstdio>

namespace {

// --- I2C / OLED -------------------------------------------------
constexpr uint8_t  PIN_SDA      = 21;
constexpr uint8_t  PIN_SCL      = 22;
constexpr uint8_t  OLED_ADDR    = 0x3C;
constexpr uint8_t  OLED_WIDTH   = 128;
constexpr uint8_t  OLED_HEIGHT  = 64;
constexpr uint32_t I2C_CLOCK_HZ = 50000;

// --- 1-Wire / DS18B20 ---------------------------------------------
constexpr uint8_t  PIN_ONEWIRE     = 4;  // 4,7k Pull-up -> 3V3
constexpr uint8_t  SENSOR_RES_BITS = 12;
constexpr uint32_t CYCLE_MS        = 1000;  // Sensor-Poll UND Tacho-Messfenster

// --- PWM (Luefter) -------------------------------------------------
constexpr uint8_t  PIN_PWM       = 18;
constexpr uint32_t PWM_FREQ_HZ   = 25000;
constexpr uint8_t  PWM_RES_BITS  = 8;

// --- Tacho -----------------------------------------------------
constexpr uint8_t  PIN_TACHO         = 32;  // externer 10k Pull-up -> 3V3
constexpr uint8_t  TACHO_PULSES_PER_REV = 2;  // Noctua-Standard

// --- Test-Kennlinie (siehe CLAUDE.md "Regelung", hier bewusst
// vereinfacht und bei 50% gedeckelt -- es geht nur ums Anlaufen und
// Erreichen der Rampen-Obergrenze) -----------------------------
constexpr float   FAN_ON_C      = 28.0f;  // darunter: aus
constexpr float   FAN_HALF_C    = 30.0f;  // ab hier: gedeckelt bei DUTY_HALF_PCT
constexpr uint8_t DUTY_MIN_PCT  = 20;     // Mindest-Anlaufwert bei FAN_ON_C
constexpr uint8_t DUTY_HALF_PCT = 50;     // Deckel fuer diesen Test
constexpr uint16_t KICKSTART_MS = 300;    // kurzer 100%-Puls beim Anlaufen aus dem Stillstand

// --- LEDC-PWM-Abstraktion, unabhaengig von der ESP32-Arduino-Core-
// Version (Core 3.x: ledcAttach(pin,...)+ledcWrite(pin,...); aeltere
// Cores: ledcSetup(channel,...)+ledcAttachPin(pin,channel)+
// ledcWrite(channel,...)) -- gleiches Muster wie in CLAUDE.md
// "Fallen" fuer den Speaker beschrieben, hier aber fuer den Luefter
// tatsaechlich per LEDC statt Bit-Banging umgesetzt, weil 25 kHz
// sauberes PWM braucht, keine Tonerzeugung mit wechselnder Frequenz.
#if !defined(ESP_ARDUINO_VERSION_MAJOR) || ESP_ARDUINO_VERSION_MAJOR < 3
constexpr uint8_t PWM_CHANNEL = 0;  // nur von der aelteren LEDC-API gebraucht
#endif

void pwmBegin(uint8_t pin, uint32_t freqHz, uint8_t resBits) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(pin, freqHz, resBits);
#else
  ledcSetup(PWM_CHANNEL, freqHz, resBits);
  ledcAttachPin(pin, PWM_CHANNEL);
#endif
}

void pwmWriteRaw(uint8_t pin, uint32_t duty) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(pin, duty);
#else
  (void)pin;
  ledcWrite(PWM_CHANNEL, duty);
#endif
}

uint32_t percentToDuty(uint8_t pct) {
  const uint32_t maxDuty = (1UL << PWM_RES_BITS) - 1;
  return (static_cast<uint32_t>(pct) * maxDuty) / 100;
}

// --- Test-Kennlinie: reine Funktion, Temperatur -> Ziel-Duty% -----
uint8_t curveDutyPct(float tempC) {
  if (tempC < FAN_ON_C) {
    return 0;
  }
  if (tempC < FAN_HALF_C) {
    const float t = (tempC - FAN_ON_C) / (FAN_HALF_C - FAN_ON_C);
    return static_cast<uint8_t>(DUTY_MIN_PCT + t * (DUTY_HALF_PCT - DUTY_MIN_PCT));
  }
  return DUTY_HALF_PCT;
}

// --- Globaler Zustand ---------------------------------------------
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
OneWire oneWire(PIN_ONEWIRE);
DallasTemperature sensors(&oneWire);

DeviceAddress romAddress{};
uint8_t deviceCount = 0;
bool sawValidReading = false;

uint32_t lastCycleMs = 0;
volatile uint32_t tachoPulseCount = 0;
uint16_t lastRpm = 0;

uint8_t currentTargetPct = 0;
bool fanWasRunning = false;
bool kickstartActive = false;
uint32_t kickstartEndMs = 0;

enum class TempStatus { Warten, Fehler, Ok };

// --- I2C-Bus-Recovery + Praesenz-Check (Pflicht, siehe CLAUDE.md
// Hardware-Fallen) --------------------------------------------------
void i2cRecover(uint8_t sdaPin, uint8_t sclPin) {
  pinMode(sdaPin, INPUT_PULLUP);
  pinMode(sclPin, OUTPUT);
  digitalWrite(sclPin, HIGH);
  delayMicroseconds(5);  // Pull-up einschwingen lassen

  uint8_t pulses = 0;
  for (; pulses < 9 && digitalRead(sdaPin) == LOW; ++pulses) {
    digitalWrite(sclPin, LOW);
    delayMicroseconds(5);
    digitalWrite(sclPin, HIGH);
    delayMicroseconds(5);
  }

  // STOP-Bedingung nur, wenn der Bus tatsaechlich haengt -- auf einem
  // gesunden Bus wuerde der manuelle SDA-Puls das OLED sonst unnoetig
  // waehrend seines eigenen Power-Ups stoeren.
  if (pulses > 0) {
    pinMode(sdaPin, OUTPUT);
    digitalWrite(sdaPin, LOW);
    delayMicroseconds(5);
    digitalWrite(sclPin, HIGH);
    delayMicroseconds(5);
    digitalWrite(sdaPin, HIGH);
    delayMicroseconds(5);
  }

  pinMode(sdaPin, INPUT_PULLUP);
  pinMode(sclPin, INPUT_PULLUP);
}

bool oledPresent() {
  Wire.beginTransmission(OLED_ADDR);
  return Wire.endTransmission() == 0;
}

void IRAM_ATTR onTachoFalling() {
  ++tachoPulseCount;
}

void formatRomShort(char* out, size_t outSize, const DeviceAddress addr) {
  std::snprintf(out, outSize, "ROM ..%02X%02X%02X%02X", addr[4], addr[5], addr[6], addr[7]);
}

void drawFrame(const char* tempText, uint8_t dutyPct, uint16_t rpm, const char* statusText) {
  char dutyLine[16];
  char rpmLine[16];
  std::snprintf(dutyLine, sizeof(dutyLine), "Duty %u%%", dutyPct);
  std::snprintf(rpmLine, sizeof(rpmLine), "%u rpm", rpm);

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Sensor+Fan Test");

  display.setTextSize(2);
  display.setCursor(0, 14);
  display.print(tempText);

  display.setTextSize(1);
  display.setCursor(0, 36);
  display.print(dutyLine);

  display.setCursor(0, 46);
  display.print(rpmLine);

  display.setCursor(0, 56);
  display.print(statusText);

  display.display();
}

}  // namespace

void setup() {
  Serial.begin(115200);

  i2cRecover(PIN_SDA, PIN_SCL);
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(I2C_CLOCK_HZ);
  Wire.setTimeOut(50);

  const bool present = oledPresent();
  Serial.println("Sensor+Fan-Test aktiv");
  Serial.printf("I2C-Praesenzcheck 0x%02X -> %s\n", OLED_ADDR,
                present ? "gefunden" : "NICHT gefunden");

  if (!present || !display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED-Init fehlgeschlagen -- Test angehalten.");
    while (true) {
      delay(1000);  // reiner Diagnose-Halt, kein Regelkreis
    }
  }
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  sensors.begin();
  deviceCount = sensors.getDeviceCount();
  Serial.printf("DS18B20 gefunden: %u\n", deviceCount);
  if (deviceCount > 0 && sensors.getAddress(romAddress, 0)) {
    Serial.print("ROM-Adresse: ");
    for (uint8_t i = 0; i < 8; ++i) {
      if (romAddress[i] < 0x10) Serial.print('0');
      Serial.print(romAddress[i], HEX);
      if (i < 7) Serial.print(':');
    }
    Serial.println();
    sensors.setResolution(romAddress, SENSOR_RES_BITS);
  }

  pwmBegin(PIN_PWM, PWM_FREQ_HZ, PWM_RES_BITS);
  pwmWriteRaw(PIN_PWM, 0);

  pinMode(PIN_TACHO, INPUT);  // externer Pull-up vorausgesetzt, siehe CLAUDE.md
  attachInterrupt(digitalPinToInterrupt(PIN_TACHO), onTachoFalling, FALLING);

  drawFrame("--", 0, 0, "AUS");
}

void loop() {
  // PWM jede Schleife anwenden (nicht nur im Zyklus-Takt), damit der
  // Kickstart-Zeitpunkt (KICKSTART_MS) millis()-praezise endet.
  const uint32_t nowFast = millis();
  uint8_t dutyToApply = currentTargetPct;
  if (kickstartActive) {
    if (nowFast < kickstartEndMs) {
      dutyToApply = 100;
    } else {
      kickstartActive = false;
      dutyToApply = currentTargetPct;
    }
  }
  pwmWriteRaw(PIN_PWM, percentToDuty(dutyToApply));

  const uint32_t now = millis();
  if (now - lastCycleMs < CYCLE_MS) {
    return;
  }
  lastCycleMs = now;

  // --- Tacho-Fenster auswerten ---
  noInterrupts();
  const uint32_t pulses = tachoPulseCount;
  tachoPulseCount = 0;
  interrupts();
  lastRpm = static_cast<uint16_t>((pulses * 60000UL) / (TACHO_PULSES_PER_REV * CYCLE_MS));

  // --- Sensor lesen + Status bestimmen ---
  TempStatus status;
  float tempC = 0.0f;
  if (deviceCount == 0) {
    status = TempStatus::Fehler;
  } else {
    sensors.requestTemperatures();
    const float raw = sensors.getTempCByIndex(0);
    if (raw == DEVICE_DISCONNECTED_C) {
      status = TempStatus::Fehler;
    } else if (!sawValidReading && raw == 85.0f) {
      status = TempStatus::Warten;
    } else {
      status = TempStatus::Ok;
      tempC = raw;
      sawValidReading = true;
    }
  }

  // --- Ziel-Duty aktualisieren (ausser im "warte"-Zustand: nicht
  // regeln, bis ein echter Wert kommt) ---
  bool updateTarget = true;
  uint8_t newTargetPct = currentTargetPct;
  switch (status) {
    case TempStatus::Fehler:
      newTargetPct = DUTY_HALF_PCT;  // im Zweifel kuehlen
      break;
    case TempStatus::Warten:
      updateTarget = false;
      break;
    case TempStatus::Ok:
      newTargetPct = curveDutyPct(tempC);
      break;
  }

  if (updateTarget) {
    const bool nowRunning = newTargetPct > 0;
    if (nowRunning && !fanWasRunning) {
      // Anlauf aus dem Stillstand -- kurz auf 100% puffern.
      kickstartActive = true;
      kickstartEndMs = now + KICKSTART_MS;
    }
    currentTargetPct = newTargetPct;
    fanWasRunning = nowRunning;
  }

  // --- Anzeige + Serial ---
  char tempText[12];
  const char* statusLine;
  if (status == TempStatus::Fehler) {
    std::snprintf(tempText, sizeof(tempText), "Sensor?");
    Serial.println("Fehler: Sensor fehlt oder DEVICE_DISCONNECTED (-127)");
  } else if (status == TempStatus::Warten) {
    std::snprintf(tempText, sizeof(tempText), "warte...");
    Serial.println("warte auf erste Conversion (85.00 Power-On-Default)");
  } else {
    std::snprintf(tempText, sizeof(tempText), "%.1foC", static_cast<double>(tempC));
  }

  if (currentTargetPct == 0) {
    statusLine = "AUS";
  } else if (currentTargetPct >= DUTY_HALF_PCT) {
    statusLine = "50%";
  } else {
    statusLine = "Rampe";
  }

  Serial.printf("T=%s Duty=%u%% RPM=%u\n", tempText, currentTargetPct, lastRpm);
  drawFrame(tempText, currentTargetPct, lastRpm, statusLine);
}
