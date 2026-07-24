// =============================================================
//  config.h — ESP32 Thermal-Monitor fuer 3dfx Voodoo 5 5500
//  Einziger Ort fuer alles Hardware-Nahe und alle Schwellen.
//
//  WICHTIG: Diese Datei enthaelt bewusst KEIN #include <Arduino.h>
//  und keine Bibliothekstypen (kein DeviceAddress). Nur <cstdint>.
//  Grund: Die Datei wird auch vom nativen Unity-Test-Build
//  eingebunden, der ohne Arduino-Framework kompiliert.
// =============================================================
#pragma once
#include <cstdint>

// -------------------------------------------------------------
//  1. PINBELEGUNG
//     Keine Strapping-Pins (0,2,12,15) belegt.
//     GPIO 34-39 nicht nutzbar (reine Eingaenge, kein Pull-up).
// -------------------------------------------------------------
constexpr uint8_t PIN_ONEWIRE = 4;    // DS18B20-Bus, 4,7k Pull-up -> 3V3
constexpr uint8_t PIN_SPEAKER = 25;   // 1k -> Basis BC547
constexpr uint8_t PIN_ACK     = 27;   // Taster gegen GND, INPUT_PULLUP
constexpr uint8_t PIN_I2C_SDA = 21;   // OLED, ESP32-Default
constexpr uint8_t PIN_I2C_SCL = 22;   // OLED, ESP32-Default

struct FanCfg { uint8_t pwmPin; uint8_t tachoPin; };

// Reine Beschriftung. Welcher Rotor physisch "1" ist, zeigt die
// Anlaufsequenz im Selbsttest. Steckertausch = Aenderung hier.
constexpr FanCfg FAN[2] = {
  { 18, 32 },   // Luefter 1 / Zone A
  { 19, 33 },   // Luefter 2 / Zone B
};

// -------------------------------------------------------------
//  2. OLED
// -------------------------------------------------------------
constexpr uint8_t  OLED_ADDR     = 0x3C;   // manche Module: 0x3D
constexpr uint8_t  OLED_WIDTH    = 128;
constexpr uint8_t  OLED_HEIGHT   = 64;
constexpr uint32_t SPLASH_MS     = 15000;  // Selbsttest-Bild, dann Dashboard

// -------------------------------------------------------------
//  3. TEMPERATURSENSOREN (DS18B20)
//     Aufloesung ist Fuehrungsgroesse, Wandlungszeit abgeleitet.
// -------------------------------------------------------------
constexpr uint8_t SENSOR_RES_BITS = 11;    // 11 Bit = 0,125 C

// Datenblatt: 9 Bit 94 ms | 10 Bit 188 ms | 11 Bit 375 ms | 12 Bit 750 ms
constexpr uint16_t ds18b20ConvMs(uint8_t bits) { return 750 >> (12 - bits); }
constexpr uint16_t SENSOR_CONV_MS = ds18b20ConvMs(SENSOR_RES_BITS);

constexpr uint16_t SENSOR_POLL_MS = 1500;  // Regeltakt

static_assert(SENSOR_POLL_MS >= SENSOR_CONV_MS,
  "SENSOR_POLL_MS kuerzer als Wandlungszeit -> Sensor liefert 85C-Default");
static_assert(SENSOR_RES_BITS >= 9 && SENSOR_RES_BITS <= 12,
  "SENSOR_RES_BITS muss zwischen 9 und 12 liegen");

// Rollen: 0 = VSA1 (Spalte #1), 1 = VSA2 (Spalte #2), 2 = Ambient
constexpr uint8_t ROLE_VSA1 = 0;
constexpr uint8_t ROLE_VSA2 = 1;
constexpr uint8_t ROLE_AMB  = 2;

// ROM-Adressen NACH der Discovery hier eintragen.
// Solange Nullen drinstehen, schlaegt die CRC-Pruefung fehl und die
// Firmware startet automatisch im Discovery-Modus (kein Regelbetrieb).
constexpr uint8_t SENSOR_ROM[3][8] = {
  { 0x28, 0, 0, 0, 0, 0, 0, 0 },   // VSA1
  { 0x28, 0, 0, 0, 0, 0, 0, 0 },   // VSA2
  { 0x28, 0, 0, 0, 0, 0, 0, 0 },   // AMB
};

constexpr bool FORCE_DISCOVERY = false;  // true = Discovery erzwingen

// Plausibilitaet
constexpr float TEMP_MIN_VALID = -55.0f;
constexpr float TEMP_MAX_VALID = 125.0f;
constexpr float TEMP_POR_VALUE =  85.0f;  // Power-On-Default, nur vor Conversion gueltig

