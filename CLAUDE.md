# ESP32 Thermal-Monitor — 3dfx Voodoo 5 5500

Firmware zur Temperaturueberwachung und Luefterregelung einer 3dfx Voodoo 5 5500
(zwei VSA-100-Chips) in einem Retro-Gaming-Rechner. Der ESP32 sitzt als
eigenstaendiger Monitor im Gehaeuse, misst per DS18B20 die Chiptemperaturen,
regelt vier Noctua-Luefter und meldet Fehler per OLED und PC-Speaker.

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
lib/Hal/              Gemeinsame Hal-Schnittstelle (BootSelfTest, AckButton, ...)
lib/BootSelfTest/     Reine Logik, KEIN Arduino.h  -> nativ testbar
lib/FanControl/       Kennlinie (curveDuty), Ambient-Panik, Sensor-Fail-Safe
lib/SensorCal/        Lastabhaengige Die-Korrektur (dieTempC: T_sonde + k*(T_sonde-T_amb))
lib/AckButton/        Taster-Zustandsautomat
lib/Health/           Runtime-Monitor, Latch, Alarm
lib/Dashboard/        Anzeige-Layout + Render-Logik, ueber IDisplay testbar
lib/SelfDiag/         Freie Diagnose-Zeile (Heap/Loop-Hz), Komfort
lib/StatusLed/        Heartbeat-Grundzustand fuer die LED (GPIO2)
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
   ROM-Adressen liegen als `uint8_t[5][8]`, nicht als `DeviceAddress`.
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
|------|-------------------------------------------------------------|-------------------------------------------------------------|
| 4    | 1-Wire Bus (5x DS18B20: VSA1.1, VSA1.2, VSA2.1, VSA2.2, AMB) | 4,7k Pull-up -> 3V3 |
| 18   | PWM1 (Luefter VSA1 Rueckseite / Zone A)                      | direkt, 25 kHz |
| 32   | TACHO1 (Luefter VSA1 Rueckseite)                             | 10k Pull-up -> 3V3 |
| 19   | PWM2 (Luefter VSA2 Rueckseite / Zone B)                      | direkt, 25 kHz |
| 33   | TACHO2 (Luefter VSA2 Rueckseite)                             | 10k Pull-up -> 3V3 |
| 23   | PWM3 (Luefter VSA1 Vorderseite, NEU)                         | direkt, 25 kHz |
| 34   | TACHO3 (Luefter VSA1 Vorderseite, NEU)                       | 10k Pull-up -> 3V3, **extern** (GPIO34 ist Input-only, kein interner Pull-up) |
| 26   | PWM4 (Luefter VSA2 Vorderseite, NEU)                         | direkt, 25 kHz |
| 35   | TACHO4 (Luefter VSA2 Vorderseite, NEU)                       | 10k Pull-up -> 3V3, **extern** (GPIO35 ist Input-only, kein interner Pull-up) |
| 21   | I2C SDA (OLED SSD1306)                                       | 0x3C |
| 22   | I2C SCL (OLED SSD1306)                                       | |
| 25   | PC-Speaker (Basis BC547 Treiberstufe)                        | 1k -> BC547, 1N4148 Freilauf |
| 27   | Ack-Taster                                                   | INPUT_PULLUP, gegen GND |

Bauteile: ESP32 DevKit, 5x DS18B20, OLED SSD1306 128x64, 4x Noctua NF-A4x10
**5V** PWM, BC547, 1N4148, PC-Speaker, Taster 16 mm momentary.

