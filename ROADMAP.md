# Roadmap: Dienstplan-Terminal

## Ziel

Ein an der Apotheke montiertes ESP32-Terminal ermöglicht Mitarbeitenden, Dienstbeginn, Dienstende, Pausenbeginn und Pausenende zu buchen. Eine NFC-Karte identifiziert den Mitarbeitenden. Der Server der Dienstplan-Anwendung ist die maßgebliche Quelle für Mitarbeiterdaten, Buchungszustand, erlaubte Aktionen und den Erfolg einer Buchung.

Das E-Paper zeigt im Ruhezustand Datum und Uhrzeit. Während einer Buchung zeigt es den Namen und eine eindeutige Rückmeldung. Beleuchtete Tasten, RGB-LED und Piezo-Buzzer unterstützen die Bedienung.

Es gibt zunächst ein Terminal. Die Architektur soll weitere Terminals ermöglichen, ohne sie für den ersten Einsatz vorauszusetzen. Es gibt keinen festen Liefertermin; umgesetzt wird in nachvollziehbaren, hobbygerechten Etappen.

## Festgelegter Bedienablauf

1. Im Ruhezustand zeigt das Gerät Datum und Uhrzeit.
2. Ein Mitarbeiter hält seine NFC-Karte vor das Terminal.
3. Das Terminal fragt über die REST-API Mitarbeiter und aktuellen Buchungszustand ab.
4. Bei einer bekannten Karte begrüßt das Gerät den Mitarbeiter mit Namen und beleuchtet nur die vom Server erlaubten Aktionen:
   - Abwesend: Dienstbeginn
   - Im Dienst: Dienstende oder Pausenbeginn
   - In der Pause: Pausenende
5. Der Mitarbeiter wählt eine beleuchtete Taste. Für jede Buchung ist ein erneutes Vorhalten der Karte erforderlich.
6. Nach der Buchungsanfrage zeigt das Gerät den vom Server gelieferten Erfolg oder Fehler verständlich an. Ein erneuter Versuch beginnt mit einem neuen Kartenscan.
7. Erfolgt innerhalb von zehn Sekunden nach dem Kartenscan keine Tastenaktion, kehrt das Gerät in den Ruhezustand zurück.

Unbekannte Karten werden abgelehnt. Bei nicht verfügbarem WLAN oder Server wird keine Buchung angenommen; der Mitarbeiter erhält eine klare Fehlermeldung. Es gibt keine lokale Warteschlange für Buchungen.

## Einrichtungs- und Betriebsentscheidungen

- Ein noch nicht konfiguriertes Gerät startet einen offenen SoftAP zur Einrichtung. In diesem Zustand liegen noch keine geheimen Zugangsdaten auf dem Gerät.
- Bei der Einrichtung wird die Filiale des Terminals ausgewählt.
- Sind bereits Zugangsdaten gespeichert, wird der SoftAP erst durch eine Taste aktiviert. Das Display weist auf diese Taste hin. Ein Wechsel zum SoftAP nach 24 h Offline-Zeit wird vorgeschlagen und muss per Taste aktiviert werden.
- Die Einrichtung erfasst mindestens WLAN-Zugangsdaten, API-Adresse, Filiale und die vom Server ausgestellte Geräteidentität bzw. Gerätezugangsdaten.
- Jedes Terminal authentifiziert sich mit eigenen Zugangsdaten am Server. Mitarbeiterkonten werden nicht für die Geräteanmeldung verwendet.
- NFC wird zunächst ohne kryptografische Kartenauthentifizierung verwendet. Die Kartenkennung dient zur Identifikation und Zuordnung.
- Eine Smartphone-App übernimmt die Zuordnung von NFC-Karten zu Mitarbeitenden unabhängig vom Terminal. Unbekannte Karten werden am Terminal nicht registriert.
- OTA-Updates sind gewünscht.

## Architekturleitlinien

