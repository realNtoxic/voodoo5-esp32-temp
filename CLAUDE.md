# ESP32 Thermal-Monitor — 3dfx Voodoo 5 5500

Firmware zur Temperaturueberwachung und Luefterregelung einer 3dfx Voodoo 5 5500
(zwei VSA-100-Chips) in einem Retro-Gaming-Rechner. Der ESP32 sitzt als
eigenstaendiger Monitor im Gehaeuse, misst per DS18B20 die Chiptemperaturen,
regelt zwei Noctua-Luefter und meldet Fehler per OLED und PC-Speaker.

Sprache in Kommentaren, Commit-Messages und Antworten: **Deutsch**.

---

## Build & Test

PlatformIO. Zwei Umgebungen:

```bash
pio test -e native      # Unity-Tests, host-nativ, kein Board noetig (Sekunden)
pio run  -e esp32       # Firmware bauen
pio run  -e esp32 -t upload
pio device monitor -b 115200
```

**Vor jedem Commit muss `pio test -e native` gruen sein.**

---

## Projektstruktur

```
lib/BootSelfTest/     Reine Logik, KEIN Arduino.h  -> nativ testbar
lib/FanControl/       Kennlinie, Hysterese, Kickstart
lib/AckButton/        Taster-Zustandsautomat
lib/Health/           Runtime-Monitor, Latch, Alarm
src/main.cpp          Reale HAL-Implementierung, nur env:esp32
test/                 Unity-Tests mit FakeHal
config.h              Zentrale Konfiguration
platformio.ini
```

**Warum `lib/` und nicht `src/`:** PlatformIO baut `src/` per Default nicht in
Tests mit (`test_build_src = no`). So bleibt `main.cpp` mit seinen
Arduino-Includes aus dem nativen Build raus.

---

## Architektur-Regeln (nicht verhandelbar)

1. **HAL-Abstraktion durchgaengig.** Jede Hardware-Interaktion laeuft ueber das
   `Hal`-Interface (virtuelle Methoden). Logik ruft niemals direkt `tone()`,
   `digitalWrite()`, `Wire` oder `sensors.*` auf.
2. **`config.h` bleibt Arduino-frei.** Nur `<cstdint>`, keine Bibliothekstypen.
   ROM-Adressen liegen als `uint8_t[3][8]`, nicht als `DeviceAddress`.
3. **Kennlinienfunktionen sind rein.** `curveDuty()` bekommt Temperatur und
   vorherigen Zustand herein und gibt Duty zurueck — keine Seiteneffekte,
   keine Zeitabhaengigkeit. Zeitliche Vorgaenge (Kickstart, Splash, Ack)
   leben im Controller.
4. **Kein `delay()` im Regelkreis.** Alles ueber `millis()`-Zeitstempel.
   `delay()` ist ausschliesslich im Boot-Selbsttest erlaubt.
5. **Schwellen und Zeiten nur in `config.h`.** Keine magischen Zahlen im Code.
6. **`static_assert` statt Laufzeitpruefung**, wo es zur Compile-Zeit geht.

---

## Hardware

Siehe `schaltplan_esp32_voodoo5.svg` im Repo.

| GPIO | Funktion | Beschaltung |
|------|-------------------|-------------------------------|
| 4    | 1-Wire DS18B20 x3 | 4,7k Pull-up -> 3V3 |
| 18   | Luefter 1 PWM     | direkt, 25 kHz |
| 19   | Luefter 2 PWM     | direkt, 25 kHz |
| 32   | Luefter 1 Tacho   | 10k Pull-up -> 3V3 |
| 33   | Luefter 2 Tacho   | 10k Pull-up -> 3V3 |
| 21   | I2C SDA (OLED)    | 0x3C |
| 22   | I2C SCL (OLED)    | |
| 25   | PC-Speaker        | 1k -> BC547, 1N4148 Freilauf |
| 27   | Ack-Taster        | INPUT_PULLUP, gegen GND |

Bauteile: ESP32 DevKit, 3x DS18B20, OLED SSD1306 128x64, 2x Noctua NF-A4x10
**5V** PWM, BC547, 1N4148, PC-Speaker, Taster 16 mm momentary.

**Fallen:**
- Pull-ups **immer gegen 3,3 V**, nie gegen 5 V (Tacho ist Open Collector).
- Luefter sind die **5V-Variante** — Molex rot, nicht gelb.
- Molex-GND und ESP32-GND muessen verbunden sein, sonst kein Tacho-Bezug.
- DS18B20 **nicht** parasitaer speisen, alle drei mit VDD.

---

## Funktionsuebersicht

### Boot-Selbsttest
Start-Piep -> OLED -> Sensoren (VSA1, VSA2, AMB per ROM-Adresse) ->
Luefter einzeln (1, dann 2; PWM 100 %, Anlaufzeit, Tacho verifizieren).
Fehler: Beep-Code (2 = OLED, 3 = Sensor, 4 = Luefter) **und** OLED-Zeile.
Bei OLED-Fehler nur akustisch.

### Discovery-Modus
Laeuft automatisch, wenn `SENSOR_ROM` keine gueltigen Adressen enthaelt
(Family-Byte != 0x28 oder CRC falsch) oder `FORCE_DISCOVERY == true`.
Scannt den Bus, gibt Adressen paste-fertig ueber Serial aus, zeigt "SETUP"
auf dem OLED. **Faehrt bewusst nicht in den Regelbetrieb** — ohne
Rollenzuordnung koennte der Ambient-Sensor als VSA gelesen werden.

