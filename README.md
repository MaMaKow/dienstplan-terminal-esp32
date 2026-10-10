# Dienstplan-Terminal

## ESP-IDF einrichten und bauen

Das Projekt benötigt ESP-IDF. Der Wert von `IDF_PATH` ist der Ordner, in dem
ESP-IDF installiert ist. In der aktuellen Entwicklungsumgebung ist das:

```bash
/home/martin/esp/esp-idf
```

Wenn der Pfad auf einem anderen Rechner oder nach einer anderen Installation
abweicht, suche nach `tools/idf.py`. Zum Beispiel:

```bash
find "$HOME" -type f -path '*/tools/idf.py' -print 2>/dev/null
```

Nimm den übergeordneten ESP-IDF-Ordner, nicht den `tools`-Ordner und nicht die
Datei `idf.py`. Für dieses Projekt wird ESP-IDF 5.5.2 verwendet. Die Werkzeuge
und die passende Python-Umgebung installierst du einmalig so:

```bash
cd /home/martin/esp/esp-idf
./install.sh esp32
```

In jedem neuen Terminal aktivierst du ESP-IDF und baust das Projekt:

```bash
cd /home/martin/esp/esp-idf
source ./export.sh
cd /home/martin/Dokumente/git/dienstplan-terminal-esp32
idf.py --version
idf.py build
```

`source ./export.sh` setzt `IDF_PATH` und richtet `PATH` sowie die Python-
Umgebung für das aktuelle Terminal ein. Der erfolgreiche Build erzeugt
`build/epaper_hello.bin` und `build/epaper_hello.elf`.

## Vorschau und Tests ohne Hardware

ESP-IDF bietet QEMU für ausgewählte ESP32-Funktionen und einen virtuellen
Framebuffer. Dieser virtuelle Bildschirm ist kein Modell des hier verwendeten
SPI-E-Paper-Displays. Der bestehende Display-Treiber und die WLAN-Anbindung
können damit daher nicht als vollständiges Gerät getestet werden. Siehe
[Espressifs QEMU-Dokumentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/tools/qemu.html).

Der Framebuffer sowie Pixel-, Rechteck- und Textfunktionen liegen im
hardwareunabhängigen Modul `main/display_framebuffer.c`. Ein PC-Vorschauprogramm
kann dieses Modul direkt verwenden und den Puffer als PGM-Bild speichern. So
lassen sich Layout, Schrift und Koordinaten ohne ESP32 prüfen; SPI, WLAN, NTP
und das echte E-Paper bleiben Hardwaretests.