- Der Server verwaltet alle Zustandsregeln. Das Terminal zeigt die vom Server zurückgegebenen erlaubten Aktionen an und verlässt sich nicht auf eine dauerhaft lokal gepflegte Kopie der Regeln.
- Der Server validiert eine Aktion erneut, wenn die Buchung abgeschickt wird. Eine zuvor erlaubte Taste garantiert noch keinen Buchungserfolg.
- API-Aufrufe erfolgen über HTTPS mit Zertifikatsprüfung. Für Geräteauthentifizierung wird eine pro Gerät widerrufbare Identität benötigt. OAuth 2.0 Client Credentials mit kurzlebigen Tokens ist eine geeignete Standardoption; Authentifizierungsverfahren, Token-Laufzeit und Provisionierung müssen mit der Server-API abgestimmt werden.
- Der Gerätezugang erhält nur die Rechte, die für Filialbindung, Mitarbeiter-/Kartenauflösung und Buchungen nötig sind.
- Die Firmware wird in Module gegliedert: Anwendungsablauf/Zustandsautomat, NFC, Tasten, LED/Buzzer, E-Paper/Framebuffer, WLAN und SoftAP, Konfiguration, API-Client, Zeitverwaltung und OTA.
- Die REST-Schnittstelle liefert maschinenlesbare Fehlercodes und kurze, für Mitarbeitende verständliche Meldungen. Technische Details gehören in das Geräteprotokoll, nicht auf die Anzeige.
- Buchungsanfragen sollten eine eindeutige Vorgangskennung unterstützen, damit Wiederholungen nach Verbindungsabbrüchen keine Doppelbuchung verursachen.

## Etappen

### 1. API-Verträge und App-Zuordnung festlegen

Die vorhandene API v0.3.0 enthält bereits Benutzer-, Mitarbeiter-, Dienstplan- und Filialendpunkte. Für den Terminalablauf sind weitere Verträge nötig.

Zu definieren und serverseitig bereitzustellen:

- Geräteanlage, Geräteauthentifizierung, Widerruf und Zuordnung zu einer Filiale
- Auflösung einer Kartenkennung zu einem Mitarbeiter samt aktuellem Zustand und erlaubten Aktionen
- Buchungsanfrage mit Filiale/Gerät, Aktion und Vorgangskennung
- Antwort mit eindeutigem Ergebnis und verständlicher Meldung
- Fehlerfälle für unbekannte/gesperrte Karte, unzulässige Aktion, fehlende Berechtigung, WLAN-/Serverfehler und abgelaufene Sitzung
- Kartenregistrierung und -widerruf durch die Smartphone-App; klären, welche Endpunkte die App dafür benötigt

**Abnahmekriterium:** App, API und Firmware können anhand dokumentierter Anfrage-/Antwortbeispiele unabhängig entwickelt werden. Der Server ist eindeutig als Quelle der erlaubten Aktionen und Buchungsentscheidung festgelegt.

### 2. Firmware-Grundstruktur und Buchungsablauf

- Bestehenden Ablauf in die vereinbarten Module aufteilen.
- Zustandsautomaten für Ruhezustand, Kartenabfrage, Mitarbeiterrückmeldung, Aktionsauswahl, Buchung, Ergebnis und Timeout einführen.
- API-Client mit Zeitlimits, TLS-Zertifikatsprüfung, JSON-Verarbeitung und klarer Trennung von Transport- und fachlichen Fehlern aufbauen.
- Den Zehn-Sekunden-Timeout nach erfolgreicher Kartenauflösung umsetzen.
- Die Anzeige von Datum/Uhrzeit im Ruhezustand und Mitarbeitername/Aktionsstatus im Buchungsablauf gestalten.

**Abnahmekriterium:** Der Ablauf lässt sich mit simulierten API-Antworten nachvollziehen; unzulässige Aktionen werden nicht angeboten und Fehler führen nicht zu einer falschen Erfolgsmeldung.

### 3. NFC, Tasten und Rückmeldung integrieren

- NFC-Leser und Kartenerkennung einbinden.
- Vier Tasten für Dienstbeginn, Dienstende, Pausenbeginn und Pausenende einbinden.
- Tastenbeleuchtung aus der Aktionsliste des Servers ableiten; nicht erlaubte Tasten bleiben aus.
- RGB-LED und Piezo für mindestens Erfolg, fachliche Ablehnung und technische Störung verwenden.
- Entprellung, wiederholte NFC-Erkennung und Abbruch/Timeout berücksichtigen.

**Abnahmekriterium:** Eine bekannte Karte zeigt Name und ausschließlich zulässige Tasten; eine unbekannte Karte wird abgelehnt; jede Buchung verlangt einen neuen Scan und liefert eine eindeutige Rückmeldung.

### 4. Geräteeinrichtung und WLAN-Wiederherstellung

