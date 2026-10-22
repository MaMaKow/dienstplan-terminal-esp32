# Entscheidungstests: Deep Sleep vs. Modem-Sleep vs. Auto Light-Sleep

Diese Tests in Etappe 6 ermöglichen einen vergleichbaren Praxisentscheid zwischen den drei Schlafstrategien. Sie prüfen neben dem Strombedarf auch Kartenreaktion, WLAN- und Serverzuverlässigkeit sowie das Verhalten nach Aussetzern. Die bestehenden Abnahmegrenzwerte von **X mA** für den Standby-Strom und **Y ms** für die Aufwachlatenz bleiben bis zur gemeinsamen Festlegung Platzhalter.

Für faire Vergleiche dieselbe Firmware-Version, denselben Access Point, denselben Server, dieselbe Karte und dasselbe Nutzungsprofil verwenden. Pro Strategie den ESP32 mit identischer Peripherie betreiben: SSD1680-E-Paper, MFRC522, Piezo an GPIO25 und Taster an GPIO33; LEDs und weitere Peripherie im dokumentierten Betriebszustand belassen. Strom möglichst am Akku-/Versorgungseingang über einen Shunt mit Stromlogger oder ein geeignetes USB-Strommessgerät erfassen. Messpunkt, Versorgungsspannung und angeschlossene Peripherie bei jedem Lauf dokumentieren. Einen GPIO als Oszilloskop-Trigger beim Aufwachen beziehungsweise beim Beginn des Scanvorgangs verwenden.

### Test 1 – Grundverbrauch und Energie pro Scan

- **Ziel / Frage:** Welche Strategie benötigt im realistischen Tagesprofil am wenigsten Energie pro Scan, und hält sie den Standby-Grenzwert **X mA** ein?
- **Aufbau und Messpunkt:** Strommessung am Versorgungseingang; alle oben genannten Peripheriegeräte angeschlossen. E-Paper zunächst mit unverändertem Bild, MFRC522 entsprechend der Strategie im Shutdown, aktiv oder passend zum jeweiligen Schlafzustand.
- **Vorgehen:** Je Strategie mindestens **[Wiederholungszahl festlegen]** vollständige Zyklen aus Standby, Kartenlesung, WLAN-Buchung und Rückkehr in den Ruhezustand aufzeichnen. Zusätzlich einen repräsentativen Zeitraum mit etwa 200 Scans/Tag simulieren oder messen. Für Modem-Sleep und Auto Light-Sleep Poll-Intervall und Timeout variieren; DTIM-Intervall erfassen und gegebenenfalls variieren, soweit der AP dies zulässt.
- **Messgrößen:** Standby-Mittelstrom (mA), Spitzenstrom (mA), Energie pro Scan (mJ), Energie pro simuliertem Betriebstag (mWh) und Streuung zwischen Zyklen.
- **Akzeptanz / Vergleich:** Standby-Mittelstrom liegt unter **X mA**. Energie pro Scan und Tagesenergie werden zwischen Strategien verglichen; für den Einsatz muss die gewählte Strategie zugleich die Zuverlässigkeits- und Latenzkriterien der Folgetests erfüllen.
- **Abbruch:** Strategie als ungeeignet werten, wenn der Standby-Grenzwert nach **[Wiederholungszahl festlegen]** Messläufen wiederholt überschritten wird oder Scan-/Buchungszyklen nicht vollständig abgeschlossen werden.
- **Abhängigkeiten / Vorarbeiten:** Power-Path und Messpunkt gemäß [hardware/EINKAUFSLISTE.md](../hardware/EINKAUFSLISTE.md) festlegen. Vorläufige Messungen am Dev-Board sind nur Orientierungswerte und müssen mit dem finalen Power-Path-Konzept wiederholt werden.

### Test 2 – Aufwachlatenz, Kartenvorhaltedauer und verpasste Scans