**Fallen:**
- Pull-ups **immer gegen 3,3 V**, nie gegen 5 V (Tacho ist Open Collector).
- Luefter sind die **5V-Variante** — Molex rot, nicht gelb.
- Molex-GND und ESP32-GND muessen verbunden sein, sonst kein Tacho-Bezug.
- DS18B20 **nicht** parasitaer speisen, alle fuenf mit VDD.
- **I2C blockiert ohne angeschlossenes Geraet.** Haengen SDA/SCL frei in
  der Luft, kann `Wire` auf dem ESP32 unbegrenzt blockieren statt "kein
  Geraet" zu melden. Der Selbsttest haengt dann in `oledInit()` fest,
  bevor die Fehlerpiepse ueberhaupt drankommen. Pflicht: `Wire.setTimeOut()`
  setzen und vor jedem `begin()` einen expliziten Praesenz-Check per
  `beginTransmission`/`endTransmission`. Dieselbe Pruefung ist auch
  `hal.oledPresent()` fuer den Health-Monitor.
- **I2C bleibt nach einem Reset blockiert.** Startet der ESP32 mitten
  in einer Uebertragung neu (EN-Taste, Watchdog, Brownout), haelt das
  durchgehend versorgte SSD1306 SDA auf LOW und der Bus ist tot —
  erkennbar an TIMEOUT (5) statt NACK (2) im Scan. Deshalb ist
  `i2cRecover()` vor jedem `Wire.begin()` Pflicht, nicht optional: Ein
  Watchdog-Reset im Betrieb erzeugt exakt diese Situation, und ohne
  Recovery waere der Monitor danach blind.
- **`tone()`/`noTone()` sind bei schnell aufeinanderfolgenden Aufrufen
  unzuverlaessig.** Der zweite Ton nach kurzer Pause startet oft nicht
  neu. Toene werden daher per `digitalWrite()`/`delayMicroseconds()` als
  Rechteck erzeugt. LEDC wurde bewusst verworfen (API unterscheidet sich
  je nach Core-Version: `ledcAttach` vs. `ledcSetup`/`ledcAttachPin`).
- **Alle 4 Tacho-Kanaele brauchen einen externen 10k-Pull-up, auch
  GPIO32/33.** GPIO34/35 sind Input-only ohne internen Pull-up — dort
  ist der externe Widerstand zwingend. GPIO32/33 koennten sich
  theoretisch auf den internen ESP32-Pull-up verlassen, bekommen aber
  bewusst denselben externen 10k-Pull-up spendiert: intern **und**
  extern parallel ist unkritisch, vereinheitlicht aber das Layout aller
  vier Kanaele.

---

## Funktionsuebersicht

### Boot-Selbsttest
Start-Piep -> OLED -> Sensoren (VSA1.1, VSA1.2, VSA2.1, VSA2.2, AMB per
ROM-Adresse) -> Luefter einzeln (1, 2, 3, 4 nacheinander; PWM 100 %,
Anlaufzeit, Tacho verifizieren) — die Zuordnung ergibt sich eindeutig
aus dem Einzel-Anlauf, nicht durch Zonen-Vergleich.
Fehler: Beep-Code (2 = OLED, 3 = Sensor, 4 = Luefter) **und** OLED-Zeile.
Bei OLED-Fehler nur akustisch.

### Discovery-Modus
Laeuft automatisch, wenn `SENSOR_ROM` keine gueltigen Adressen enthaelt
(Family-Byte != 0x28 oder CRC falsch) oder `FORCE_DISCOVERY == true`.
Scannt den Bus, gibt Adressen paste-fertig ueber Serial aus, zeigt "SETUP"
auf dem OLED. **Faehrt bewusst nicht in den Regelbetrieb** — ohne
Rollenzuordnung koennte der Ambient-Sensor als VSA gelesen werden.

Rollen: `0 = VSA1.1`, `1 = VSA1.2`, `2 = VSA2.1`, `3 = VSA2.2`, `4 = AMB`
(siehe `config.h` `ROLE_*`). Jede Sonde einzeln anwaermen und beobachten,
welche ROM-Adresse reagiert. Bei 5 Sonden ist die sofortige physische
Markierung nach der Zuordnung noch kritischer als bei 3: Eine Verwechslung
bedeutet, dass ein Luefter auf dem falschen Chip regelt (siehe
"Regelmodell").