- SoftAP-Konfigurationsseite für WLAN, API-Adresse, Filiale und Gerätezugangsdaten erstellen.
- Unkonfigurierten Start in den Einrichtungsmodus führen.
- Bei gespeicherter Konfiguration den SoftAP nach Verlust des WLAN nur nach Tastendruck öffnen und den Tastenschritt auf dem Display erklären.
- Konfiguration persistent und sicherheitsbewusst speichern; Zugangsdaten nicht protokollieren oder anzeigen.
- WLAN-Wiederverbindung und verständliche Offline-Anzeige umsetzen. Offline-Buchungen werden abgelehnt.

**Abnahmekriterium:** Ein Gerät kann erstmals eingerichtet, einer Filiale zugeordnet und nach einer WLAN-Änderung erneut konfiguriert werden. Ohne Netzwerkverbindung wird keine Buchung als erfolgreich dargestellt.

### 5. OTA und Wartbarkeit

- OTA-Updateweg mit Prüfung des Update-Ergebnisses und Wiederherstellungsverhalten integrieren.
- Gerätestatus und Diagnoseprotokolle für WLAN, API, NFC und Display ergänzen, ohne Karten- oder Zugangsdaten offenzulegen.
- Firmwareversion bei der Gerätekommunikation bereitstellen.

**Abnahmekriterium:** Ein Firmwareupdate kann über den vorgesehenen Updateweg eingespielt werden; ein fehlgeschlagenes Update lässt das Gerät wieder startfähig.

### 6. Erprobung im Apothekenbetrieb und Mehrgerätefähigkeit

- Hardwareablauf mit echten Karten, WLAN-Ausfall, API-Fehlern und typischen Buchungsfolgen erproben.
- Feedback, Lesbarkeit und Bedienzeit im realen Montageort beurteilen.
- Gerätekennung und Filialbindung so verwenden, dass ein zweites Terminal ohne Sonderlogik ergänzt werden kann.
- Erst nach erfolgreichem Einzelgerätebetrieb entscheiden, ob Mehrgerätebetrieb zusätzliche API- oder Verwaltungsfunktionen benötigt.

**Abnahmekriterium:** Die vier Buchungsarten funktionieren im vorgesehenen Arbeitsablauf, Fehler sind für Mitarbeitende verständlich und das Gerät kehrt zuverlässig zur Uhranzeige zurück.

## Noch zu klären

- Welcher NFC-Leser und welcher Kartentyp werden eingesetzt, und welches Interface verwendet der Leser?
- Welche vier beleuchteten Tasten und welche RGB-LED-/Piezo-Hardware werden konkret verwendet?
- Wie registriert die Smartphone-App eine Karte: über einen Online-Scan mit dem Smartphone oder über ein anderes Verfahren? Das Terminal ist nicht am Zuordnungsablauf beteiligt.
- Unterstützt die API OAuth 2.0 bereits, oder wird ein schlanker Geräteauthentifizierungs-Endpunkt benötigt? Wie werden Gerätezugangsdaten erstellt, erneuert und widerrufen?
- Welche konkrete Antwort soll die API bei der Kartenauflösung liefern, insbesondere Mitarbeitername, aktueller Zustand und Aktionsliste?
- Welche Displaydauer und genauen LED-/Piezo-Muster gelten für Erfolg, Ablehnung und technische Fehler?
- Welche HTTPS-Zertifikatsstrategie und OTA-Quelle werden verwendet?
- Soll das Gerät nach einem Stromausfall den letzten Betriebszustand nur aus dem Server neu laden? (Empfehlung: ja; keine Buchungszustände lokal als maßgeblich speichern.)

## Aktueller Projektstand

- Vorhanden sind eine ESP-IDF-Anwendung für ESP32, WLAN-Verbindung, NTP-Zeitabgleich, E-Paper-Treiber für ein 2,9-Zoll-SSD1680-Display, Framebuffer und einfache Zeit-/Datumsanzeige. Der aktuelle Anwendungscode enthält noch keinen NFC-Buchungsablauf, keine Tasten-/LED-/Buzzer-Ansteuerung, keine Dienstplan-API-Kommunikation, keine SoftAP-Konfiguration und keine OTA-Funktion.
- Weiterhin vorhanden ist folgende Hardware:
  - 1 Passiver Buzzer
  - große und kleine Taster
  - farbige LEDs und zwei RGB-LEDs
  - RC522 RFID-Modul MFRC522