// -------------------------------------------------------------
//  4. LUEFTERKENNLINIE (eine Kurve fuer beide Zonen)
//     Kurvenform steht im Code, jeder Stuetzpunkt hier.
// -------------------------------------------------------------
struct FanCurve {
  float   onC;       // Idle -> Regelung
  float   offC;      // Regelung -> Idle (Hysterese-Totband)
  float   fullC;     // ab hier maxDuty
  uint8_t minDuty;   // PWM % beim Einschalten
  uint8_t maxDuty;   // PWM % Obergrenze
};

constexpr FanCurve CURVE = {
  45.0f,   // onC
  40.0f,   // offC  -> 5 K Totband gegen Pumpen
  70.0f,   // fullC
  20,      // minDuty: Noctua-Spec-Untergrenze (@20% PWM)
  100,     // maxDuty
};

static_assert(CURVE.offC < CURVE.onC,  "offC muss unter onC liegen (Hysterese)");
static_assert(CURVE.onC  < CURVE.fullC,"fullC muss ueber onC liegen");

// Kickstart: Uebergang aus -> ein. Gehoert NICHT in die Kennlinie,
// weil es ein zeitlicher Vorgang ist. Empirisch trimmbar.
constexpr uint8_t  KICKSTART_DUTY = 30;    // >= CURVE.minDuty
constexpr uint16_t KICKSTART_MS   = 300;

static_assert(KICKSTART_DUTY >= CURVE.minDuty,
  "KICKSTART_DUTY muss mindestens minDuty sein");

// PWM / Tacho
constexpr uint32_t FAN_PWM_FREQ_HZ      = 25000;  // ausserhalb Hoerbereich
constexpr uint8_t  FAN_PWM_RESOLUTION   = 8;      // Bit
constexpr uint8_t  TACHO_PULSES_PER_REV = 2;
constexpr uint16_t TACHO_WIN_MS         = 500;    // Messfenster
constexpr uint16_t FAN_MIN_RPM          = 300;    // darunter = Fehler trotz Duty
constexpr uint16_t FAN_STARTUP_MS       = 800;    // Anlaufzeit im Selbsttest

// RPM = Flanken * 60000 / (PULSES_PER_REV * Fenster_ms)

// -------------------------------------------------------------
//  5. STATUS-SCHWELLEN
// -------------------------------------------------------------
constexpr float WARN_C = 70.0f;   // Ok -> Warn (identisch mit CURVE.fullC)

// -------------------------------------------------------------
//  6. AKUSTIK
//     Alarm 1500 Hz, Bestaetigungen bewusst andere Frequenzen.
// -------------------------------------------------------------
constexpr uint16_t BEEP_START_FREQ = 2000;
constexpr uint16_t BEEP_START_MS   = 120;
constexpr uint16_t BEEP_ERR_FREQ   = 1500;
constexpr uint16_t BEEP_ERR_MS     = 150;
constexpr uint16_t BEEP_GAP_MS     = 80;     // Pause zwischen Fehlerpieps

// Fehlercodes (Anzahl Pieps im Selbsttest)
constexpr uint8_t BEEPS_OLED   = 2;
constexpr uint8_t BEEPS_SENSOR = 3;
constexpr uint8_t BEEPS_FAN    = 4;

// Wiederholender Runtime-Alarm
constexpr uint32_t ALARM_REPEAT_MS = 2500;

// -------------------------------------------------------------
//  7. ACK-TASTER
//     Ton markiert den Ausloeser der Aktion:
//       < AKTION1_TIME            -> kein Ton, keine Aktion
//       AKTION1_TIME..AKTION2_TIME-> Ton 1 bei 500 ms,
//                                    Aktion beim Loslassen (Quittieren)
//       >= AKTION2_TIME           -> Ton 2 bei 1200 ms,
//                                    Aktion sofort (Latch-Reset)
// -------------------------------------------------------------
constexpr uint16_t ACK_DEBOUNCE_MS = 30;
constexpr uint16_t AKTION1_TIME    = 500;    // Quittieren scharf
constexpr uint16_t AKTION2_TIME    = 1200;   // Latch-Reset scharf

constexpr uint16_t AKTION1_BEEP_FREQ = 2600; // hell + kurz
constexpr uint16_t AKTION1_BEEP_MS   = 40;
constexpr uint16_t AKTION2_BEEP_FREQ = 1800; // tiefer + laenger
constexpr uint16_t AKTION2_BEEP_MS   = 120;

static_assert(AKTION2_TIME > AKTION1_TIME,
  "Aktion2 muss spaeter liegen als Aktion1");
static_assert(AKTION1_TIME > ACK_DEBOUNCE_MS,
  "Aktion1 muss nach der Entprellung liegen");

// -------------------------------------------------------------
//  8. HEALTH-MONITOR / WATCHDOG
// -------------------------------------------------------------
constexpr uint8_t FAIL_PERSIST = 3;   // Zyklen in Folge bis "echter" Fehler
constexpr uint8_t WDT_TIMEOUT_S = 8;  // erst NACH dem Selbsttest scharf schalten