### Anzeige (zwei Phasen)
- 0–15 s: Selbsttest-Diagnosebild
- danach: Dashboard, aktuell 4 Spalten x 4 Zeilen (Kopf, T, rpm, Status;
  Spalten Label, VSA1, VSA2, Amb) plus eine freie SelfDiag-Zeile
  ausserhalb des Rasters (siehe "Dashboard-Anzeige & Meldekanaele")

```
    #1    #2   #Amb
T C 30    35    20
rpm  0   150     -
Sta idle  OK    OK
```

(Zelle 0;0 der Kopfzeile zeigt das Lebenszeichen `heartbeatChar(phase)`,
nicht dargestellt im Mockup.)

Ambient hat KEINE rpm (kein Luefter -> `-`, Sentinel `rpm = -1`), aber
vollen Status `Ok`/`Warn`/`Err` wie VSA1/VSA2 — siehe "Ambient-Umbau".
Umschaltung ueber `millis()`, kein `delay(15000)`.

**Ziel-Layout (geplant, noch NICHT in `lib/Dashboard/` umgesetzt):**
Vier Kanalspalten `#1`..`#4` (je ein Luefter-Regelkreis, siehe
"Regelmodell") statt VSA1/VSA2/Amb. Ambient wandert aus der
Kanaltabelle in die freie Zeile 5 und wird dort zu einem eigenen
Segment. Pixelwerte bereits auf echter Hardware ueber
`tools/display_test/` abgestimmt:

```
    |  #1   #2   #3   #4
  T | 41   44   40   43
rpm | 0   3090 2800   0
Sta | OK  Warn  Err  idle
[Amb:26] Heap 142k 45Hz
```

Zelle 0;0 zeigt weiterhin das Lebenszeichen. Ambient ist hier **keine**
Kanalspalte mehr, sondern das Segment `Amb:<temp>` links in Zeile 5 —
reagiert wie eine Statuszelle auf seinen Status (Warn -> invertiert,
Err -> blinkt), hat aber keine rpm-Anzeige (kein eigener Luefter, kein
rpm-Konzept im Segment). Der Umbau von `lib/Dashboard/Dashboard.cpp`
auf dieses Layout folgt in einem spaeteren Schritt.

### Regelung
Semi-passiv. Unter 40 C stehen die Luefter (0 %). Ab 45 C Kickstart 30 % /
300 ms, dann linear 20 % (45 C) bis 100 % (70 C). 5 K Hysterese-Totband
gegen Pumpen. Dieselbe Kurve (`curveDuty()`) wird viermal unabhaengig
angewandt — siehe "Regelmodell".

### Regelmodell
4 unabhaengige Regelkreise, Zuordnung 1:1:1 (**kein** Zonen-Mischen,
**kein** Max/Mittelwert ueber mehrere Sonden):

| Sensor | Luefter | Kanal |
|--------|---------|-------|
| VSA1.1 | Luefter 1 (PWM1/TACHO1) | VSA1 Rueckseite / Zone A |
| VSA1.2 | Luefter 3 (PWM3/TACHO3) | VSA1 Vorderseite |
| VSA2.1 | Luefter 2 (PWM2/TACHO2) | VSA2 Rueckseite / Zone B |
| VSA2.2 | Luefter 4 (PWM4/TACHO4) | VSA2 Vorderseite |

Jeder Luefter reagiert **nur** auf seinen zugeordneten Sensor. Dieselbe
reine `curveDuty()`-Logik (Hysterese, Kickstart, Semi-Passiv) gilt pro
Kreis, unabhaengig voneinander. Kennlinienparameter (`CURVE` in
`config.h`) bleiben gemeinsam, sofern nicht spaeter pro Kanal abweichend
gewuenscht.

Der 5. Sensor (AMB) ist **kein** eigener Regelkreis, sondern speist
ausschliesslich die Panik-Uebersteuerung (`ambientPanicActive()` /
`effectiveDuty()`, siehe "Ambient-Umbau"). Bei Panik-Ausloesung gehen
**alle vier** Luefter auf 100 % (`max(Kurve, Panik)` je Kanal).

