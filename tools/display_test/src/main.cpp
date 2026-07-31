// =============================================================
//  tools/display_test/main.cpp — Eigenstaendiger Display-Bring-up-Test.
//
//  Zweck: NUR pruefen, ob das Dashboard-Layout (4 Luefter-Spalten +
//  Ambient-Segment in der SelfDiag-Zeile) auf dem echten SSD1306
//  richtig aussieht. Keine Regelung, keine Sensoren, kein Hal/IDisplay
//  aus der Hauptfirmware -- bewusst vollstaendig eigenstaendig, damit
//  dieses Tool die Hauptfirmware (src/main.cpp, lib/) nicht beruehrt.
//  Alle Layoutwerte sind lokale Konstanten (kein #include "config.h"),
//  weil das Dashboard-Modul aktuell noch 3 Datenspalten (VSA1/VSA2/Amb)
//  zeichnet -- dieses Tool testet bereits das kuenftige 4-Luefter-
//  Layout aus dem Regelmodell (siehe CLAUDE.md "Regelmodell").
//
//  Bauen/Flashen: pio run -e disptest -t upload
// =============================================================
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <cstdio>
#include <cstring>

namespace {

// --- I2C / OLED -------------------------------------------------
constexpr uint8_t  PIN_SDA      = 21;
constexpr uint8_t  PIN_SCL      = 22;
constexpr uint8_t  OLED_ADDR    = 0x3C;
constexpr uint8_t  OLED_WIDTH   = 128;
constexpr uint8_t  OLED_HEIGHT  = 64;
constexpr uint32_t I2C_CLOCK_HZ = 50000;

// --- Layoutwerte (kuenftiges 4-Luefter-Dashboard, aus OLED
// Dashboard Test auf dem echten Display ermittelt) ----------------
constexpr uint8_t  COL_X[5]  = { 0, 21, 48, 75, 102 };  // Label,#1,#2,#3,#4
constexpr uint8_t  CELL_W[5] = { 18, 24, 24, 24, 24 };
constexpr uint8_t  ROW_Y[4]  = { 0, 10, 22, 34 };       // Kopf,Temp,rpm,Status
constexpr uint8_t  TXT_SIZE_HEAD = 1;
constexpr uint8_t  TXT_SIZE_VAL  = 1;
constexpr bool     HEADER_UNDERLINE = false;
constexpr bool     VSEP = true;
constexpr bool     HSEP = true;
constexpr uint16_t BLINK_MS = 500;
constexpr char      HEARTBEAT_A = '|';
constexpr char      HEARTBEAT_B = '-';
constexpr bool      SELFDIAG_ROW = true;
constexpr uint8_t   SELFDIAG_Y   = 53;

// Default-Font (Adafruit_GFX, Groesse 1) ist 6x8 px pro Zeichen --
// gebraucht, um die Fuellbreite des Ambient-Segments in der SelfDiag-
// Zeile aus der Textlaenge zu berechnen.
constexpr uint8_t GLYPH_W = 6;
constexpr uint8_t GLYPH_H = 8;

enum class ChStatus : uint8_t { Idle, Ok, Warn, Err };

struct ChannelDummy {
  const char* temp;
  const char* rpm;
  ChStatus    status;
};

// --- Dummy-Daten: alle Zustaende auf einen Blick -----------------
constexpr ChannelDummy CHANNELS[4] = {
  { "41", "0",    ChStatus::Ok   },
  { "44", "3090", ChStatus::Warn },
  { "40", "2800", ChStatus::Err  },
  { "43", "0",    ChStatus::Idle },
};
constexpr const char* AMB_TEMP     = "26";
constexpr ChStatus    AMB_STATUS   = ChStatus::Warn;
constexpr const char* SELFDIAG_REST = "Heap 142k  45Hz";

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
uint32_t lastPhaseSwitchMs = 0;
uint8_t  phase = 0;

// --- Reine Render-Regeln, 1:1 wie im Dashboard-Modul -------------
bool cellInverted(ChStatus st, uint8_t ph) {
  if (st == ChStatus::Warn) return true;
  if (st == ChStatus::Err)  return ph != 0;
  return false;
}

char heartbeatChar(uint8_t ph) {
  return ph != 0 ? HEARTBEAT_A : HEARTBEAT_B;
}

// Fuellbreite einer invertierten Zelle, geclippt an die naechste
// Spaltengrenze (bzw. Displayrand bei der letzten Spalte) -- eine
// Invert-Markierung darf nie in die Nachbarzelle laufen.
uint8_t invertFillWidth(uint8_t col) {
  const uint8_t naturalWidth = CELL_W[col];
  uint8_t gap;
  if (col + 1 < 5) {
    gap = static_cast<uint8_t>(COL_X[col + 1] - COL_X[col]);
  } else {
    gap = static_cast<uint8_t>(OLED_WIDTH - COL_X[col]);
  }
  return naturalWidth < gap ? naturalWidth : gap;
}

// --- I2C-Bus-Recovery + Praesenz-Check (Pflicht, siehe CLAUDE.md
// Hardware-Fallen) --------------------------------------------------
void i2cRecover(uint8_t sdaPin, uint8_t sclPin) {
  // Bus-Recovery per Bit-Banging: haelt das durchgehend versorgte
  // SSD1306 SDA nach einem Reset mitten in einer Uebertragung auf
  // LOW, wird der Bus erst durch weiteres Takten wieder frei. Muss
  // VOR Wire.begin() laufen, solange die Pins noch einfache GPIOs sind.
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

  // STOP-Bedingung nur erzeugen, wenn der Bus tatsaechlich haengt
  // (pulses > 0). Auf einem gesunden, idle-High-Bus wuerde der
  // manuelle SDA-Puls das OLED sonst unnoetig waehrend seines eigenen
  // Power-Ups stoeren.
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

// --- Zeichenhilfen ------------------------------------------------
void drawText(uint8_t x, uint8_t y, const char* s, uint8_t size, bool inverted) {
  display.setTextSize(size);
  display.setTextColor(inverted ? SSD1306_BLACK : SSD1306_WHITE);
  display.setCursor(x, y);
  display.print(s);
}

const char* statusText(ChStatus st) {
  switch (st) {
    case ChStatus::Idle: return "idle";
    case ChStatus::Ok:   return "OK";
    case ChStatus::Warn: return "WARN";
    case ChStatus::Err:  return "ERR";
  }
  return "?";
}

void drawStatusCell(uint8_t col, ChStatus status, uint8_t ph) {
  const bool inv = cellInverted(status, ph);
  const uint8_t x = COL_X[col];
  const uint8_t y = ROW_Y[3];
  if (inv) {
    const uint8_t w = invertFillWidth(col);
    const uint8_t h = static_cast<uint8_t>(GLYPH_H * TXT_SIZE_VAL + 1);
    display.fillRect(x, static_cast<uint8_t>(y - 1), w, h, SSD1306_WHITE);
  }
  drawText(x, y, statusText(status), TXT_SIZE_VAL, inv);
}

void renderFrame(uint8_t ph) {
  display.clearDisplay();

  // Kopfzeile: Lebenszeichen (0;0) + #1..#4.
  const char hb[2] = { heartbeatChar(ph), '\0' };
  drawText(COL_X[0], ROW_Y[0], hb, TXT_SIZE_HEAD, false);
  drawText(COL_X[1], ROW_Y[0], "#1", TXT_SIZE_HEAD, false);
  drawText(COL_X[2], ROW_Y[0], "#2", TXT_SIZE_HEAD, false);
  drawText(COL_X[3], ROW_Y[0], "#3", TXT_SIZE_HEAD, false);
  drawText(COL_X[4], ROW_Y[0], "#4", TXT_SIZE_HEAD, false);

  if (HEADER_UNDERLINE) {
    display.drawFastHLine(0, static_cast<uint8_t>(ROW_Y[1] - 1), OLED_WIDTH, SSD1306_WHITE);
  }

  // Temp-Zeile.
  drawText(COL_X[0], ROW_Y[1], "T C", TXT_SIZE_VAL, false);
  for (uint8_t i = 0; i < 4; ++i) {
    drawText(COL_X[i + 1], ROW_Y[1], CHANNELS[i].temp, TXT_SIZE_VAL, false);
  }

  if (HSEP) {
    display.drawFastHLine(0, static_cast<uint8_t>(ROW_Y[2] - 1), OLED_WIDTH, SSD1306_WHITE);
  }

  // rpm-Zeile.
  drawText(COL_X[0], ROW_Y[2], "rpm", TXT_SIZE_VAL, false);
  for (uint8_t i = 0; i < 4; ++i) {
    drawText(COL_X[i + 1], ROW_Y[2], CHANNELS[i].rpm, TXT_SIZE_VAL, false);
  }

  if (HSEP) {
    display.drawFastHLine(0, static_cast<uint8_t>(ROW_Y[3] - 1), OLED_WIDTH, SSD1306_WHITE);
  }

  // Status-Zeile.
  drawText(COL_X[0], ROW_Y[3], "Sta", TXT_SIZE_VAL, false);
  for (uint8_t i = 0; i < 4; ++i) {
    drawStatusCell(i + 1, CHANNELS[i].status, ph);
  }

  // Trennlinien nur INNEN: senkrechte Linien enden an der Unterkante
  // der Statuszeile, nicht am Displayrand -- Zeile 5 (SelfDiag) bleibt frei.
  if (VSEP) {
    const uint8_t vLineBottom = static_cast<uint8_t>(ROW_Y[3] + GLYPH_H * TXT_SIZE_VAL);
    for (uint8_t col = 0; col < 4; ++col) {
      const uint8_t gapStart = static_cast<uint8_t>(COL_X[col] + CELL_W[col]);
      const uint8_t gapEnd = COL_X[col + 1];
      display.drawFastVLine(static_cast<uint8_t>((gapStart + gapEnd) / 2), 0, vLineBottom, SSD1306_WHITE);
    }
  }

  // Zeile 5 (SelfDiag), ausserhalb des Rasters: links ein Ambient-
  // Segment, das auf seinen eigenen Status reagiert, dahinter freier
  // Rest-Text.
  if (SELFDIAG_ROW) {
    char ambBuf[12];
    std::snprintf(ambBuf, sizeof(ambBuf), "Amb:%s", AMB_TEMP);
    const bool ambInv = cellInverted(AMB_STATUS, ph);
    const uint8_t ambW = static_cast<uint8_t>(std::strlen(ambBuf) * GLYPH_W * TXT_SIZE_VAL);
    if (ambInv) {
      const uint8_t h = static_cast<uint8_t>(GLYPH_H * TXT_SIZE_VAL + 1);
      display.fillRect(0, static_cast<uint8_t>(SELFDIAG_Y - 1), ambW, h, SSD1306_WHITE);
    }
    drawText(0, SELFDIAG_Y, ambBuf, TXT_SIZE_VAL, ambInv);
    drawText(static_cast<uint8_t>(ambW + 4), SELFDIAG_Y, SELFDIAG_REST, TXT_SIZE_VAL, false);
  }

  display.display();  // genau ein display() pro Frame
}

}  // namespace

void setup() {
  Serial.begin(115200);

  i2cRecover(PIN_SDA, PIN_SCL);

  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(I2C_CLOCK_HZ);
  Wire.setTimeOut(50);

  const bool present = oledPresent();
  Serial.println("Display-Test aktiv");
  Serial.printf("I2C-Praesenzcheck 0x%02X -> %s\n", OLED_ADDR,
                present ? "gefunden" : "NICHT gefunden");

  if (!present || !display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED-Init fehlgeschlagen -- Test angehalten.");
    // Reiner Diagnose-Halt in einem Wegwerf-Tool, kein Regelkreis ->
    // delay() hier unkritisch (Architektur-Regel 4 gilt fuer die
    // Hauptfirmware).
    while (true) {
      delay(1000);
    }
  }

  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  renderFrame(phase);  // erstes Bild sofort, nicht erst nach BLINK_MS
  lastPhaseSwitchMs = millis();
}

void loop() {
  const uint32_t now = millis();
  if (now - lastPhaseSwitchMs >= BLINK_MS) {
    lastPhaseSwitchMs = now;
    phase = phase == 0 ? 1 : 0;
    renderFrame(phase);
  }
}
