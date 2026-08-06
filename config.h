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
// 4 unabhaengige Kanaele (siehe CLAUDE.md "Regelmodell"): VSA1/VSA2
// haben je einen Luefter Rueckseite und einen Vorderseite. Alle vier
// teilen sich einen gemeinsamen Molex-Stromanschluss, nur PWM/Tacho
// sind pro Luefter separat.
constexpr FanCfg FAN[4] = {
  { 18, 32 },   // Luefter 1: VSA1 Rueckseite / Zone A
  { 19, 33 },   // Luefter 2: VSA2 Rueckseite / Zone B
  { 23, 34 },   // Luefter 3: VSA1 Vorderseite
  { 26, 35 },   // Luefter 4: VSA2 Vorderseite
};

// -------------------------------------------------------------
//  2. OLED
// -------------------------------------------------------------
constexpr uint8_t  OLED_ADDR     = 0x3C;   // manche Module: 0x3D
constexpr uint8_t  OLED_WIDTH    = 128;
constexpr uint8_t  OLED_HEIGHT   = 64;
constexpr uint32_t I2C_CLOCK_HZ  = 50000;  // langsamer als Standard (100 kHz):
                                            // robuster bei langen Steckbrett-Leitungen
constexpr uint8_t  OLED_PROBE_RETRIES          = 5;   // Versuche, bis "nicht gefunden"
constexpr uint16_t OLED_PROBE_RETRY_DELAY_MS   = 20;  // Basis-Pause, steigt pro Versuch
                                                       // (20,40,60,80,100 ms -> 300 ms total,
                                                       // deckt den beobachteten Einmal-TIMEOUT
                                                       // direkt nach einem Reset sicher ab)
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

// Rollen: 5 Sonden, 1:1 einem Regelkreis zugeordnet (siehe CLAUDE.md
// "Regelmodell") -- ausser AMB, die speist nur die Panik-Uebersteuerung.
constexpr uint8_t ROLE_VSA1_1 = 0;  // VSA1 Rueckseite -> Luefter 1
constexpr uint8_t ROLE_VSA1_2 = 1;  // VSA1 Vorderseite -> Luefter 3
constexpr uint8_t ROLE_VSA2_1 = 2;  // VSA2 Rueckseite -> Luefter 2
constexpr uint8_t ROLE_VSA2_2 = 3;  // VSA2 Vorderseite -> Luefter 4
constexpr uint8_t ROLE_AMB    = 4;  // kein eigener Regelkreis

// ROM-Adressen NACH der Discovery hier eintragen.
// Solange Nullen drinstehen, schlaegt die CRC-Pruefung fehl und die
// Firmware startet automatisch im Discovery-Modus (kein Regelbetrieb).
// VSA1.1 ist per tools/sensor_test/ auf dem Steckbrett ermittelt und
// per CRC8 verifiziert (siehe DEBUG_SINGLE_CHANNEL unten) -- die
// uebrigen vier warten noch auf Hardware/Discovery.
constexpr uint8_t SENSOR_ROM[5][8] = {
  { 0x28, 0x16, 0x4D, 0x53, 0x0F, 0x00, 0x00, 0xFD },   // VSA1.1 (Rueckseite)
  { 0x28, 0, 0, 0, 0, 0, 0, 0 },   // VSA1.2 (Vorderseite)
  { 0x28, 0, 0, 0, 0, 0, 0, 0 },   // VSA2.1 (Rueckseite)
  { 0x28, 0, 0, 0, 0, 0, 0, 0 },   // VSA2.2 (Vorderseite)
  { 0x28, 0, 0, 0, 0, 0, 0, 0 },   // AMB
};

constexpr bool FORCE_DISCOVERY = false;  // true = Discovery erzwingen

// Bankaufbau-Modus: nur Kanal VSA1.1/Fan1 ist physisch vorhanden.
// Kanaele VSA1.2/VSA2.1/VSA2.2 und AMB gelten als "nicht bestueckt" --
// sie werden weder gelesen noch auf Fehler geprueft (Status bleibt
// `Idle`), damit nach dem Boot kein Fehler fuer fehlende Hardware
// stehen bleibt. Auf dem Dashboard zusaetzlich an einem grossen,
// im Blink-Takt invertierenden "D" in Zelle 0;0 erkennbar (siehe
// Dashboard::renderFinal()). false = Vollbetrieb, alle 4 Kanaele +
// Ambient erwartet (noch nicht implementiert, siehe CLAUDE.md).
constexpr bool DEBUG_SINGLE_CHANNEL = true;