Alle vier Luefter teilen sich einen gemeinsamen Molex-Stromanschluss —
nur die PWM-/Tacho-Signale sind pro Luefter separat.

### Sensor-Kalibrierung (lastabhaengige Die-Korrektur)
**Ein konstanter Offset ist physikalisch falsch** und wird nicht
verwendet: Er wuerde behaupten, das Die sei *immer* um X °C waermer
als die Sonde — auch bei stehender Karte. Tatsaechlich ist die
Differenz bei Nulllast selbst null und waechst mit der
Verlustleistung (`ΔT = P · R_thermisch`). Da `P` nicht gemessen wird,
dient die Kuehler-Uebertemperatur ueber Ambient (`T_sonde - T_amb`)
als monoton steigender Stellvertreter fuer die Last:

```
T_die ≈ T_sonde + k * (T_sonde - T_amb)      (dieTempC(), lib/SensorCal/)
```

`SENSOR_K[5]` (config.h) haelt den empirisch (per Waermebildkamera)
bestimmten Faktor `k` je Sensor-Rolle, `k = 0` bedeutet keine
Korrektur. Index 4 (AMB) ist der Ambient-Sensor selbst — sein `k`
bleibt per Definition 0 (Referenz, wird nicht korrigiert).

**Regel unveraendert:** reine Regel-/Anzeige-Groesse. Die
Rohtemperatur geht immer unveraendert ins Log (Ebene 1/2), die
Korrektur darf sie dort **niemals** ueberschreiben — sonst geht bei
einer spaeteren Nachkalibrierung die Historie verloren. Also:
`rawSondeC` -> Log; `calC = dieTempC(rawSondeC, ambC, k)` ->
Regelung/Anzeige.

**Kalibrierung von `k`:** mehrere Lastpunkte anfahren (Idle, mittlere
Last, Volllast). Je Punkt `(T_sonde - T_amb)` auf x und
`(T_die_Kamera - T_sonde)` auf y auftragen — die Steigung ist `k`.
Erst mit einem linearen `k` arbeiten; nur falls die Messpunkte
deutliche Kruemmung zeigen, `k` spaeter zu einer kleinen Kennlinie
(Stuetzstellen + Interpolation) ausbauen. Die Struktur (`dieTempC()`
nimmt `k` als einfachen Parameter) laesst diesen Schritt offen, ohne
ihn vorwegzunehmen.

### Status je Kanal
`Idle` = Luefter aus und unter Einschaltschwelle ·
`Ok` = regelt normal ·
`Warn` = ab `WARN_C` (bei Ambient: ab `AMB_WARN_C`) ·
`Err` = Sensor antwortet nicht oder kein Tacho trotz Duty

Ambient durchlaeuft dieselben vier Status wie VSA1.1/VSA1.2/VSA2.1/VSA2.2
(kein fest verdrahteter `NA`-Status mehr, siehe "Ambient-Umbau"). Nur die
rpm-Zelle bleibt fuer Ambient immer `-` (Sentinel `rpm = -1`) — Ambient
hat schlicht keinen eigenen Luefter, unabhaengig vom Status. Diese vier
Status sind reine Regelungs-/Health-Semantik, unabhaengig von der
Darstellung: aktuell ist Ambient eine Dashboard-Spalte mit rpm-Zelle
`-`, im geplanten Ziel-Layout (siehe "Anzeige") zeigt stattdessen das
Ambient-Segment in Zeile 5 denselben Status — dort ganz ohne
rpm-Konzept.

### Health-Monitor (Runtime)
Laeuft in jedem Regelzyklus, nicht nur beim Boot.
- Sensor: `-127 C` = weg; `85.00 C` nur vor abgeschlossener Conversion gueltig;
  unveraenderter Wert ueber N Zyklen = eingefroren; einzelne CRC-Fehler ->
  Retry, erst wiederholte zaehlen. Gilt fuer alle 5 Sonden (VSA1.1,
  VSA1.2, VSA2.1, VSA2.2, AMB) gleichermassen.
