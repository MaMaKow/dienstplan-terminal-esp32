# Mock-Server für die Dienstplan-API v1

**Zweck:** Lokaler HTTPS-Mock für die in `docs/api_contract_v1_draft.md` beschriebene API. Die Firmware wird gegen diesen Mock entwickelt, solange der echte PHP-Server die Endpunkte noch nicht implementiert hat.

**Wichtig:** Dies ist ein reines Entwicklungswerkzeug. Kein Produktivcode, keine echten Daten, kein echter JWT. Der Contract-Draft ist die Spezifikation – bei Abweichungen gilt der Draft, und der Mock wird angepasst (nicht umgekehrt).

## Voraussetzungen

- Linux mit Bash und `ip` aus dem Paket `iproute2`
- PHP 8.1+ CLI mit OpenSSL-Erweiterung
- `openssl`, `stunnel` und `hostname`
- `jq` für die Token-Auswertung im curl-Beispiel

## Verzeichnisstruktur

```text
tools/mock-server/
├── README.md
├── start.sh                         # Zertifikate, stunnel und PHP-Mock starten
├── router.php                       # Einstiegspunkt für php -S
├── lib/
│   ├── MockApplication.php          # Request-Routing
│   ├── MockApplicationSupport.php   # Zustand, Authentifizierung, Validierung, Antworten
│   ├── MockProvisioningRoutes.php   # Token, Provisionierung und Geräteverwaltung
│   ├── MockTerminalRoutes.php      # Kartenauflösung und Buchungen
│   └── MockAppCardRoutes.php       # Kartenverwaltung für die App
├── data/
│   └── state.json                   # wird beim ersten Request lokal erzeugt; gitignored
└── certs/
    ├── dev-ca.pem                   # lokale Dev-CA; wird vom Startskript bei Bedarf erzeugt
    ├── dev-ca.key                   # privater Schlüssel; gitignored
    ├── mock-server.key              # privater Schlüssel; gitignored
    ├── mock-server.csr              # beim Start erzeugt; gitignored
    ├── mock-server.crt              # beim Start erzeugt; gitignored
    └── dev-ca.srl                   # OpenSSL-Seriennummer; gitignored
```

Die tatsächliche stunnel-Konfiguration wird von `start.sh` als temporäre Datei erzeugt. `stunnel.conf` liegt zusätzlich im Verzeichnis, wird vom Startskript aber nicht verwendet.

## Schnellstart

### 1. Zertifikate erstellen oder erneuern

Einmalig im Repository-Root starten:

```bash
cd tools/mock-server
./start.sh
```

Das Skript erzeugt `certs/dev-ca.key` und `certs/dev-ca.pem`, falls die lokale CA fehlt oder nicht zusammenpasst. `certs/mock-server.key` wird bei Bedarf angelegt. Bei jedem Start werden ein neuer CSR und ein neues Server-Zertifikat erstellt. Die temporäre SAN- und stunnel-Konfiguration wird beim regulären Beenden entfernt.

Das Server-Zertifikat enthält SANs für alle aktiven globalen LAN-IPv4-Adressen (ohne Loopback- und Link-Local-Adressen), zusätzlich `127.0.0.1`, `localhost` und den kurzen Hostnamen des Rechners. Die Startausgabe zeigt außerdem den ermittelten Hostnamen und die für die URL verwendete LAN-IP. Bei mehreren aktiven Interfaces kann diese URL-IP von der Adresse abweichen, die du zum Testen verwenden willst. Dann kannst du sie vor dem Start festlegen:

```bash
MOCK_IP=192.168.1.25 ./start.sh
```

Mit `MOCK_IP` enthält das Zertifikat diese IPv4-Adresse anstelle der automatisch ermittelten LAN-IPv4-Adressen. Die SANs für `127.0.0.1`, `localhost` und den Hostnamen bleiben erhalten.

Folgende Werte lassen sich über die Umgebung setzen:

- `MOCK_IP`: IPv4-Adresse für URL und Zertifikat
- `MOCK_TLS_PORT`: TLS-Port, Standard `8443`
- `MOCK_PHP_PORT`: lokaler PHP-Port, Standard `8088`
- `PHP_BIN`: PHP-CLI-Programm, Standard `php`
- `STUNNEL_BIN`: stunnel-Programm, Standard `stunnel`

### 2. Server starten und beenden

`start.sh` startet stunnel auf dem TLS-Port und `php -S` auf `127.0.0.1` am PHP-Port. Beide Prozesse laufen im Vordergrund; mit `Ctrl+C` werden sie beendet. Die LAN-IP, URL, CA-Datei und lokalen Ports erscheinen in der Startausgabe.

Die Firmware muss `certs/dev-ca.pem` als vertrauenswürdige CA verwenden. Wenn das CA-Zertifikat neu erzeugt wird, muss diese Datei auch in der Firmware aktualisiert werden. Bei einem Zertifikatsfehler prüfe, ob die verwendete IP oder der Hostname in den SANs des Server-Zertifikats enthalten ist.