// Plausibilitaet
constexpr float TEMP_MIN_VALID = -55.0f;
constexpr float TEMP_MAX_VALID = 125.0f;
constexpr float TEMP_POR_VALUE =  85.0f;  // Power-On-Default, nur vor Conversion gueltig

// Lastabhaengige Die-Korrektur (siehe CLAUDE.md "Sensor-Kalibrierung"):
// ein KONSTANTER Offset waere physikalisch falsch -- die Differenz
// Die-zu-Sonde ist bei Nulllast selbst null und waechst mit der
// Verlustleistung (ΔT = P * R_thermisch). Da P nicht gemessen wird,
// dient die Kuehler-Uebertemperatur ueber Ambient (rawSondeC - ambC)
// als monoton steigender Stellvertreter fuer die Last:
// T_die ≈ T_sonde + k * (T_sonde - T_amb), k=0 -> keine Korrektur.
// Reine Regel-/Anzeige-Groesse wie zuvor -- die ROHTEMPERATUR geht
// immer unveraendert ins Log (siehe lib/SensorCal/SensorCal.h).
// Index 4 (AMB) ist der Ambient-Sensor selbst -- dessen k bleibt per
// Definition 0 (Referenz, wird nicht korrigiert).
constexpr float SENSOR_K[5] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

// -------------------------------------------------------------
//  4. LUEFTERKENNLINIE (dieselbe Kurve, 4x unabhaengig angewandt --
//     siehe CLAUDE.md "Regelmodell", kein Zonen-Mischen)
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

// -------------------------------------------------------------
//  9. LOGGING (siehe CLAUDE.md, Abschnitt "Logging")
//     Nur Ebene 1 (Serial) und Ebene 2 (LittleFS-Ringpuffer).
//     WLAN-/HTTP-Konstanten fuer Ebene 3 kommen erst, wenn dieser
//     Baustein tatsaechlich gebaut wird.
// -------------------------------------------------------------
constexpr bool     LOG_ENABLED           = true;   // Logging global ein-/ausschalten
constexpr uint32_t LOG_FLUSH_MS          = 5000;   // Flush-Intervall LittleFS-Ringpuffer
constexpr uint32_t LOG_RINGBUFFER_BYTES  = 65536;  // max. Groesse der Logdatei

// -------------------------------------------------------------
//  10. DASHBOARD-LAYOUT (aus OLED Dashboard Test)
//      4 Spalten (Label, VSA1, VSA2, Amb) x 4 Zeilen (Kopf, Temp,
//      rpm, Status), feste Zellbreiten. Rechts der Amb-Spalte
//      (ab x ≈ 105) bleiben ~23 px frei -- bewusst reserviert fuer
//      eine moegliche 4. Datenspalte spaeter.
// -------------------------------------------------------------
constexpr uint8_t COL_X[4]  = { 0, 27, 54, 81 };   // Label, VSA1, VSA2, Amb
constexpr uint8_t CELL_W[4] = { 24, 24, 24, 24 };  // feste Zellbreiten (px)
constexpr uint8_t ROW_Y[4]  = { 0, 13, 25, 37 };   // Kopf, Temp, rpm, Status
constexpr uint8_t TXT_SIZE_HEAD = 1;
constexpr uint8_t TXT_SIZE_VAL  = 1;
constexpr bool    HEADER_UNDERLINE = true;
constexpr bool    VSEP = false;
constexpr bool    HSEP = true;
// Regeln: WARN-Zelle invertiert (statisch), ERR-Zelle blinkt (Takt = BLINK_MS)
constexpr uint16_t BLINK_MS = 500;
constexpr char HEARTBEAT_A = '|';
constexpr char HEARTBEAT_B = '-';
// SelfDiag: freie Textzeile ueber volle Breite (Zeile 5), ausserhalb des Rasters
constexpr bool    SELFDIAG_ROW = true;
constexpr uint8_t SELFDIAG_Y   = 56;

// -------------------------------------------------------------
//  11. AMBIENT-SCHWELLEN
//      Eigene Bedeutung: Gehaeuse-Innenraum, keine VSA-Kurvenschwelle.
// -------------------------------------------------------------
constexpr float AMB_WARN_C     = 48.0f;  // darueber: Gehaeuse zu heiss -> Panik
constexpr float AMB_WARN_OFF_C = 43.0f;  // 5K Hysterese gegen Pumpen der Panik-Stufe