- Luefter: Tacho **gegen kommandierte Duty** pruefen. `duty == 0` und
  `rpm == 0` ist korrekt (Semi-Passiv), kein Alarm.
- OLED: I2C-Adresse aktiv nachpollen, Ausfall nicht kritisch.
- Speaker: in Software **nicht** verifizierbar — daher Redundanz OLED + Beep.

Entprellung ueber `FAIL_PERSIST` Zyklen.

### Fail-Safe
- Sensorausfall -> **nur** der zugeordnete Luefter dieses Regelkreises auf
  100 % (Worst Case annehmen, `channelFailSafeDuty()`). Die anderen drei
  Kreise regeln unveraendert weiter — pro Kreis unabhaengig, **keine**
  2-Kanal-Kopplung mehr.
- Luefter weg trotz Duty -> Alarm. Pro Kreis unabhaengig: keine
  automatische Kompensation durch einen anderen Kreis mehr (das war
  2-Kanal-Logik mit genau einem "anderen" Luefter).
- OLED weg -> weiterregeln, Alarm nur akustisch, `oledInit()` periodisch neu

Der Monitor ist **passiv** — er kann die Grafikkarte nicht abschalten.

### Fehler-Latch und Alarm
Zwei Bitmasken: `latched` (was ist passiert) und `acked` (was wurde
quittiert). Alarm laeuft, solange `latched & ~acked != 0`.
`healthTick` **setzt** Bits nur, loescht nie — Erholung entfernt den Fehler
nicht. Ein neuer Fehler nach dem Quittieren loest den Ton erneut aus.

### Akustik (Speaker)
Der bit-gebangte Ton (siehe "Fallen") **blockiert**. Im Boot-Selbsttest
ist das erlaubt und unkritisch. Im **Laufbetrieb nicht**: Wiederhol-Alarm
und Taster-Bestaetigungen duerfen den Regelkreis nicht anhalten. Ein
120-ms-Ton an der 500-ms-Schwelle wuerde sonst die Erkennung der
1200-ms-Schwelle um 120 ms verschieben.

`hal.beepAsync()` wird daher ueber eine FreeRTOS-Task (Core 0) realisiert,
die die Rechteck-Erzeugung uebernimmt; die Hauptschleife stoesst nur an
und kehrt sofort zurueck. `hal.beep()` (blockierend) bleibt
ausschliesslich dem Boot-Selbsttest vorbehalten.

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

## Logging

### Grundsatz (nicht verhandelbar)
- Logging ist Komfort, nicht Teil des Sicherheitspfads. Faellt Logging
  aus (Flash voll, WLAN weg, Dateizugriff schlaegt fehl), darf das die
  Regelung und den Health-Monitor **niemals** beeinflussen.
- Logging ist strikt nicht-blockierend. Kein Dateizugriff und keine
  Netzwerkanfrage darf den Regelkreis anhalten (sonst verschiebt sich
  die Tacho-Messung, wie zuvor bei den Beeps). Datei-/Netz-Arbeit laeuft
  in einer eigenen FreeRTOS-Task auf Core 0; Regelung und Health-Monitor
  bleiben auf Core 1.

### Ebene 1 — Serielles Log (Basis)
- Strukturierte Logzeilen ueber Serial: Zeitstempel (`millis`), je Kanal
  Temp/Duty/RPM/Status, sowie Fehler-Events (`latched`/`acked`
  Aenderungen).
- Festes, maschinenlesbares Format (z. B. CSV-artig), damit der
  PlatformIO-Filter `log2file` es direkt in eine Datei auf dem Host
  schreiben kann. Funktioniert nur mit USB.
- Ausgabe ueber die HAL (z. B. `hal.logLine(...)`), nicht direkt
  `Serial` im Logikcode — konsistent mit der HAL-Regel.