### 3. Token abrufen und Karte auflösen

Das Beispiel setzt `jq` voraus und wird aus `tools/mock-server` ausgeführt. Ersetze die LAN-IP bei Bedarf durch die IP aus der Startausgabe oder setze sie wie oben gezeigt explizit:

```bash
LAN_IP=192.168.1.25
TOKEN=$(curl -s --cacert certs/dev-ca.pem -X POST \
  "https://${LAN_IP}:8443/v1/device-auth/token" \
  -u device_mock:test-secret \
  -d "grant_type=client_credentials&scope=terminal" \
  | jq -r .access_token)

curl --cacert certs/dev-ca.pem -X POST \
  "https://${LAN_IP}:8443/v1/terminal/cards/resolve" \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"device_id":"dev_mock","branch_id":"42","card_uid":"04A1B2C3D4E5F6"}'
```

Der Token-Endpunkt antwortet mit `access_token`, `token_type`, `expires_in`, `scope`, `device_id` und `branch_id`. `access_token` ist bei jedem Aufruf ein zufälliger Hex-String mit 64 Zeichen, gilt 900 Sekunden und ist kein JWT. Verwende den ausgegebenen Token für geschützte Requests; der String `dummy-token` ist nicht gültig.

## Verhalten des Mocks

### Implementierte Routen

- `POST /v1/device-auth/token`
- `POST /v1/device-registrations`
- `GET /v1/admin/device-registrations?status=pending`
- `POST /v1/admin/device-registrations/{registration_id}/approve`
- `GET /v1/device-registrations/{registration_id}`
- `POST /v1/device-registrations/{registration_id}/ack`
- `POST /v1/admin/devices`
- `PATCH /v1/admin/devices/{device_id}`
- `POST /v1/admin/devices/{device_id}/credentials/rotate`
- `POST /v1/admin/devices/{device_id}/revoke`
- `POST /v1/terminal/cards/resolve`
- `POST /v1/terminal/bookings`
- `POST /v1/app/cards/assign`
- `POST /v1/app/cards/{card_uid}/revoke`

Der Mock erzeugt `data/state.json` beim ersten Request mit einem fiktiven Gerät (`dev_mock`), einer Filiale (`42`), einem Mitarbeiter (`emp_123`) und einer aktiven Karte (`04A1B2C3D4E5F6`) im Zustand `off_duty`. Karten- und Buchungszustände, Registrierungen, Geräte, Rate-Limits und Idempotenz-Antworten werden in dieser JSON-Datei gespeichert und bleiben über Server-Neustarts erhalten. Die Datei ist gitignored. Es gibt keine `data/cards.json`.

Die Buchungszustände folgen dem Contract: `off_duty` erlaubt `shift_start`; `on_shift` erlaubt `shift_end` und `break_start`; `on_break` erlaubt `break_end`. Buchungen ändern den Karten-State. Ein wiederholter Request mit demselben Gerät und Idempotenz-Key liefert die gespeicherte Erfolgsantwort zurück. Der Mock begrenzt Registrierungen auf 10 Requests pro Quell-IP und Minute, insgesamt höchstens 20 aktive Pending-Registrierungen sowie Status-Polls auf 60 pro Registrierung und Minute. Diese Grenzwerte sind Mock-Konfigurationen, weil der Contract die konkreten Schwellenwerte offenlässt. Abgelaufene Registrierungen werden nicht automatisch aus der Zustandsdatei gelöscht.

Für die lokalen Admin-Routen werden `Authorization: Bearer admin-token` und `Authorization: Bearer provisioning-admin-token` akzeptiert. Für App-Kartenrouten werden `Authorization: Bearer user-token` und `Authorization: Bearer app-user-token` akzeptiert. Das sind feste Mock-Testwerte und keine echte Benutzer- oder Rollenprüfung. Claim-Statusabfragen verwenden `Authorization: Claim <claim_token>`. Die bei der Freigabe erzeugten Geräte-Credentials werden bis zum Ack AES-256-GCM-verschlüsselt gespeichert; der Sicherheitscode wird als HMAC abgelegt.

### Fehlerformat und Fehlercodes

Fehlerantworten enthalten `code`, `userMessage`, `requestId` und `details`:

```json
{
  "error": {
    "code": "CARD_UNKNOWN",
    "userMessage": "Diese Karte ist nicht zugeordnet.",
    "requestId": "req_…",
    "details": null
  }
}
```

Buchungskonflikte mit HTTP `409` enthalten zusätzlich `result: rejected`, den aktuellen `state`, `allowed_actions` und `idempotency_key`. Die im aktuellen Code verwendeten Fehlercodes sind:

| HTTP | `code`-Werte |
| ---: | --- |
| 400 | `VALIDATION_ERROR` |
| 401 | `DEVICE_UNAUTHORIZED`, `TOKEN_EXPIRED` |
| 403 | `DEVICE_REVOKED`, `CARD_BLOCKED`, `CARD_REVOKED`, `BRANCH_MISMATCH`, `CLAIM_CODE_INVALID` |
| 404 | `CARD_UNKNOWN`, `VALIDATION_ERROR` |
| 409 | `ACTION_NOT_ALLOWED`, `VALIDATION_ERROR` |
| 410 | `REGISTRATION_EXPIRED`, `CLAIM_CONSUMED` |
| 429 | `RATE_LIMITED` |
| 500 | `SERVER_ERROR` |
| 503 | `SERVICE_UNAVAILABLE` |

`BRANCH_MISMATCH` kann beim Anlegen eines Geräts mit unbekannter Filiale auch mit HTTP `404` zurückkommen. Unbekannte Routen antworten mit `VALIDATION_ERROR` und HTTP `404`.

### Fehlerfälle gezielt auslösen

- Unbekannte Karte: eine UID verwenden, die nicht in `data/state.json` registriert ist → `CARD_UNKNOWN`.
- Abgelaufenes Token: `Authorization: Bearer expired` senden → `TOKEN_EXPIRED`.
- Widerrufenes Gerät: `Authorization: Bearer revoked` senden → `DEVICE_REVOKED`.
- Ungültiger oder fehlender Bearer-Token: → `DEVICE_UNAUTHORIZED`.
- Serverfehler: `X-Mock-Error: 503` im Request-Header senden → `SERVICE_UNAVAILABLE`.

## Bekannte Abweichungen vom Contract (bewusst)

- Das Access-Token ist ein zufälliger 64-stelliger Hex-String, kein JWT. Es läuft nach 900 Sekunden ab. Der Contract zeigt ein JWT-artiges Beispiel, nennt aber dieselben Antwortfelder: `access_token`, `token_type`, `expires_in`, `scope`, `device_id` und `branch_id`.
- Der Token-Endpunkt akzeptiert `scope=terminal` auch dann, wenn das Feld fehlt; der Contract zeigt `scope` im Request.
- Die lokalen Rate-Limit-Werte sind 10 Registrierungen pro Quell-IP und Minute, maximal 20 aktive Pending-Registrierungen und 60 Status-Polls pro Registrierung und Minute. Admin-Requests haben im Mock kein separates Rate-Limit.
- Idempotenz-Antworten werden lokal gespeichert. Der Mock prüft bei Wiederverwendung des Keys nur Karte, Filiale und Aktion auf Übereinstimmung; er speichert die Einträge ohne automatische Bereinigung.
- Die Provisionierungs-, Admin- und App-Endpunkte sind im Mock vorhanden. Authentifizierung und Berechtigungen verwenden feste lokale Test-Tokens statt der realen Benutzer-/Admin-Integration.
- Der Karten-State wird in `data/state.json` gespeichert; der Contract beschreibt eine Server-API, aber kein JSON-Dateiformat für die Persistenz.
- Der Sicherheitscode ist im Mock fest auf sechs Ziffern begrenzt. Format und Länge sind im Contract noch offen.

Diese Liste bei jeder Contract-Änderung und bei jeder Änderung am Mock mitpflegen.

## Regeln

1. Der Contract-Draft (`docs/api_contract_v1_draft.md`) ist maßgeblich. Der Mock folgt ihm und definiert keine neuen API-Verträge.
2. Keine echten Mitarbeiter-, Karten- oder Zugangsdaten im Mock. Die Beispieldaten sind fiktiv.
3. Wer den Mock ändert, aktualisiert die Liste der bekannten Abweichungen.

## Troubleshooting

| Symptom | Ursache |
| --- | --- |
| `curl: (60) certificate verify failed` | `--cacert certs/dev-ca.pem` fehlt oder die CA-Datei ist nicht diejenige, die das Serverzertifikat signiert hat. |
| `curl: (60) … no alternative certificate subject name matches target host name '<ip>'` | `start.sh` hat eine andere LAN-IP erkannt als die verwendete Test-IP. Verwende `MOCK_IP=<ip> ./start.sh` oder die IP aus der Startausgabe. |
| `{"error":{"code":"DEVICE_UNAUTHORIZED",…}}` trotz Bearer-Header | `dummy-token` ist kein gültiges Token. Rufe zuerst `/v1/device-auth/token` auf und verwende dessen `access_token`. |
| ESP32 erreicht den Mock nicht | Prüfe, ob stunnel und PHP laufen, die Firewall den TLS-Port durchlässt und das Gerät die ausgegebene LAN-IP erreicht. |
| PHP meldet `Address already in use` | Der konfigurierte PHP-Port ist belegt. Beende den Prozess oder setze `MOCK_PHP_PORT`. |
| TLS-Port ist belegt | Beende den Prozess auf dem TLS-Port oder setze `MOCK_TLS_PORT`. |
