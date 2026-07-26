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
#include "IDisplay.h"

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

    Serial.printf("i2cRecover: SDA vor dem Takten %s\n",
                  digitalRead(sdaPin) == HIGH ? "HIGH" : "LOW");

    uint8_t pulses = 0;
    for (; pulses < 9 && digitalRead(sdaPin) == LOW; ++pulses) {
      digitalWrite(sclPin, LOW);
      delayMicroseconds(5);
      digitalWrite(sclPin, HIGH);
      delayMicroseconds(5);
    }
    Serial.printf("i2cRecover: %u Takt(e), SDA danach %s\n", pulses,
                  digitalRead(sdaPin) == HIGH ? "HIGH" : "LOW");

    // Explizite STOP-Bedingung nur, wenn der Bus tatsaechlich haengt (SDA
    // war LOW). Auf einem gesunden, idle-High-Bus (der Normalfall) diesen
    // Schritt NICHT ausfuehren: Ein manuell erzeugter SDA-Puls waere dort
    // ein unnoetiges, unerwartetes Signal auf den I2C-Pins des SSD1306 --
    // moeglicherweise genau waehrend dessen eigener Power-Up-Sequenz --
    // und koennte den Chip selbst erst in einen haengenden Zustand
    // bringen, statt nur einen bereits haengenden zu befreien. Der
    // Wackeltest-Scanner ruehrt die Pins ueberhaupt nicht an und
    // funktioniert deshalb zuverlaessig.
    if (pulses > 0) {
      // SDA LOW -> SCL HIGH -> SDA HIGH.
      pinMode(sdaPin, OUTPUT);
      digitalWrite(sdaPin, LOW);
      delayMicroseconds(5);
      digitalWrite(sclPin, HIGH);
      delayMicroseconds(5);
      digitalWrite(sdaPin, HIGH);
      delayMicroseconds(5);
    }

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
    //
    // Mehrere Versuche mit steigender Pause: Sowohl bei uns als auch im
    // Wackeltest-Scanner liefert der allererste Scan direkt nach einem
    // Reset (z. B. ueber EN) reproduzierbar TIMEOUT (5); ab dem naechsten
    // Versuch klappt es sofort. Ein Einmalversuch wuerde das faelschlich
    // als "nicht gefunden" werten. Die Pause waechst pro Versuch
    // (OLED_PROBE_RETRY_DELAY_MS * Versuchsnummer), damit auch ein
    // Ausreisser mit laengerer Anlaufzeit sicher abgefangen wird, ohne im
    // Normalfall unnoetig lange zu warten.
    uint8_t i2cResult = 5;
    for (uint8_t attempt = 0; attempt < OLED_PROBE_RETRIES; ++attempt) {
      Wire.beginTransmission(OLED_ADDR);
      i2cResult = Wire.endTransmission();
      Serial.printf("oledInit: Scan 0x%02X -> %u (Versuch %u/%u)\n", OLED_ADDR,
                    i2cResult, attempt + 1, OLED_PROBE_RETRIES);
      if (i2cResult == 0) {
        break;
      }
      delay(OLED_PROBE_RETRY_DELAY_MS * (attempt + 1));
    }
    if (i2cResult != 0) {
      oledOk_ = false;
      return false;
    }

    oledOk_ = display_.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
    if (oledOk_) {
      display_.clearDisplay();
      // Ohne explizite Farbe/Groesse bleibt Text unsichtbar: Adafruit_GFX
      // initialisiert textcolor mit 0xFFFF, das die SSD1306-Bibliothek
      // fuer alles ausser SSD1306_WHITE/SSD1306_INVERSE als Schwarz
      // interpretiert -> Text auf Schwarz-Hintergrund, also unsichtbar,
      // obwohl print()/display() klaglos durchlaufen.
      display_.setTextColor(SSD1306_WHITE);
      display_.setTextSize(1);
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

  void setHeartbeatLed(bool on) override {
    digitalWrite(PIN_LED, on ? HIGH : LOW);
  }

private:
  Adafruit_SSD1306 display_;
  bool oledOk_ = false;
};

// Reale IDisplay-Implementierung fuer das Dashboard (siehe
// lib/Dashboard/). Eigene Adafruit_SSD1306-Instanz, unabhaengig von
// Esp32Hal::display_ (die nur fuer die grobe Boot-Selbsttest-Zeile
// via hal.oledShowLine() gebraucht wird) -- beide teilen sich denselben
// physischen Bus (globales Wire), das ist unkritisch, da nie beide
// gleichzeitig zeichnen. Noch NICHT in setup()/loop() instanziiert:
// dafuer fehlt noch die Datenquelle (Sensor-/Luefter-Regelkreis), die
// erst in einem spaeteren Schritt entsteht.
class Esp32Display : public IDisplay {
public:
  explicit Esp32Display(TwoWire& wire) : display_(OLED_WIDTH, OLED_HEIGHT, &wire, -1) {}

  bool begin() {
    return display_.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  }

  void clear() override {
    display_.clearDisplay();
  }

  void drawText(uint8_t x, uint8_t y, const char* s, uint8_t size, bool inverted) override {
    display_.setTextSize(size);
    display_.setTextColor(inverted ? SSD1306_BLACK : SSD1306_WHITE);
    display_.setCursor(x, y);
    display_.print(s);
  }

  void fillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h) override {
    display_.fillRect(x, y, w, h, SSD1306_WHITE);
  }

  void hLine(uint8_t y) override {
    display_.drawFastHLine(0, y, OLED_WIDTH, SSD1306_WHITE);
  }

  void vLine(uint8_t x) override {
    display_.drawFastVLine(x, 0, OLED_HEIGHT, SSD1306_WHITE);
  }

  void present() override {
    display_.display();
  }

private:
  Adafruit_SSD1306 display_;
};

Esp32Hal hal;

void setup() {
  Serial.begin(115200);
  pinMode(PIN_SPEAKER, OUTPUT);

  runBootSelfTest(hal);

  // GPIO2 ist ein Strapping-Pin -- Ausgang erst NACH dem Selbsttest
  // konfigurieren (siehe CLAUDE.md "Dashboard-Anzeige & Meldekanaele").
  pinMode(PIN_LED, OUTPUT);
}

void loop() {
}
