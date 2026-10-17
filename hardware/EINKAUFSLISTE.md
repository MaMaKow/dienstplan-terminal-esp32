# Einkaufsliste: Dienstplan-Terminal

**Stand:** 13.10.2026  
**Status:** Entwurf; vor Bestellung Power-Path und Bedienelemente final festlegen.  
**Bevorzugter Anbieter:** Conrad. Für ein passendes Lade-/Power-Path-Modul ist derzeit kein konkreter Conrad-Artikel verifiziert; ein möglicher Alternativanbieter ist ausdrücklich markiert.

Die Liste unterscheidet bereits vorhandene Teile von noch benötigten Teilen. Preise und Lagerbestand sind nicht festgeschrieben und müssen vor einer Bestellung erneut geprüft werden.

## Bereits vorhanden – nicht erneut kaufen

| Bauteil | Stand / Hinweis |
|---|---|
| ESP32-DevKitC | Im Projekt bzw. Schaltplan vorgesehen. Exakte Boardversion vor Festlegung der Versorgung prüfen. |
| 2,9-Zoll-E-Paper mit SSD1680 | Vorhandener Displaytreiber im Projekt. |
| RF-IO-RC522 / MFRC522-Lesemodul | Vorhanden; für 3,3-V-Logik vorgesehen. |
| Panasonic NCR18650B, eine Zelle | Vorhanden. Die Zellaufschrift identifiziert den Typ; eine integrierte Schutzschaltung ist nicht erkennbar. |
| USB-C-TP4056-Ladeboard mit Schutz-Ausgang | Vorhanden. Die Platine hat `B+`, `B-`, `OUT+`, `OUT-`; Zellschutz ist vorhanden, ein Power-Path für gleichzeitigen Betrieb ist damit nicht nachgewiesen. |
| AMS1117-3.3-Reglermodule | Vorhanden, aber nicht für die Versorgung des Terminals aus der einzelnen Zelle vorgesehen. |
| Kingbright RGB-LED L-154A4SURKQBDZGC | Vorhanden; gemeinsame Kathode, Vorwiderstand je Farbkanal erforderlich. |

## Noch zu beschaffen oder festzulegen