### Ebene 2 — Persistentes Log auf dem ESP32 (LittleFS)
- Ringpuffer-Logdatei auf LittleFS; alte Zeilen werden ueberschrieben,
  damit der Flash nicht volllaeuft.
- Schreibzyklen schonen: nicht jede Messung sofort committen, sondern
  gepuffert sammeln und periodisch flushen (Intervall als benannte
  Konstante, siehe `config.h`).
- Laeuft unabhaengig vom USB-Anschluss.
- Fehler beim Dateizugriff werden abgefangen und ignoriert (siehe
  Grundsatz) — kein Einfluss auf die Regelung.

### Ebene 3 — Abruf im Betrieb ohne Neustart (WLAN, optional, spaeter)
Als eigener, klar getrennter Baustein zu implementieren, **nachdem**
Ebene 1 und 2 stehen und stabil sind. Nicht mit der Kernfirmware
vermischen.
- Schlanker HTTP-Server als FreeRTOS-Task auf Core 0. Endpunkt liefert
  die LittleFS-Logdatei auf Abruf (z. B. `GET /log`). Die Regelung im
  `loop()` laeuft dabei ungestoert weiter.
- WLAN-Koexistenz beachten: Der ESP sitzt im Metallgehaeuse neben
  schaltender Elektronik; WLAN kann schwaecheln. Fuer das Log
  unkritisch, aber es darf keine Annahme geben, dass der Server immer
  erreichbar ist.

### Konfiguration (`config.h`)
- `LOG_ENABLED` — Logging global ein-/ausschalten.
- `LOG_FLUSH_MS` — Flush-Intervall fuer den LittleFS-Ringpuffer.
- `LOG_RINGBUFFER_BYTES` — maximale Groesse der Logdatei.
- WLAN-/HTTP-Konstanten fuer Ebene 3 werden erst angelegt, wenn dieser
  Baustein tatsaechlich gebaut wird.

### Umsetzungsreihenfolge
1. Ebene 1 (Serial-Log ueber HAL)
2. Ebene 2 (LittleFS-Ringpuffer)
3. Ebene 3 (WLAN-Abruf) — separat, spaeter, optional.

---

## Dashboard-Anzeige & Meldekanaele

### Dashboard-Klasse (`lib/Dashboard/`)
- Zeichnet ausschliesslich ueber `IDisplay`
  (`clear/drawText/fillRect/hLine/vLine/present`) — kein direkter
  `Adafruit_SSD1306`-Zugriff in der Logik, dadurch nativ mit
  `FakeDisplay` testbar. Reale Umsetzung (`Esp32Display` in
  `src/main.cpp`) kapselt `Adafruit_SSD1306`, ist aber noch nicht in
  `setup()/loop()` verdrahtet — dafuer fehlt die Sensor-/Luefter-
  Datenquelle, die erst in einem spaeteren Schritt entsteht.
- `Dashboard::render(const DashboardData&, uint8_t phase)` zeichnet zwei
  Zeilentypen: die Kanal-Zeilen (4 Spalten x 4 Zeilen nach
  `COL_X`/`ROW_Y`) und die freie SelfDiag-Zeile (Zeile 5, `SELFDIAG_Y`,
  volle Breite, ausserhalb des Rasters).
- Rechts der aktuellen dritten Spalte (Amb, ab x ≈ 105) bleiben rein
  rechnerisch ~23 px frei. Im geplanten Ziel-Layout (vier Kanalspalten
  statt drei, siehe "Anzeige") wird dieser Platz anders genutzt — mit
  eigenem `COL_X` fuer vier statt drei Datenspalten.

### Layout-Regeln
- Feste Zellbreiten (`CELL_W`). Eine invertierte Zelle (Warn/Err) fuellt
  sich per `fillRect` (1 px Luft oben, Hoehe `8*size+1`); die Fuellbreite
  wird IMMER an der naechsten Spaltengrenze geclippt (`invertFillWidth()`,
  min(`CELL_W`, Luecke zur naechsten Spalte bzw. zum Bildschirmrand bei
  der letzten Spalte)) — eine Invert-Markierung laeuft nie in die
  Nachbarzelle.
