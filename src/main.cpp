// =============================================================
//  main.cpp — Reale HAL-Implementierung. Nur env:esp32, nicht
//  Teil des nativen Test-Builds (test_build_src = no, Default).
// =============================================================
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>
#include "config.h"
#include "Hal.h"
#include "BootSelfTest.h"
#include "IDisplay.h"
#include "ILed.h"
#include "IHistoryStore.h"
#include "Dashboard.h"
#include "FanControl.h"
#include "SensorCal.h"
#include "SelfDiag.h"

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

  void frameRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h) override {
    display_.drawRect(x, y, w, h, SSD1306_WHITE);
  }

  void clearRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h) override {
    display_.fillRect(x, y, w, h, SSD1306_BLACK);
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

// Reale ILed-Implementierung fuer das LED-Muster-Modul (siehe
// lib/Led/). Eigenstaendig neben Esp32Hal::setHeartbeatLed() -- genau
// wie Esp32Display neben Hal::oledShowLine() steht: beide steuern
// dasselbe physische Ausgabemittel an, aber ueber die fokussierte
// Abstraktion, die das jeweilige Modul (hier LedPattern::ledLevel())
// tatsaechlich braucht. Noch NICHT in setup()/loop() instanziiert:
// dafuer fehlt der reale Fault-Latch-Zustand, den erst der spaetere
// Health-Monitor liefert (siehe CLAUDE.md "LED").
class Esp32Led : public ILed {
public:
  // GPIO2 ist ein Strapping-Pin -- ausschliesslich NACH dem
  // Boot-Selbsttest aufrufen (siehe CLAUDE.md "Fallen").
  void begin() {
    pinMode(PIN_LED, OUTPUT);
  }

  void set(bool on) override {
    digitalWrite(PIN_LED, on ? HIGH : LOW);
  }
};

// Reale IHistoryStore-Implementierung ueber NVS (Preferences), siehe
// lib/History/ und CLAUDE.md "Verschleiss-Historie". NVS liegt in
// einer EIGENEN Flash-Partition, die ein normaler "pio run -t upload"
// NICHT ueberschreibt -- die Historie ueberlebt das Flashen der
// Firmware, nur ein voller Chip-Erase (esptool erase_flash) loescht
// sie. Noch NICHT in setup()/loop() instanziiert: dafuer fehlt die
// Sensor-Datenquelle, die erst der spaetere Regelkreis liefert.
class Esp32HistoryStore : public IHistoryStore {
public:
  bool load(HistoryData& data) override {
    Preferences prefs;
    if (!prefs.begin("history", /*readOnly=*/true)) {
      return false;
    }
    const size_t got = prefs.getBytes("data", &data, sizeof(HistoryData));
    prefs.end();
    return got == sizeof(HistoryData);
  }

  bool save(const HistoryData& data) override {
    Preferences prefs;
    if (!prefs.begin("history", /*readOnly=*/false)) {
      return false;
    }
    const size_t written = prefs.putBytes("data", &data, sizeof(HistoryData));
    prefs.end();
    return written == sizeof(HistoryData);
  }
};

Esp32Hal hal;

// =============================================================
//  Regelkreis Kanal #1 (VSA1.1 / Fan1, siehe CLAUDE.md "Regelmodell").
//  Erste reale Verdrahtung von Sensor + Luefter + Dashboard in die
//  Hauptfirmware. DEBUG_SINGLE_CHANNEL (config.h) haelt die Kanaele
//  #2-#4 und Ambient bewusst auf Idle-Platzhaltern, weil deren
//  Hardware noch nicht existiert -- kein Health-Monitor faellt fuer
//  fehlende Sensoren/Luefter um.
//
//  Bewusst NICHT Teil dieses Schritts (bleiben unveraendert nur als
//  getestete, unverdrahtete Module bestehen): Ack-Taster, LED-
//  Fehlermuster, Verschleiss-Historie, Logging, Watchdog. Siehe
//  CLAUDE.md fuer den jeweiligen Umsetzungsstand.
// =============================================================
namespace {

OneWire oneWire(PIN_ONEWIRE);
DallasTemperature sensors(&oneWire);

Esp32Display dashDisplay(Wire);
Dashboard dashboard(dashDisplay);

#if !defined(ESP_ARDUINO_VERSION_MAJOR) || ESP_ARDUINO_VERSION_MAJOR < 3
constexpr uint8_t PWM_CHANNEL_FAN1 = 0;  // nur von der aelteren LEDC-API gebraucht
#endif

// LEDC-PWM-Abstraktion, unabhaengig von der ESP32-Arduino-Core-Version
// (siehe CLAUDE.md "Fallen" zu ledcAttach vs. ledcSetup/ledcAttachPin;
// hier fuer den Luefter tatsaechlich per LEDC statt Bit-Banging
// umgesetzt, weil 25 kHz sauberes PWM braucht, keine Tonerzeugung).
void fan1PwmBegin() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(FAN[ROLE_VSA1_1].pwmPin, FAN_PWM_FREQ_HZ, FAN_PWM_RESOLUTION);
#else
  ledcSetup(PWM_CHANNEL_FAN1, FAN_PWM_FREQ_HZ, FAN_PWM_RESOLUTION);
  ledcAttachPin(FAN[ROLE_VSA1_1].pwmPin, PWM_CHANNEL_FAN1);
#endif
}

void fan1PwmWrite(uint32_t duty) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(FAN[ROLE_VSA1_1].pwmPin, duty);
#else
  ledcWrite(PWM_CHANNEL_FAN1, duty);
#endif
}