static_assert(AMB_WARN_OFF_C < AMB_WARN_C,
  "AMB_WARN_OFF_C muss unter AMB_WARN_C liegen (Hysterese)");

// -------------------------------------------------------------
//  12. LED (Onboard, GPIO2)
// -------------------------------------------------------------
constexpr uint8_t PIN_LED = 2;  // Strapping-Pin, nach Boot als Ausgang unkritisch

// Fault-Blinkmuster (Doppelblink): 200ms an / 200ms aus / 200ms an /
// 600ms aus -- deutlich schneller/anders als der ruhige Heartbeat,
// synchron zum akustischen Alarm erkennbar (siehe lib/Led/LedPattern).
constexpr uint16_t LED_FAULT_ON_MS    = 200;
constexpr uint16_t LED_FAULT_GAP_MS   = 200;
constexpr uint16_t LED_FAULT_PAUSE_MS = 600;

// -------------------------------------------------------------
//  13. VERSCHLEISS-HISTORIE (persistente Max-Uebertemperatur,
//      siehe CLAUDE.md "Verschleiss-Historie", lib/History/)
// -------------------------------------------------------------
// Mindest-Zuwachs, den ein neuer Wert gegenueber dem gespeicherten
// Maximum haben muss, um als echter Rekord zu zaehlen -- filtert
// Sensorrauschen (DS18B20 ±0,06 K bei 12 Bit) und Luftturbulenz.
constexpr float HISTORY_EPSILON_C = 0.5f;

// Gepufferter NVS-Commit-Takt: Flash/NVS haben begrenzte
// Schreibzyklen (~100k) -- ein neuer Rekord wird sofort im RAM
// uebernommen, aber erst nach diesem Intervall tatsaechlich
// persistiert, nicht bei jedem einzelnen Rausch-Peak.
constexpr uint32_t HISTORY_COMMIT_MS = 300000;  // 5 Minuten

// -------------------------------------------------------------
//  14. FINALES DASHBOARD-LAYOUT (4 Kanalspalten + Ambient-Segment/
//      Laufband in Zeile 5, siehe CLAUDE.md "Dashboard-Anzeige",
//      aus OLED Dashboard Test auf echter Hardware abgestimmt).
//      Eigene Namen (Praefix CH_), weil das bestehende COL_X/CELL_W/
//      ROW_Y/HEADER_UNDERLINE/VSEP/SELFDIAG_Y (oben, Block 10) fuer
//      das aktuelle 3-Spalten-Layout von lib/Dashboard/ unveraendert
//      bleiben muessen (siehe test_dashboard). Werte, die sich
//      zwischen beiden Layouts nicht unterscheiden (TXT_SIZE_*, HSEP,
//      BLINK_MS, HEARTBEAT_A/B, SELFDIAG_ROW), werden weiterhin
//      gemeinsam genutzt, nicht dupliziert.
// -------------------------------------------------------------
constexpr uint8_t CH_COL_X[5]  = { 0, 21, 48, 75, 102 };  // Label,#1,#2,#3,#4
constexpr uint8_t CH_CELL_W[5] = { 18, 24, 24, 24, 24 };
constexpr uint8_t CH_ROW_Y[4]  = { 0, 10, 22, 34 };       // Kopf,Temp,rpm,Status
constexpr bool    CH_HEADER_UNDERLINE = false;
constexpr bool    CH_VSEP = true;
constexpr uint8_t CH_SELFDIAG_Y = 43;  // 10px hoeher (Diagnose: naeher an Sta-Zeile/vLine-Ende)

// Laufband (Zeile 5, rechts vom Ambient-Segment), siehe
// lib/Dashboard/ScrollLine.h. Eigener Takt, getrennt von BLINK_MS --
// Blinken und Scrollen duerfen unabhaengig voneinander schnell/langsam
// sein.
constexpr uint16_t SCROLL_MS      = 60;  // Scroll-Takt
constexpr uint8_t  SCROLL_STEP_PX = 2;   // Pixel je Scroll-Takt

// Ack-Karenzzone (siehe lib/Health/AckLatch.h): ein Fehler, der juenger
// als das ist, kann noch nicht quittiert werden -- erzwingt bewusste
// Kenntnisnahme statt reflexartigem Wegdruecken direkt beim Ausloesen.
constexpr uint16_t ACK_GRACE_MS = 1500;
