// =============================================================
//  tools/sensor_test/src/main.cpp — Eigenstaendiger DS18B20-Bring-up-
//  Test.
//
//  Zweck: NUR pruefen, ob EIN DS18B20 am 1-Wire-Bus (GPIO4) erkannt
//  wird und plausible Temperaturen liefert -- Anzeige auf dem echten
//  SSD1306. Keine Regelung, kein Hal aus der Hauptfirmware -- bewusst
//  vollstaendig eigenstaendig, analog zu tools/display_test/ und
//  tools/led_test/.
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
constexpr uint8_t  PIN_ONEWIRE   = 4;     // wie im Hauptprojekt, 4,7k Pull-up -> 3V3
constexpr uint8_t  SENSOR_RES_BITS = 12;
constexpr uint32_t POLL_MS       = 1000;  // millis()-basiert, kein delay() im Loop

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
OneWire oneWire(PIN_ONEWIRE);
DallasTemperature sensors(&oneWire);

DeviceAddress romAddress{};
uint8_t deviceCount = 0;
bool sawValidReading = false;  // fuer den 85.00-Power-On-Default-Sonderfall
uint32_t lastPollMs = 0;

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

void printRomAddress(const DeviceAddress addr) {
  for (uint8_t i = 0; i < 8; ++i) {
    if (addr[i] < 0x10) {
      Serial.print('0');
    }
    Serial.print(addr[i], HEX);
    if (i < 7) {
      Serial.print(':');
    }
  }
  Serial.println();
}

// Gekuerzte ROM-Anzeige fuers Display: letzte 4 Bytes reichen zum
// Wiedererkennen auf dem Steckbrett.
void formatRomShort(char* out, size_t outSize, const DeviceAddress addr) {
  std::snprintf(out, outSize, "ROM ..%02X%02X%02X%02X", addr[4], addr[5], addr[6], addr[7]);
}

void drawFrame(const char* romLine, const char* tempText, const char* statusText) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("DS18B20 Test");

  display.setCursor(0, 10);
  display.print(romLine);

  display.setTextSize(2);
  display.setCursor(0, 24);
  display.print(tempText);

  display.setTextSize(1);
  display.setCursor(0, 50);
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
  Serial.println("DS18B20-Test aktiv");
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
    printRomAddress(romAddress);
    sensors.setResolution(romAddress, SENSOR_RES_BITS);
  }

  char romLine[24];
  if (deviceCount > 0) {
    formatRomShort(romLine, sizeof(romLine), romAddress);
  } else {
    std::snprintf(romLine, sizeof(romLine), "kein ROM");
  }
  drawFrame(romLine, "--", deviceCount > 0 ? "warte..." : "kein Sensor");
}

void loop() {
  const uint32_t now = millis();
  if (now - lastPollMs < POLL_MS) {
    return;
  }
  lastPollMs = now;

  char romLine[24];
  char tempText[12];
  const char* statusText;

  if (deviceCount == 0) {
    std::snprintf(romLine, sizeof(romLine), "kein ROM");
    std::snprintf(tempText, sizeof(tempText), "--");
    statusText = "kein Sensor";
    Serial.println("kein Sensor gefunden");
  } else {
    formatRomShort(romLine, sizeof(romLine), romAddress);

    sensors.requestTemperatures();
    const float tempC = sensors.getTempCByIndex(0);

    if (tempC == DEVICE_DISCONNECTED_C) {
      std::snprintf(tempText, sizeof(tempText), "--");
      statusText = "Fehler -127";
      Serial.println("Fehler: DEVICE_DISCONNECTED (-127) -- Verkabelung/Pull-up pruefen");
    } else if (!sawValidReading && tempC == 85.0f) {
      // Power-On-Default vor der ersten abgeschlossenen Conversion --
      // kein echter Messwert.
      std::snprintf(tempText, sizeof(tempText), "--");
      statusText = "warte...";
      Serial.println("warte auf erste Conversion (85.00 Power-On-Default)");
    } else {
      sawValidReading = true;
      // ASCII 'o' statt '°': das UTF-8-Gradzeichen ist 2 Byte und wird
      // vom Adafruit-GFX-Standardfont als Muell dargestellt.
      std::snprintf(tempText, sizeof(tempText), "%.1foC", static_cast<double>(tempC));
      statusText = "OK";
      Serial.printf("Temperatur: %.1f C (Rohstatus OK)\n", static_cast<double>(tempC));
    }
  }

  drawFrame(romLine, tempText, statusText);
}