uint32_t percentToDuty(uint8_t pct) {
  const uint32_t maxDuty = (1UL << FAN_PWM_RESOLUTION) - 1;
  return (static_cast<uint32_t>(pct) * maxDuty) / 100;
}

volatile uint32_t fan1TachoPulses = 0;

void IRAM_ATTR onFan1TachoFalling() {
  ++fan1TachoPulses;
}

// --- Sensor-Poll, nicht-blockierend (Architektur-Regel 4: kein
// delay() im Regelkreis). DallasTemperature blockiert per Default
// waehrend der Conversion -- stattdessen zweiphasig: anstossen, nach
// SENSOR_CONV_MS (millis()-getaktet) lesen. ---
enum class PollPhase { Idle, Converting };
PollPhase pollPhase = PollPhase::Idle;
uint32_t pollPhaseStartMs = 0;

enum class SensorState { Warten, Fehler, Ok };
SensorState sensor1State = SensorState::Warten;
bool sensor1SawValidReading = false;
float sensor1RawC = 0.0f;
float sensor1CalC = 0.0f;

uint8_t fan1TargetDutyPct = 0;
bool fan1WasOn = false;
bool fan1KickstartActive = false;
uint32_t fan1KickstartEndMs = 0;

uint16_t fan1Rpm = 0;
uint8_t fan1FailCycles = 0;

uint32_t loopCount = 0;
uint16_t loopHz = 0;