| Menge | Bauteil / Mindestanforderung | Anbieter / Status |
|---:|---|---|
| 1 | **Lade- und Power-Path-Modul für eine Li-Ion-Zelle:** 4,2-V-Ladeprofil, USB-Eingang, Betrieb der Last während des Ladens ohne Beeinträchtigung der Ladeabschaltung, dokumentierter Last-Ausgang. Für den endgültigen Aufbau muss die nachgeschaltete Versorgung eine stabile 3,3-V-Schiene mit mindestens 1 A Dauerstrom und ausreichender Reserve für ESP32-Stromspitzen liefern. | Conrad zuerst prüfen; derzeit kein geeignetes konkretes Modul bestätigt. Kein TP4056-Board ohne dokumentierten Power-Path als Ersatz einplanen. |
| 1 | **3,3-V-Buck-Boost-Wandler**, falls der Power-Path-Ausgang nicht selbst geregelt 3,3 V liefert: Eingang passend zum Einzellen-Akku-/SYS-Bereich, 3,3 V Ausgang, mindestens 1 A Dauerstrom; geringe Ruhestromaufnahme bevorzugt. | Conrad zuerst prüfen; konkrete Bestellnummer offen. Der vorhandene AMS1117 und ein reiner Abwärtswandler wie MP1584 erfüllen diese Anforderung nicht. |
| 1 | **1x-18650-Halter mit Kabeln**, passend auch für die Länge des konkreten Akkus. Conrad führt den TRU COMPONENTS TC-13220688 als 1x-18650-Halter mit Kabeln. | [Conrad-Batteriehalter-Sortiment](https://www.conrad.de/de/o/batteriehalter-0501190.html) – Artikel TC-13220688 ist dort auffindbar. |
| 1 | **USB-Netzteil für den Ladebetrieb**, 5 V, mindestens 2 A, mit passendem Kabel zum gewählten Lade-/Power-Path-Modul. | Conrad; erst nach Auswahl des Moduls bestellen. Ein 5-V/1-A-Eingang kann bei gleichzeitiger Last den Ladestrom begrenzen. |
| 4 | **Momentane Taster** für Dienstbeginn, Dienstende, Pausenbeginn und Pausenende. Mechanische Bauform, Einbaudurchmesser und Beleuchtung müssen zum Gehäuse passen. | Conrad bevorzugt; Artikel noch offen. Bei integrierter LED deren Spannung und Anschluss vor Kauf prüfen. Alternative: Taster plus je eine separate Anzeige-LED. |
| 4 | **LED-Anzeigen für die vier Tasten**, falls nicht in den Tastern enthalten; getrennt steuerbar, mit geeigneter Treiberstufe und individuellem Vorwiderstand. | Conrad bevorzugt; konkrete LEDs und Widerstandswerte nach Festlegung der Versorgung und Taster. |
| 1 | **Aktiver Piezo-Signalgeber**, für Versorgung und Lautstärke passend; Ansteuerung über eine geeignete Treiberstufe, falls der GPIO-Strom nicht genügt. Conrad listet den TRU COMPONENTS TC-9202108 als 3-V/DC-Dauerton-Kandidat; Datenblatt und Eignung an 3,3 V vor Bestellung prüfen. | [Conrad-Signalgeber-Sortiment](https://www.conrad.de/de/o/schallwandler-signalgeber-1312030.html?tfo_productType=Piezo-Signalgeber) – Kandidat TC-9202108. |
| nach Schaltung | Treibertransistoren bzw. MOSFETs, Widerstände und Entkopplungskondensatoren für Tastenbeleuchtung, RGB-LED, Piezo und Versorgung. | Conrad; konkrete Typen und Werte nach Schaltungsentwurf. |
| nach Gehäuse | Montage-/Verdrahtungsmaterial: passende Leitungen, Steckverbinder, Befestigung, Gehäuse und Zugentlastung für Akku und USB-C. | Conrad bevorzugt; Abmessungen und Stecksystem noch offen. |

## Geprüfte Kandidaten für Laden während des Betriebs

### Adafruit Universal USB/DC/Solar Charger (BQ24074) – guter Lade-/Power-Path-Kandidat, Regler fehlt noch

[BerryBase-Artikel ADA4755](https://www.berrybase.de/en/adafruit-universal-usb-dc-solar-lithium-ion-polymer-charger) basiert laut [Adafruit-Dokumentation zum BQ24074](https://learn.adafruit.com/adafruit-bq24074-universal-usb-dc-solar-charger-breakout) auf einem Lade-IC mit Load-Sharing: Bei angeschlossenem USB versorgt der Eingang zuerst die Last; die Batterie wird mit dem verbleibenden Strom geladen. Der LOAD-Ausgang ist nicht auf 3,3 V geregelt, sondern liegt laut Adafruit je nach Zustand ungefähr zwischen Batteriespannung und maximal 4,4 V. Für unser Gerät braucht es daher zusätzlich einen geeigneten 3,3-V-Buck-Boost-Regler. Lade-/Power-Path-Funktion ist außerdem nicht gleichbedeutend mit Zellschutz; für die vorhandene ungeschützte NCR18650B muss der Schutz separat geklärt werden. Adafruit spezifiziert bis zu 1,5 A für die Last. BerryBase zeigt den Artikel derzeit als nicht verfügbar. **Technisch der passendste der drei neu geprüften Kandidaten**, aber noch keine finale Auswahl: Verfügbarkeit, Zellschutz, nötige 3,3-V-Regelung und Stromspitzen des konkreten ESP32-Aufbaus müssen geklärt werden.

### Adafruit Pro Trinket LiPoly/LiIon Backpack (2124) – nicht passend für dieses Terminal

[Reichelt-Artikel DEBO POWER3 / Adafruit 2124](https://www.reichelt.de/de/de/shop/produkt/entwicklerboards_-_pro_trinket_fuer_li-ion_lipo_akkus_mcp73831-235494) ist ein Lade- und Umschalt-Zusatz für das Pro-Trinket-Board, kein eigenständiges Netzteil für einen ESP32. Adafruit nennt 100 mA Ladestrom ab Werk, per Lötjumper maximal 500 mA, und beschreibt die Verwendung mit dem 3,3-V-Pro-Trinket. Die Reichelt-Angabe „regelt ... auf 3,3 V herunter“ sollte nicht als garantierter, belastbarer 3,3-V-Ausgang für ein ESP32-DevKit verstanden werden. Wegen der geringen Ladeleistung und fehlender ausgewiesener Leistungsreserve für unser Terminal nicht einplanen.

### Soldered SOL-333014 – Ladegerät mit Zellschutz, aber Power-Path nicht belegt

[BerryBase-Artikel SOL-333014](https://www.berrybase.de/soldered-lithium-ionen-akku-ladeplatine-mit-schutz-usb-type-c-cc-cv-1-a-jst-2-leds) gibt CC/CV-Ladung, 1 A maximalen Ausgangsstrom sowie Kurzschluss-, Über- und Unterspannungsschutz an. Die Produktangaben weisen keinen separaten LOAD-/SYS-Ausgang und kein Load-Sharing aus. Die beworbene Schutzfunktion ersetzt zudem keinen Nachweis, dass das Terminal während des Ladens korrekt versorgt wird. Daher nicht als Lösung für gleichzeitigen Betrieb und Laden einplanen, bis das Hersteller-Schaltbild/Datenblatt diese Funktion eindeutig bestätigt.

### Adafruit PowerBoost 1000C – alternative Architektur, noch nicht ausgewählt

[Adafruit PowerBoost 1000C](https://www.adafruit.com/product/2465) kombiniert Einzellen-Lader, Load-Sharing und einen 5,2-V-Ausgang bis 1 A. Der Hersteller nennt Micro-USB als Ladeeingang und empfiehlt für Betrieb plus Laden eine 5-V-Quelle mit 2 A. Das Modul ist **nur ein Prüfkandidat**, keine freigegebene Auswahl: Vor Bestellung müssen die 5,2-V-Kompatibilität mit der konkreten DevKit-Version, die Leistungsreserve und der notwendige Schutz der vorhandenen Zelle geklärt werden. Es ist kein Conrad-Artikel.

## Geprüft, derzeit nicht empfohlen

[BerryBase CHB-214](https://www.berrybase.de/4in1-lade-entlade-schutzschaltungs-spannungswandler-modul-fuer-3-7v-lithium-akkus-5v-2a): Das Modul wäre von den beworbenen Funktionen her interessant (Laden, Zellschutz und 5-V-Ausgang). BerryBase zeigt jedoch **0,5/5 aus zwei Bewertungen**; beide berichten von nicht oder nur kurz funktionierenden Modulen. Das ist eine sehr kleine Stichprobe, aber ein starkes Warnsignal. Außerdem ist der beworbene 5-V-Ausgang kein direkter Ersatz für eine geregelte 3,3-V-Schiene. Daher nicht für den Aufbau einplanen, solange kein unabhängiger Funktionstest und keine passende 3,3-V-Versorgung nachgewiesen sind.

## Versorgungsziel

Für den späteren Aufbau ist folgende Funktion gefordert:

```text
USB-C-Netzteil
      │
      ▼
1S-Ladegerät mit Power-Path ───── 18650-Zelle mit Schutz
      │
      ▼
geregelte 3,3-V-Schiene mit ausreichender Stromreserve
      │
      ├── ESP32-DevKitC
      ├── MFRC522
      ├── E-Paper
      └── Tasten, LEDs und Piezo
```

Das konkrete Power-Path-Modul kann eine eigene 3,3-V-Regelung enthalten. Falls nicht, wird ein separater Buck-Boost benötigt. Es darf nicht gleichzeitig über USB und über einen externen Regler in den 3,3-V-Pin des DevKit eingespeist werden; Espressif verlangt für das DevKit jeweils nur eine der vorgesehenen Versorgungsarten.

## Vor Bestellung noch zu prüfen

1. Exakte Version des ESP32-DevKitC sowie maximaler Strombedarf des vollständigen Aufbaus.
2. Ob die vorhandene Zelle tatsächlich ungeschützt ist und welche Schutzfunktion das ausgewählte Power-Path-Modul abdeckt.
3. Ladeeingang, Ladestrom und Dauer-/Spitzenstrom des Power-Path-Moduls.
4. Einbaumaße und Beleuchtungsanschlüsse der vier Taster.
5. Gehäusemaße, Leitungslängen und passende Steckverbinder.