- Trennlinien nur INNEN, nie vor der ersten oder nach der letzten
  Zeile/Spalte: `HEADER_UNDERLINE` unterstreicht die Kopfzeile, `HSEP`
  trennt die uebrigen Datenzeilen, `VSEP` (Default aus) trennt Spalten
  mittig in der Luecke. Horizontale Linien liegen 1 px oberhalb der
  jeweils folgenden Zeile.

### Statuszellen
- `cellInverted(status, phase)`: `Warn` -> immer invertiert; `Err` ->
  blinkt im Takt von `phase` (`phase != 0`); `Ok`/`Idle` -> nie.
- Gilt aktuell fuer ALLE drei Kanaele inkl. Ambient — Ambient ist nicht
  mehr fest auf `NA` gesetzt, sondern durchlaeuft dieselbe Ok/Warn/Err-
  Logik wie VSA1/VSA2 (siehe "Ambient-Umbau" unten). Die rpm-Zelle
  bleibt fuer Ambient trotzdem immer "-": Ambient hat schlicht keinen
  Luefter. Im geplanten Ziel-Layout (vier Kanalspalten `#1`..`#4` plus
  Ambient-Segment in Zeile 5, siehe "Anzeige") gilt dieselbe
  `cellInverted()`-Regel unveraendert weiter — fuer die vier
  Kanalspalten genauso wie fuer das Ambient-Segment.

### Lebenszeichen (Zelle 0;0)
`heartbeatChar(phase)`: `phase != 0` -> `HEARTBEAT_A`, sonst
`HEARTBEAT_B`. **Wichtig:** `phase` kommt ausschliesslich aus dem
Regelkreis (Hauptloop, bevorzugt ein Zaehler, der pro erfolgreichem
Sensor-/Regelzyklus erhoeht wird), niemals aus einem eigenen
Display-Timer. Ein eigener Timer wuerde weiterlaufen, selbst wenn der
Sensorpfad haengt — das Lebenszeichen beweist Liveness nur, wenn sein
Takt tatsaechlich aus dem ueberwachten Kreislauf stammt.

### Ambient-Umbau (Regelungslogik — Referenz, Health-Monitor existiert noch nicht)
Ambient durchlaeuft (sobald der Health-Monitor existiert) dieselbe
Plausibilitaets- und Latch-Logik wie die vier VSA-Sonden (-127 °C = weg,
85.00 °C nur vor abgeschlossener Conversion gueltig, eingefroren ueber N
Zyklen, CRC-Retry) und bekommt Status Ok/Warn/Err statt fest NA. WARN
bedeutet bei Ambient etwas anderes als bei VSA: "Gehaeuse-Innenraum zu
heiss", ab `AMB_WARN_C`, mit Hysterese zurueck unter `AMB_WARN_OFF_C`
(siehe `lib/FanControl/ambientPanicActive()`, bereits implementiert und
getestet). Bei aktiver Panik gehen **alle vier** Luefter vorsorglich auf
100 % (nicht nur zwei — siehe "Regelmodell") — effektive Duty =
`max(Kennlinie, Panik)` je Kanal (`effectiveDuty()`), das gewinnt gegen
die normale Kennlinie. Ambient-Err wird gelatcht und alarmiert wie bei
VSA, hat aber keinen eigenen Luefter (keine Luefter-Fail-Safe) und
deaktiviert mangels gueltigem Wert die Panik-Uebersteuerung. Die
Verdrahtung in den echten Health-Monitor/Regelkreis folgt erst, wenn
diese Module gebaut werden.