### Anzeige (zwei Phasen)
- 0–15 s: Selbsttest-Diagnosebild
- danach: Dashboard, 4 Spalten x 3 Zeilen

```
        #1    #2   #Amb
T C     30    35    20
rpm      0   150     -
Sta   idle    OK     -
```

Ambient hat weder rpm noch Status -> `-` (Sentinel `rpm = -1`,
`ChStatus::NA`). Umschaltung ueber `millis()`, kein `delay(15000)`.

### Regelung
Semi-passiv. Unter 40 C stehen die Luefter (0 %). Ab 45 C Kickstart 30 % /
300 ms, dann linear 20 % (45 C) bis 100 % (70 C). 5 K Hysterese-Totband
gegen Pumpen. Eine Kurve fuer beide Zonen.

### Status je Kanal
`Idle` = Luefter aus und unter Einschaltschwelle ·
`Ok` = regelt normal ·
`Warn` = ab `WARN_C` ·
`Err` = Sensor antwortet nicht oder kein Tacho trotz Duty ·
`NA` = Ambient

### Health-Monitor (Runtime)
Laeuft in jedem Regelzyklus, nicht nur beim Boot.
- Sensor: `-127 C` = weg; `85.00 C` nur vor abgeschlossener Conversion gueltig;
  unveraenderter Wert ueber N Zyklen = eingefroren; einzelne CRC-Fehler ->
  Retry, erst wiederholte zaehlen.
- Luefter: Tacho **gegen kommandierte Duty** pruefen. `duty == 0` und
  `rpm == 0` ist korrekt (Semi-Passiv), kein Alarm.
- OLED: I2C-Adresse aktiv nachpollen, Ausfall nicht kritisch.
- Speaker: in Software **nicht** verifizierbar — daher Redundanz OLED + Beep.

Entprellung ueber `FAIL_PERSIST` Zyklen.

### Fail-Safe
- VSA-Sensor weg -> zugehoerigen Luefter auf **100 %** (Worst Case annehmen)
- Luefter weg trotz Duty -> Alarm, anderen Luefter auf 100 %
- OLED weg -> weiterregeln, Alarm nur akustisch, `oledInit()` periodisch neu

Der Monitor ist **passiv** — er kann die Grafikkarte nicht abschalten.

### Fehler-Latch und Alarm
Zwei Bitmasken: `latched` (was ist passiert) und `acked` (was wurde
quittiert). Alarm laeuft, solange `latched & ~acked != 0`.
`healthTick` **setzt** Bits nur, loescht nie — Erholung entfernt den Fehler
nicht. Ein neuer Fehler nach dem Quittieren loest den Ton erneut aus.

### Ack-Taster
Ton markiert den Ausloeser der Aktion:

| Haltedauer | Ton | Aktion |
|------------|-----|--------|
| < 500 ms | — | keine |
| 500–1200 ms | hell 2600 Hz bei 500 ms | Quittieren, beim Loslassen |
| >= 1200 ms | + tief 1800 Hz bei 1200 ms | Latch-Reset, sofort |

Quittieren schaltet nur den **Ton** stumm, die OLED-Markierung bleibt.

### Watchdog
`esp_task_wdt` mit 8 s. **Core 3.x API:** `esp_task_wdt_init()` nimmt eine
`esp_task_wdt_config_t`-Struct, nicht zwei Argumente. Erst **nach** dem
Selbsttest scharf schalten (der hat blockierende Phasen). Fuettern genau
einmal am Ende der Hauptschleife.

---

## Testkonventionen

FakeHal protokolliert Aufrufe in einen `std::vector<std::string>`. Tests
pruefen Reihenfolge und Inhalt, nicht nur Rueckgabewerte.

Wichtige Faelle:
- "VSA2 antwortet nicht" -> `{Sensor, 1}` **und** OLED-Ausgabe `VSA2 FEHLT`
- "Luefter 2 dreht nicht" -> `{Fan, 1}`, waehrend Luefter 1 vorher lief
- Hysterese: 42 C mit `fanWasOn = false` -> 0; mit `true` -> laufender Duty
- Ack: 2500 ms halten -> genau ein `LatchReset`, beim Loslassen **kein** `Ack`
- Ack: 300 ms halten -> kein Ton, kein Event

Zeit wird als Parameter hereingereicht (`now`), nie intern `millis()` in
testbarer Logik aufrufen.

**Grenze:** Unity testet Logik, nicht Hardware. Ob der Piezo klingt oder der
SSD1306 physisch antwortet, kann kein Test pruefen — das bleibt
Hardware-in-the-Loop oder Renode.

---

## Inbetriebnahme (stufenweise)

1. ESP32 + Speaker — Start-Piep
2. + OLED — Selbsttest-Anzeige
3. + DS18B20 **auf dem Steckbrett** — Discovery, Adressen zuordnen,
   Sensoren **sofort physisch markieren**
4. + Luefter — **zuerst nur einen** anschliessen, damit die Zuordnung
   1/2 eindeutig ist
5. Erst dann: Sensoren auf die Karte, Halter drucken, Einbau