// Wertet einen abgeschlossenen Sensor-Read aus, aktualisiert die
// Kennlinie (Kickstart lebt hier im Controller, siehe
// CLAUDE.md Architektur-Regel 3) und loggt den Zyklus.
void applySensorReading(float rawC, uint32_t now) {
  bool regulate = true;
  if (rawC < TEMP_MIN_VALID || rawC > TEMP_MAX_VALID) {
    // Deckt auch DEVICE_DISCONNECTED_C (-127) ab, ohne eine
    // library-spezifische Konstante ausserhalb von config.h
    // einzufuehren (Architektur-Regel 5: Schwellen nur in config.h).
    sensor1State = SensorState::Fehler;
  } else if (!sensor1SawValidReading && rawC == TEMP_POR_VALUE) {
    sensor1State = SensorState::Warten;
    regulate = false;  // noch keine abgeschlossene Conversion -- nicht regeln
  } else {
    sensor1State = SensorState::Ok;
    sensor1SawValidReading = true;
    sensor1RawC = rawC;
  }

  if (regulate) {
    // Kein Ambient-Sensor im Debug-Modus vorhanden. SENSOR_K ist fuer
    // alle Rollen ohnehin 0 (noch keine Waermebildkamera-Kalibrierung),
    // deshalb rawSonde auch als ambC hereinreichen -- Delta wird 0,
    // dieTempC() liefert exakt rawSonde zurueck (siehe CLAUDE.md
    // "Sensor-Kalibrierung").
    sensor1CalC = dieTempC(sensor1RawC, sensor1RawC, SENSOR_K[ROLE_VSA1_1]);

    const bool sensorOk = sensor1State == SensorState::Ok;
    const uint8_t rampDuty = curveDuty(sensor1CalC, fan1WasOn);
    const uint8_t newDuty = channelFailSafeDuty(rampDuty, sensorOk);
    const bool nowOn = newDuty > 0;
    if (nowOn && !fan1WasOn) {
      fan1KickstartActive = true;
      fan1KickstartEndMs = now + KICKSTART_MS;
    }
    fan1TargetDutyPct = newDuty;
    fan1WasOn = nowOn;
  }

  switch (sensor1State) {
    case SensorState::Fehler:
      Serial.println("Kanal #1: Sensor-Fehler (ausserhalb Plausibilitaet/getrennt)");
      break;
    case SensorState::Warten:
      Serial.println("Kanal #1: warte auf erste Conversion (85.00 Power-On-Default)");
      break;
    case SensorState::Ok:
      Serial.printf("Kanal #1: T=%.1f C Duty=%u%% RPM=%u\n", static_cast<double>(sensor1CalC),
                    fan1TargetDutyPct, fan1Rpm);
      break;
  }
}

void tickSensorPoll(uint32_t now) {
  if (pollPhase == PollPhase::Idle) {
    if (now - pollPhaseStartMs >= SENSOR_POLL_MS) {
      sensors.requestTemperatures();  // non-blocking, siehe setWaitForConversion(false)
      pollPhaseStartMs = now;
      pollPhase = PollPhase::Converting;
    }
  } else {
    if (now - pollPhaseStartMs >= SENSOR_CONV_MS) {
      const float raw = sensors.getTempC(SENSOR_ROM[ROLE_VSA1_1]);
      pollPhaseStartMs = now;
      pollPhase = PollPhase::Idle;
      applySensorReading(raw, now);
    }
  }
}

void applyFan1Pwm(uint32_t now) {
  uint8_t dutyToApply = fan1TargetDutyPct;
  if (fan1KickstartActive) {
    if (now < fan1KickstartEndMs) {
      // Kickstart setzt sich gegen eine niedrigere Kennlinien-Duty
      // durch, gewinnt aber nie gegen eine bereits hoehere (z. B.
      // Fail-Safe-100%).
      dutyToApply = KICKSTART_DUTY > fan1TargetDutyPct ? KICKSTART_DUTY : fan1TargetDutyPct;
    } else {
      fan1KickstartActive = false;
      dutyToApply = fan1TargetDutyPct;
    }
  }
  fan1PwmWrite(percentToDuty(dutyToApply));
}

uint32_t tachoWindowStartMs = 0;

void tickTachoWindow(uint32_t now) {
  if (now - tachoWindowStartMs < TACHO_WIN_MS) {
    return;
  }
  tachoWindowStartMs = now;

  noInterrupts();
  const uint32_t pulses = fan1TachoPulses;
  fan1TachoPulses = 0;
  interrupts();
  fan1Rpm = static_cast<uint16_t>((pulses * 60000UL) / (TACHO_PULSES_PER_REV * TACHO_WIN_MS));

  // Health-Monitor (Fan-Teil, siehe CLAUDE.md): Tacho gegen
  // kommandierte Duty pruefen. duty==0 && rpm==0 ist korrekt
  // (Semi-Passiv), kein Alarm. FAIL_PERSIST entprellt kurze Ausreisser
  // (z. B. beim Kickstart-Uebergang).
  if (fan1TargetDutyPct > 0 && fan1Rpm < FAN_MIN_RPM) {
    if (fan1FailCycles < 255) {
      ++fan1FailCycles;
    }
  } else {
    fan1FailCycles = 0;
  }
}