- **Ziel / Frage:** Erkennt jede Strategie eine Karte schnell und zuverlässig, auch wenn sie nur kurz vor das Terminal gehalten wird?
- **Aufbau und Messpunkt:** Oszilloskop oder Logikanalysator am Wakeup-/Diagnose-GPIO und an den relevanten NFC-Signalen, soweit zugänglich; Strommessung am Versorgungseingang. E-Paper während des Tests mit einem festen Bild betreiben, damit Display-Aktualisierungen die Latenzmessung nicht verfälschen. Karte reproduzierbar an einer markierten Position vorhalten.
- **Vorgehen:** Für jede Strategie mindestens **[Wiederholungszahl festlegen]** Kartenannäherungen je Vorhaltedauer durchführen. Vorhaltedauer und – beim Timer-Wakeup – Timer-Intervall systematisch variieren. Zeiten vom Kartenannähern bis zur UID-Erkennung sowie bis zur UID-Anzeige erfassen. Bei Deep Sleep den IRQ-Pfad und den Timer-Fallback getrennt testen. Beim Timer-Wakeup kurze Vorhaltephasen während des Schlafintervalls einbeziehen: Sie können ohne IRQ bis zum nächsten Aufwachen verstrichen sein und dadurch zu verpassten Karten führen.
- **Messgrößen:** Latenz Karte→UID und Karte→Anzeige (ms), Erkennungsrate (%), verpasste Scans (%), Timer-Intervall (ms) und Kartenvorhaltedauer (ms).
- **Akzeptanz / Vergleich:** Aufwachlatenz bis zur Anzeige liegt unter **Y ms**; Erkennungsrate und Zahl verpasster Scans erfüllen **[Grenzwert festlegen]**. Ergebnisse getrennt nach Vorhaltedauer und Wakeup-Pfad ausweisen.
- **Abbruch:** Strategie als ungeeignet werten, wenn wiederholt Karten nicht erkannt werden oder die Latenz den Grenzwert **Y ms** überschreitet.
- **Abhängigkeiten / Vorarbeiten:** Der MFRC522-IRQ ist derzeit nicht verdrahtet. IRQ-Wakeup kann daher erst nach Verdrahtung und Dokumentation des IRQ-Pfads gemäß [hardware/wiring/ERSTAUFBAU.md](../hardware/wiring/ERSTAUFBAU.md) bewertet werden. Für den Deep-Sleep-Timer-Test ist kein IRQ erforderlich.

### Test 3 – WLAN-Wiederverbindung und Server-Erreichbarkeit

- **Ziel / Frage:** Kann eine Buchung zuverlässig und rechtzeitig an den Pflicht-Server übertragen werden, insbesondere nach WLAN-Schlaf oder kurzen Aussetzern?
- **Aufbau und Messpunkt:** ESP32, MFRC522 und Versorgungseingang wie in Test 1 messen; Access Point und Dienstplan-Server protokollieren. RSSI, BSSID, Kanal und DTIM-Intervall des AP erfassen. Deep Sleep mit gespeicherten AP-Daten (IP, BSSID, Kanal im RTC-Speicher) testen; Modem-Sleep und Auto Light-Sleep mit bestehender Assoziation.
- **Vorgehen:** Pro Strategie mindestens **[Wiederholungszahl festlegen]** Buchungen ausführen. Beim Deep Sleep gespeicherte AP-Daten verwenden und Zeit vom Wakeup bis zur erneuten Server-Erreichbarkeit messen. Bei allen Strategien Buchungen während stabiler Verbindung sowie nach kontrollierten WLAN-Aussetzern unterschiedlicher Dauer testen. Für Modem-Sleep das DTIM-Verhalten, für Auto Light-Sleep Poll-Intervall und Timeout variieren. Da keine lokale Warteschlange vorhanden ist, einen fehlgeschlagenen Buchungsversuch als Fehler erfassen und prüfen, ob die Firmware einen erneuten Versuch zulässt.
- **Messgrößen:** Reconnect-Zeit (ms), Zeit bis Server-Erreichbarkeit (ms), Buchungserfolgsrate (%), Verbindungsabbrüche pro **[Testdauer festlegen]**, Wiederholungszahl sowie Strom und Energie pro Buchung (mA, mJ).
- **Akzeptanz / Vergleich:** Buchungserfolgsrate erfüllt **[Grenzwert festlegen]**; Reconnect- und Server-Erreichbarkeitszeit bleiben unter **[Grenzwert festlegen]**. Festhalten, ob der Server ohne Reconnect erreichbar bleibt und wie sich jede Strategie nach einem Aussetzer erholt.
- **Abbruch:** Strategie als ungeeignet werten, wenn wiederholt Buchungen verloren gehen oder die Verbindung nach einem Aussetzer nicht innerhalb des festgelegten Grenzwerts wiederhergestellt wird.
- **Abhängigkeiten / Vorarbeiten:** AP- und Server-Logging verfügbar machen; DTIM-Intervall und Netzwerkkonfiguration dokumentieren. Deep-Sleep-Wiederverbindung mit RTC-gespeicherten AP-Daten und der Zielzeit von 300–800 ms prüfen, ohne diese Zeit als bereits belegten Messwert anzunehmen.

