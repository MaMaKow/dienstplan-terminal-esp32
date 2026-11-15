# Task: Implementierung des SoftAP- und lokalen Webserver-Moduls

## 1. Ziel
Erweiterung der Firmware um einen Einrichtungsmodus (SoftAP) im `WIFI_MODE_APSTA`, der beim Start eines unkonfigurierten Geräts ein eigenes WLAN aufspannt, das generierte WPA2-Passwort auf dem E-Paper-Display anzeigt und einen lokalen HTTP-Webserver zur Entgegennahme von WLAN-Zugangsdaten (SSID & Passwort) bereitstellt.

---

## 2. Betroffene und neu zu erstellende Dateien
- **Erweitern:** `main/wifi_manager.h` & `main/wifi_manager.c` (Umschaltung auf `WIFI_MODE_APSTA`, AP-Konfiguration, Zufallspasswort-Generierung, STA-Verbindungsaufbau).
- **Neu erstellen:** `main/setup_webserver.h` & `main/setup_webserver.c` (Lokaler HTTP-Server via `esp_http_server` mit HTML-Formular und POST-Endpunkt).
- **Anpassen:** `main/main.cpp` (Integration in die Startsequenz, E-Paper-Ausgabe der Zugangsdaten und Anbindung an `app_state_machine`).

---

## 3. Atomare Arbeitsschritte

### Schritt 1: SoftAP-Konfiguration in `wifi_manager`
1. **Netzwerk-Interfaces initialisieren:**
   - Sicherstellen, dass sowohl `esp_netif_create_default_wifi_sta()` als auch `esp_netif_create_default_wifi_ap()` aufgerufen werden.
2. **Modus setzen:**
   - WLAN-Modus explizit auf `WIFI_MODE_APSTA` setzen (`esp_wifi_set_mode(WIFI_MODE_APSTA)`).
3. **Passwort- & SSID-Generierung:**
   - Eindeutige AP-SSID generieren (z. B. `Dienstplan-Setup-XXXX` aus den letzten 2 Bytes der MAC-Adresse).
   - Kryptografisch zufälliges, **8-stelliges alphanumerisches WPA2-Passwort** mittels `esp_random()` generieren (Zeichenvorrat ohne verwechselbare Zeichen wie `0/O`, `1/I/l`, z. B. `ABCDEFGHJKLMNPQRSTUVWXYZ23456789`).
4. **SoftAP konfigurieren & starten:**
   - `wifi_config_t` für `WIFI_IF_AP` konfigurieren: `authmode = WIFI_AUTH_WPA2_PSK`, `max_connection = 2`, `channel = 1`.
   - Funktion `wifi_manager_start_softap(char *out_ssid, size_t ssid_len, char *out_pass, size_t pass_len)` bereitstellen.
   - Funktion `wifi_manager_stop_softap(void)` zum späteren Schließen des APs (Wechsel auf `WIFI_MODE_STA`) vorbereiten.
5. **STA-Verbindungsfunktion bereitstellen:**
   - Funktion `wifi_manager_connect_sta(const char *ssid, const char *password)` implementieren, die im laufenden `APSTA`-Modus die Station-Konfiguration (`WIFI_IF_STA`) setzt und `esp_wifi_connect()` auslöst.

### Schritt 2: Lokaler Einrichtungs-Webserver (`setup_webserver`)
1. **HTTP-Server starten (`esp_http_server.h`):**
   - Funktion `setup_webserver_start(void)` und `setup_webserver_stop(void)` implementieren (Standard-Port `80`, erreichbar unter `http://192.168.4.1`).
2. **GET-Endpunkt `/` (Einrichtungsseite):**
   - Liefert eine schlanke, eingebettete HTML-Seite (`Content-Type: text/html; charset=utf-8`).
   - Enthält ein Formular (`method="POST" action="/save"`) mit Eingabefeldern für `ssid` (Text) und `password` (Passwort) sowie einem Submit-Button.
3. **POST-Endpunkt `/save` (Zugangsdaten verarbeiten):**
   - Liest den URL-enkodierten Body (`application/x-www-form-urlencoded`) aus, dekodiert `ssid` und `password` sauber (inkl. Sonderzeichen/Leerzeichen).
   - Validiert, dass `ssid` nicht leer ist und die Pufferlängen (`32` bzw. `64` Bytes) nicht überschritten werden.
   - Übergibt die Credentials an `wifi_manager_connect_sta(ssid, password)`.
   - Sendet eine Bestätigungsseite (`HTTP 200 OK`) an den Browser zurück (z. B. *"Zugangsdaten empfangen. Terminal verbindet sich mit dem WLAN..."*).
   - Feuert das Event `APP_EVENT_WIFI_CREDENTIALS_RECEIVED` an `app_state_machine_post_event()`.
   - **Sicherheit:** Das empfangene WLAN-Passwort darf **nicht** per `ESP_LOGI`/`ESP_LOGE` protokolliert werden.

### Schritt 3: E-Paper-Anzeige & Ablaufsteuerung (`main.cpp`)
1. **Zustandsübergang:**
   - Nach `app_state_machine_start()` und Initialisierung von NVS/WLAN wird `APP_EVENT_STARTUP_COMPLETE` gepostet (Zustand wechselt zu `APP_STATE_SOFTAP_EINRICHTUNG`).
2. **SoftAP & Server starten:**
   - `wifi_manager_start_softap(...)` und `setup_webserver_start()` aufrufen.
   - `APP_EVENT_SOFT_AP_STARTED` posten.
3. **E-Paper-Ausgabe (thread-sicher mit `epd_lock()` / `epd_unlock()`):**
   - Display löschen (`epd_clear(EPD_WHITE)`).
   - Klare Einrichtungsanweisung zeichnen:
     - Titel: `"WLAN Einrichtung"`
     - Zeile 1: `"WLAN: <generierte_SSID>"`
     - Zeile 2: `"Passwort: <8-stelliges_PW>"`
     - Zeile 3: `"URL: http://192.168.4.1"`
   - `epd_display()` aufrufen.
   - Sicherstellen, dass der `time_task` (Uhranzeige) die Einrichtungsanzeige im Zustand `APP_STATE_SOFTAP_EINRICHTUNG` nicht überschreibt.

---

## 4. Akzeptanzkriterien (Definition of Done)
- [ ] **Compiler & Build:** `idf.py build` läuft ohne Warnungen oder Fehler durch.
- [ ] **WLAN-Sichtbarkeit:** Nach dem Booten erscheint ein WLAN-Netzwerk (`Dienstplan-Setup-XXXX`) auf einem Smartphone/PC.
- [ ] **Display-Ausgabe:** SSID, das 8-stellige WPA2-Passwort und die IP-Adresse sind auf dem E-Paper-Display klar lesbar und werden nicht durch die Uhrzeit überschrieben.
- [ ] **Verbindung & Webserver:** Eine Verbindung zum SoftAP mit dem angezeigten Passwort ist erfolgreich; der Aufruf von `http://192.168.4.1` im Browser lädt das Konfigurationsformular.
- [ ] **Credential-Übergabe:** Nach Absenden des Formulars bestätigt die Webseite den Empfang, im seriellen Monitor wird der STA-Verbindungsversuch (ohne Passwort im Log) sichtbar und `app_state_machine` verarbeitet das Event fehlerfrei.