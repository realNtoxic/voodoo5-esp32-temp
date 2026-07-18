# ESP32 Thermal-Monitor — 3dfx Voodoo 5 5500

Firmware zur Temperaturüberwachung und Lüfterregelung einer 3dfx Voodoo 5 5500.
Architektur, Hardware-Pinbelegung und Funktionsübersicht stehen in
[`CLAUDE.md`](CLAUDE.md) — hier geht es nur um die Einrichtung der
Toolchain auf **CachyOS**, das Ausführen der Tests und das Erzeugen einer
Flash-Datei.

---

## 1. Voraussetzungen

- CachyOS (oder jede andere Arch-basierte Distribution)
- Python 3 (bei CachyOS bereits vorinstalliert)
- ESP32 DevKit per USB angeschlossen (nur zum Flashen/Monitoring nötig,
  nicht für `pio test -e native`)

Gebaut wird mit **PlatformIO Core** (CLI). Eine grafische Oberfläche ist
nicht nötig, kann aber optional als VS-Code-Extension "PlatformIO IDE"
nachgerüstet werden — die CLI-Befehle unten funktionieren identisch.

---

## 2. PlatformIO auf CachyOS installieren

CachyOS' System-Python ist "externally managed" (PEP 668) — ein nacktes
`pip install platformio` schlägt daher mit `externally-managed-environment`
fehl. Empfohlen ist `pipx`, das PlatformIO in einer eigenen, isolierten
virtuellen Umgebung installiert:

```bash
sudo pacman -S python-pipx
pipx ensurepath
# Terminal neu starten oder: source ~/.bashrc (bzw. ~/.zshrc)

pipx install platformio
pio --version
```

Alternative über die AUR (z. B. mit `yay`):

```bash
yay -S platformio-core
```

### USB-Zugriff (udev-Regeln)

Ohne udev-Regeln landet der ESP32 unter einer Gruppe, auf die der eigene
Benutzer keinen Schreibzugriff hat, und `pio run -t upload` schlägt mit
`Permission denied` auf `/dev/ttyUSB0` (bzw. `/dev/ttyACM0`) fehl.

```bash
curl -fsSL \
  https://raw.githubusercontent.com/platformio/platformio-core/master/scripts/99-platformio-udev.rules \
  | sudo tee /etc/udev/rules.d/99-platformio-udev.rules
sudo udevadm control --reload-rules
sudo udevadm trigger
```

Danach den eigenen Benutzer zur Gruppe hinzufügen, der das serielle Gerät
gehört. Auf Arch/CachyOS ist das meist `uucp` (nicht `dialout` wie bei
Debian/Ubuntu) — vorher mit `ls -l /dev/ttyUSB0` (ESP32 einstecken, dann
prüfen) verifizieren:

```bash
ls -l /dev/ttyUSB0
sudo usermod -aG uucp "$USER"
```

**Danach ab- und wieder anmelden** (Gruppenmitgliedschaft wird erst bei
einer neuen Login-Session wirksam).

Beim ersten `pio run` bzw. `pio test` lädt PlatformIO zusätzlich die
benötigten Toolchains/Plattform-Pakete (`native` bzw. `espressif32`)
automatisch nach — dafür ist beim ersten Lauf Internetzugriff nötig
(mehrere hundert MB für die Xtensa-Toolchain).

---

## 3. Tests ausführen

Die Logik-Tests laufen host-nativ, **kein Board nötig**:

```bash
pio test -e native
```

Vor jedem Commit muss dieser Befehl grün sein (siehe `CLAUDE.md`).

---

## 4. Firmware bauen (Flash-Datei erzeugen)

```bash
pio run -e esp32
```

Die fertige Flash-Datei liegt danach unter:

```
.pio/build/esp32/firmware.bin
```

(zusätzlich `firmware.elf` mit Debug-Symbolen im selben Verzeichnis).

---

## 5. Flashen

ESP32 per USB anschließen, dann:

```bash
pio run -e esp32 -t upload
```

PlatformIO erkennt den seriellen Port normalerweise automatisch. Falls
mehrere serielle Geräte angeschlossen sind oder die Erkennung fehlschlägt,
Port explizit angeben:

```bash
pio device list                       # verfügbare Ports anzeigen
pio run -e esp32 -t upload --upload-port /dev/ttyUSB0
```

### Serielle Konsole

```bash
pio device monitor -b 115200
```

(Baudrate passend zu `Serial.begin(115200)` in `src/main.cpp`.)

---

## 6. Troubleshooting

| Problem | Ursache / Lösung |
|---|---|
| `Permission denied` auf `/dev/ttyUSB0` | udev-Regel fehlt oder Gruppenmitgliedschaft noch nicht aktiv → Schritt 2, danach neu anmelden |
| `externally-managed-environment` bei `pip install` | Nicht `pip` direkt nutzen, sondern `pipx install platformio` |
| Kein `/dev/ttyUSB0`/`/dev/ttyACM0` sichtbar | USB-Kabel prüfen (Daten, nicht nur Ladekabel), ggf. anderer USB-Port/-Treiber (CP210x vs. CH340) |
| Upload bricht mit Timeout ab | Beim Verbinden BOOT-Taster am DevKit gedrückt halten, bis der Upload startet |
| Erster `pio run`/`pio test` sehr langsam | Normal — Toolchain-Download beim ersten Mal, danach gecacht in `~/.platformio` |