### Test 4 – Schlaf-/Aufwachzyklen und Langzeitverhalten

- **Ziel / Frage:** Bleibt die Karten- und WLAN-Funktion über wiederholte Schlaf-/Aufwachzyklen stabil?
- **Aufbau und Messpunkt:** Vollständiger Aufbau mit E-Paper, MFRC522, Piezo, Taster und Strommessung am Versorgungseingang. Firmware-Logs für Wakeup-Grund, NFC-Erkennung, WLAN-Zustand und Buchungsergebnis aktivieren.
- **Vorgehen:** Je Strategie mindestens **[Wiederholungszahl festlegen]** Schlaf-/Aufwachzyklen oder **[Testdauer festlegen]** kontinuierlichen Betrieb durchführen. In festgelegten Abständen Scans und Buchungen auslösen; WLAN-Aussetzer kontrolliert einstreuen. Deep Sleep mit MFRC522-`CommandReg = 0x03` testen und nach dem Aufwachen die Antenneninitialisierung prüfen.
- **Messgrößen:** Erfolgreiche Zyklen (%), verlorene Karten und Buchungen (Anzahl, %), Aufwach- und Reconnect-Zeit (ms), Fehler/Neustarts (Anzahl), Standby-Strom (mA) und Energie je Zyklus (mJ).
- **Akzeptanz / Vergleich:** Keine wiederholten NFC- oder Buchungsfehler; Aufwachlatenz und Standby-Strom erfüllen **Y ms** beziehungsweise **X mA**. Fallback-Verhalten und Fehlerursachen nachvollziehbar protokollieren.
- **Abbruch:** Strategie als ungeeignet werten, wenn wiederholte Scans oder Buchungen verloren gehen, Neustarts auftreten oder ein festgelegter Latenz-/Stromgrenzwert überschritten wird.
- **Abhängigkeiten / Vorarbeiten:** Fehler- und Wakeup-Logging ergänzen. Für einen belastbaren Laufzeittest Power-Path, Akkuanschluss und Messpunkt finalisieren; die Panasonic NCR18650B mit einem definierten Tagesprofil prüfen.

### Entscheidungsmatrix

| Kriterium | Deep Sleep mit gespeicherten AP-Daten | Modem-Sleep | Auto Light-Sleep |
|---|---|---|---|
| Energie pro Scan (mJ) | [zu messen] | [zu messen] | [zu messen] |
| Standby-Mittelstrom (mA) / Grenzwert X | [zu messen] | [zu messen] | [zu messen] |
| Aufwachlatenz Karte→UID/Anzeige (ms) / Grenzwert Y | [zu messen] | [zu messen] | [zu messen] |
| WLAN-Zuverlässigkeit / Buchungserfolgsrate (%) | [zu messen] | [zu messen] | [zu messen] |
| Server-Erreichbarkeit ohne Reconnect | [zu messen] | [zu messen] | [zu messen] |
| Verhalten bei WLAN-Aussetzern / Erholungszeit (ms) | [zu messen] | [zu messen] | [zu messen] |
| Verpasste Scans nach Kartenvorhaltedauer (ms) | [zu messen] | [zu messen] | [zu messen] |
| Komplexität / nötige Änderungen | [zu bewerten] | [zu bewerten] | [zu bewerten] |
| Eignung bei ca. 200 Scans/Tag | [zu bewerten] | [zu bewerten] | [zu bewerten] |
| Gesamtentscheidung | [nach Tests ausfüllen] | [nach Tests ausfüllen] | [nach Tests ausfüllen] |

Die Tests verhindern, dass eine Strategie allein aufgrund ihres Schlafstroms gewählt wird, obwohl sie Karten verpasst, die Serververbindung zu langsam wiederherstellt oder Buchungen verliert. Frühe Messungen am Dev-Board liefern nur Orientierungswerte; sämtliche relevanten Messungen sind mit dem finalen Power-Path-Konzept aus [hardware/EINKAUFSLISTE.md](../hardware/EINKAUFSLISTE.md) zu wiederholen.