ChStatus computeChannel1Status() {
  if (sensor1State == SensorState::Fehler || fan1FailCycles >= FAIL_PERSIST) {
    return ChStatus::Err;
  }
  if (sensor1State == SensorState::Warten) {
    return ChStatus::Idle;  // noch kein Fehler, nur noch keine Regelung
  }
  if (sensor1CalC >= WARN_C) {
    return ChStatus::Warn;
  }
  if (fan1TargetDutyPct == 0) {
    return ChStatus::Idle;
  }
  return ChStatus::Ok;
}

uint32_t renderStartMs = 0;

void tickRender(uint32_t now) {
  if (now - renderStartMs < SCROLL_MS) {
    return;
  }
  renderStartMs = now;

  FinalDashboardData data{};
  data.channels[0] = { sensor1CalC, static_cast<int16_t>(fan1Rpm), computeChannel1Status(), false };

  // Kanaele #2-#4 und Ambient: im Debug-Modus bewusst nicht bestueckt
  // (siehe config.h DEBUG_SINGLE_CHANNEL) -- Idle-Platzhalter, rpm=-1
  // ("-") markiert klar "kein Kanal", nicht "Luefter steht".
  const ChannelStatus idlePlaceholder{ 0.0f, -1, ChStatus::Idle, false };
  data.channels[1] = idlePlaceholder;
  data.channels[2] = idlePlaceholder;
  data.channels[3] = idlePlaceholder;
  data.ambient = idlePlaceholder;

  char selfLine[22];
  formatSelfDiagLine(selfLine, sizeof(selfLine), ESP.getFreeHeap(), loopHz);
  data.scrollText = selfLine;
  data.scrollTextLen = static_cast<uint8_t>(strlen(selfLine));

  const bool phaseOn = (now / BLINK_MS) % 2 != 0;
  const int32_t scrollOffsetPx = static_cast<int32_t>((now / SCROLL_MS) * SCROLL_STEP_PX);

  dashboard.renderFinal(data, phaseOn, scrollOffsetPx, DEBUG_SINGLE_CHANNEL);
}

uint32_t loopHzWindowStartMs = 0;

void tickLoopHz(uint32_t now) {
  ++loopCount;
  if (now - loopHzWindowStartMs < 1000) {
    return;
  }
  loopHz = static_cast<uint16_t>((loopCount * 1000UL) / (now - loopHzWindowStartMs));
  loopCount = 0;
  loopHzWindowStartMs = now;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  pinMode(PIN_SPEAKER, OUTPUT);

  runBootSelfTest(hal);

  // GPIO2 ist ein Strapping-Pin -- Ausgang erst NACH dem Selbsttest
  // konfigurieren (siehe CLAUDE.md "Dashboard-Anzeige & Meldekanaele").
  pinMode(PIN_LED, OUTPUT);

  Serial.println(DEBUG_SINGLE_CHANNEL
                     ? "Debug-Modus: nur Kanal #1 (VSA1.1/Fan1) aktiv, "
                       "#2-#4 + Ambient als Idle-Platzhalter"
                     : "Vollbetrieb (4 Kanaele + Ambient) ist noch nicht verdrahtet");

  sensors.begin();
  sensors.setWaitForConversion(false);  // nicht-blockierend, siehe tickSensorPoll()
  sensors.setResolution(SENSOR_ROM[ROLE_VSA1_1], SENSOR_RES_BITS);

  fan1PwmBegin();
  fan1PwmWrite(0);

  pinMode(FAN[ROLE_VSA1_1].tachoPin, INPUT);  // externer 10k Pull-up vorausgesetzt
  attachInterrupt(digitalPinToInterrupt(FAN[ROLE_VSA1_1].tachoPin), onFan1TachoFalling, FALLING);

  if (!dashDisplay.begin()) {
    Serial.println("Dashboard-OLED-Init fehlgeschlagen -- renderFinal() zeichnet ins Leere.");
  }
}

void loop() {
  const uint32_t now = millis();

  tickSensorPoll(now);
  applyFan1Pwm(now);
  tickTachoWindow(now);
  tickRender(now);
  tickLoopHz(now);
}