### SelfDiag (`lib/SelfDiag/`, Komfort)
Formatiert freien Heap und Loop-Frequenz zu einer Zeile (z. B.
"Heap 142k 45Hz") fuer `DashboardData.selfLine`. Wie Logging: Komfort,
nicht im kritischen Regelpfad — faellt das Auslesen aus, darf das die
Regelung nicht beeinflussen.

### LED (`PIN_LED`, GPIO2) — dritter, display-unabhaengiger Meldekanal
Default: Heartbeat, getrieben vom selben Regelkreis-`phase` wie das
OLED-Lebenszeichen (`heartbeatLedOn()` in `lib/StatusLed/`, weiterhin
unveraendert) — funktioniert auch bei totem OLED. Fehlerfall
(gelatchter, nicht quittierter Fehler): unterscheidbares Doppelblink-
Muster (`LED_FAULT_ON_MS`/`LED_FAULT_GAP_MS`/`LED_FAULT_PAUSE_MS` in
`config.h`) statt ruhigem Heartbeat, synchron zum Alarmton erkennbar.

Die Musterlogik dafuer liegt in `lib/Led/LedPattern.{h,cpp}`, analog
zum Dashboard schmal und rein testbar:
- `LedMode` (`Heartbeat`/`Fault`), `selectLedMode(faultActive)` waehlt
  den Modus aus einem Bool (gelatchter, nicht quittierter Fehler --
  der Latch selbst wird hier nicht neu gebaut, nur der Zustand
  hereingereicht).
- `ledLevel(mode, phase, nowMs)` liefert den Pegel: im Heartbeat-Fall
  ausschliesslich aus `phase` (kein eigener Timer, genau wie beim
  OLED-Lebenszeichen), im Fault-Fall ausschliesslich aus `nowMs` (das
  Doppelblink-Zeitfenster braucht echte Millisekunden).
- `ILed` (`lib/Led/ILed.h`) ist eine schmale Schnittstelle analog zu
  `IDisplay` -- unabhaengig von der groeberen `Hal::setHeartbeatLed()`.
  Reale Implementierung `Esp32Led` in `src/main.cpp`, Fake-Test-Double
  `FakeLed` protokolliert `set()`-Aufrufe mit Zeitstempel.

Noch NICHT in `setup()/loop()` verdrahtet: dafuer fehlt der reale
Fault-Latch-Zustand, den erst der spaetere Health-Monitor liefert
(siehe "Fehler-Latch und Alarm"). Grenze wie beim Speaker: die Tests
pruefen nur die Musterlogik, nicht ob die LED physisch leuchtet --
das bleibt Sichtpruefung am Board.

LED und Speaker sind beide in Software nicht verifizierbar (fire-and-
forget), signalisieren aber bewusst Unterschiedliches (Muster vs.
Ton) und ergaenzen sich, statt sich zu ersetzen. GPIO2 ist ein
Strapping-Pin: erst NACH dem Boot-Selbsttest als Ausgang konfigurieren
(`hal.setHeartbeatLed()` bzw. `Esp32Led::begin()`).

---

## Inbetriebnahme (stufenweise)

1. ESP32 + Speaker — Start-Piep **(erledigt)**
2. + OLED — Selbsttest-Anzeige, inkl. verifiziertem Fehlerpfad
   (fehlendes OLED -> Fehlerbeep) **(erledigt)**
3. + DS18B20 **auf dem Steckbrett** — Discovery, Adressen den 5 Rollen
   zuordnen (VSA1.1, VSA1.2, VSA2.1, VSA2.2, AMB), Sensoren **sofort
   physisch markieren** **(naechster Schritt)**
4. + Luefter — **einzeln nacheinander** anschliessen (1, dann 2, 3, 4),
   damit jeder Kanal eindeutig zugeordnet ist (siehe "Regelmodell": kein
   Zonen-Vergleich, PWM/TACHO sind ohnehin pro Luefter fest verdrahtet).
   Luefter und die beiden zusaetzlichen Sensoren sind aktuell in
   Beschaffung.
5. Erst dann: Sensoren auf die Karte, Halter drucken, Einbau
