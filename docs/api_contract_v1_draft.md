# API-Vertrag v1 – Entwurf

**Status:** Draft, Version 1  
**Projekt:** `dienstplan-terminal-esp32`  
**Stand:** 17. Oktober 2026

## 1. Zweck und Geltungsbereich

Dieser Entwurf beschreibt die neuen HTTPS-Verträge für Geräteverwaltung, Geräteanmeldung, NFC-Kartenauflösung und Dienstbuchungen des ESP32-Terminals sowie für die Kartenverwaltung durch eine Smartphone-App. Die bestehende Benutzer-API v0.3.0 bleibt unverändert; ihre Endpunkte werden hier nur als Bestand referenziert. Alle Beispiele zeigen JSON. Pfade, Claims und Statuswerte sind Vorschläge und müssen vor Implementierungsbeginn mit der Server-API abgestimmt werden.

## 2. Konventionen

### Basis-URL und Versionierung

- Vorgeschlagene Basis-URL: `https://api.example.invalid/dienstplan/src/php/restful-api/v1`.
- Neue Verträge liegen unter `/v1`. Der konkrete Host und ein möglicher Präfix sind deploymentabhängig.
- Die vorhandene API v0.3.0 bleibt bestehen und wird durch diesen Draft nicht geändert.
- JSON wird mit `Content-Type: application/json` gesendet. Antworten verwenden `application/json; charset=utf-8`.
- Unbekannte JSON-Felder sollen Clients ignorieren. Server dürfen zusätzliche Antwortfelder ergänzen.

### Authentifizierung und Transport

- Jeder Aufruf erfolgt über HTTPS mit Zertifikatsprüfung. Das Terminal darf Zertifikatsfehler nicht übergehen.
- Geräte-Endpunkte verwenden `Authorization: Bearer <access_token>`.
- Benutzer-Endpunkte der App verwenden die bestehende Benutzeranmeldung und deren Bearer-Token. Das Gerätegeheimnis darf niemals für Benutzer-Endpunkte verwendet werden.
- Provisionierungs-Endpunkte sind gesondert geschützt (siehe Abschnitt 4); öffentliche Selbstregistrierung eines Terminals ist nicht vorgesehen.
- Bei `401 TOKEN_EXPIRED` darf der Client einmal ein neues Token anfordern und den ursprünglichen Aufruf wiederholen. Bei anderen `401`-Fehlern darf er nicht in einer Schleife wiederholen.

### Zeitstempel

- Zeitangaben sind ISO 8601 im UTC-Format mit `Z`, zum Beispiel `2026-10-17T08:15:30Z`.
- `clientTimestamp` ist optional und dient nur Diagnose und Plausibilitätsprüfung. Die Serverzeit ist für Buchungszeitpunkt und fachliche Entscheidung maßgeblich.

### Einheitliche Fehlerantwort

Jeder Fehler enthält einen stabilen maschinenlesbaren `code` und einen kurzen deutschen `userMessage`-Text. `requestId` hilft bei der Diagnose. Interne Ursachen, Stacktraces, Tokens und Geheimnisse gehören nicht in die Antwort.

```json
{
  "error": {
    "code": "CARD_UNKNOWN",
    "userMessage": "Diese Karte ist nicht zugeordnet.",
    "requestId": "req_01JAE7K5J9Z4T6Q2",
    "details": null
  }
}
```

`details` ist optional und darf nur validierungsbezogene, nicht geheime Informationen enthalten. Geräte zeigen `userMessage` an und schreiben `code` sowie `requestId` ins Diagnoseprotokoll.

### Bestehende API v0.3.0 (unverändert)

Die bestehende Benutzer-API bietet unter anderem `POST /auth/login`, `GET /users`, `GET /users/me`, `GET /users/{id}`, Mitarbeiter-, Abwesenheits-, Überstunden-, Dienstplan-, Filial- und Notdienst-Endpunkte. Ihre Pfade und Login-Semantik sind nicht Teil dieses Entwurfs. Insbesondere wird `POST /auth/login` nicht für Geräte verwendet.

## 3. Geräteauthentifizierung

### Entscheidungsvorschlag: OAuth 2.0 Client Credentials

Jedes Terminal erhält eine eigene `client_id` und ein eigenes widerrufbares Client-Geheimnis. Es tauscht diese am Token-Endpunkt gegen ein kurzlebiges Bearer-Token mit Gerätekennung und Filialkontext. Vorgeschlagene Laufzeit: 15 Minuten. Das Terminal speichert Client-Geheimnis und Refresh-/Access-Tokens nicht im Klartext im Log. Es fordert ein neues Access-Token mit Client Credentials an, statt ein langlebiges Refresh-Token zu verwenden.

Vorteile: standardisiertes Verfahren, kurzlebige API-Tokens, gezielte Rechte und einzeln widerrufbare Geräte. Nachteile: zusätzlicher Token-Endpunkt und sichere Provisionierung des Client-Geheimnisses erforderlich.

### Alternative: direktes Gerätegeheimnis

Eine schlanke Alternative ist `client_id` plus `client_secret` als HTTP Basic- oder dedizierte Authentifizierungsdaten bei jedem Aufruf. Das spart Token-Logik, erhöht aber die Zahl der Übertragungen des langlebigen Geheimnisses und erschwert Rechte- und Laufzeitbegrenzung. Auch diese Variante braucht TLS, Rotation und Widerruf pro Gerät. Sie ist nur dann vorzuziehen, wenn OAuth in der bestehenden Serverumgebung unverhältnismäßig aufwendig ist.

### Token-Endpunkt

#### `POST /v1/device-auth/token`

Gibt ein kurzlebiges Zugriffstoken für ein provisioniertes Terminal zurück. Authentifizierung: HTTP Basic mit `client_id:client_secret`; ausschließlich über HTTPS.

Request (`application/x-www-form-urlencoded`):

```text
grant_type=client_credentials&scope=terminal
```

HTTP Basic Credentials sind im Beispiel nicht abgebildet. Erfolgsantwort `200`:

```json
{
  "access_token": "eyJhbGciOi...",
  "token_type": "Bearer",
  "expires_in": 900,
  "scope": "terminal",
  "device_id": "dev_01JAE7K5J9Z4T6Q2",
  "branch_id": "branch_42"
}
```

Fehlerantwort `401`:

```json
{
  "error": {
    "code": "DEVICE_UNAUTHORIZED",
    "userMessage": "Das Terminal ist nicht angemeldet.",
    "requestId": "req_01JAE7M0V1Q2",
    "details": null
  }
}
```

Validierung: `grant_type` muss `client_credentials` sein; Zugangsdaten müssen gültig sein; gesperrte Geräte erhalten kein Token. Token sind nur für den vorgesehenen Gerätescope und die gebundene Filiale gültig.

## 4. Geräteverwaltung

Geräteverwaltung ist eine administrative Funktion, keine offene Terminal-Selbstregistrierung. Die folgenden Admin-Endpunkte verwenden einen gesonderten Provisionierungs- oder Admin-Scope, hier als `Bearer <provisioning_admin_token>` dargestellt. Der genaue Betreiber- und Rollenprozess ist offen.

### Gerät anlegen und Filiale zuordnen

#### `POST /v1/admin/devices`

Legt ein Terminal an und weist es einer Filiale zu. Authentifizierung: Provisionierungs-Admin.

Request:

```json
{
  "device_name": "terminal-apotheke-nord-01",
  "branch_id": "42"
}
```

Erfolgsantwort `201` (Geheimnis wird nur bei Anlage ausgegeben):

```json
{
  "device_id": "dev_01JAE7K5J9Z4T6Q2",
  "device_name": "terminal-apotheke-nord-01",
  "branch_id": "42",
  "status": "active",
  "credentials": {
    "client_id": "terminal-apotheke-nord-01",
    "client_secret": "one-time-secret-value"
  },
  "created_at": "2026-10-17T08:15:30Z"
}
```

Fehlerantwort `404` bei unbekannter Filiale:

```json
{
  "error": {
    "code": "BRANCH_MISMATCH",
    "userMessage": "Das Terminal kann dieser Filiale nicht zugeordnet werden.",
    "requestId": "req_01JAE7N1F2A3",
    "details": null
  }
}
```

Validierung: `device_name` ist erforderlich und innerhalb des Mandanten eindeutig; `branch_id` muss existieren und für den Admin zugänglich sein. Ein erfolgreicher Aufruf gibt das Geheimnis genau einmal zurück; der Server speichert es geschützt.

### Zugangsdaten erneuern

#### `POST /v1/admin/devices/{device_id}/credentials/rotate`

Erzeugt ein neues Client-Geheimnis und macht das alte nach erfolgreicher Rotation ungültig. Authentifizierung: Provisionierungs-Admin.

Request:

```json
{
  "reason": "planned_rotation"
}
```

Erfolgsantwort `200`:

```json
{
  "device_id": "dev_01JAE7K5J9Z4T6Q2",
  "client_id": "terminal-apotheke-nord-01",
  "client_secret": "new-one-time-secret-value",
  "rotated_at": "2026-10-17T09:00:00Z"
}
```

Fehlerantwort `404`:

```json
{
  "error": {
    "code": "VALIDATION_ERROR",
    "userMessage": "Das Terminal konnte nicht gefunden werden.",
    "requestId": "req_01JAE7P4C5D6",
    "details": null
  }
}
```

Validierung: Gerät muss existieren und aktiv sein. Neue Zugangsdaten werden einmalig ausgegeben. Bereits ausgestellte Access-Tokens des Geräts sollen bei Rotation widerrufen werden.

### Gerät widerrufen

#### `POST /v1/admin/devices/{device_id}/revoke`

Sperrt das Gerät und widerruft seine Zugangsdaten sowie aktive Tokens. Authentifizierung: Provisionierungs-Admin.

Request:

```json
{
  "reason": "device_retired"
}
```

Erfolgsantwort `200`:

```json
{
  "device_id": "dev_01JAE7K5J9Z4T6Q2",
  "status": "revoked",
  "revoked_at": "2026-10-17T09:10:00Z"
}
```

Fehlerantwort `404`:

```json
{
  "error": {
    "code": "VALIDATION_ERROR",
    "userMessage": "Das Terminal konnte nicht gefunden werden.",
    "requestId": "req_01JAE7Q7E8F9",
    "details": null
  }
}
```

Validierung: Wiederholter Widerruf eines bereits widerrufenen Geräts soll idempotent mit `200` und `status: revoked` beantwortet werden. Tokens des Geräts müssen abgelehnt werden.

## 5. Kartenauflösung

### `POST /v1/terminal/cards/resolve`

Löst eine gelesene NFC-UID im Kontext des authentifizierten Geräts auf. Authentifizierung: Geräte-Bearer-Token. `device_id` und `branch_id` im Request müssen mit Token-Claims und Gerätezuordnung übereinstimmen.

Request:

```json
{
  "device_id": "dev_01JAE7K5J9Z4T6Q2",
  "branch_id": "42",
  "card_uid": "04A1B2C3D4E5F6",
  "client_timestamp": "2026-10-17T08:15:30Z"
}
```

Erfolgsantwort `200` – Mitarbeiter abwesend:

```json
{
  "card_status": "active",
  "employee": {
    "id": "emp_123",
    "display_name": "Erika Mustermann"
  },
  "state": "off_duty",
  "allowed_actions": ["shift_start"],
  "display": {
    "title": "Guten Morgen, Erika",
    "message": "Dienstbeginn möglich.",
    "action_labels": {
      "shift_start": "Dienstbeginn"
    }
  },
  "resolved_at": "2026-10-17T08:15:31Z"
}
```

Fehlerantwort `404` – unbekannte Karte:

```json
{
  "error": {
    "code": "CARD_UNKNOWN",
    "userMessage": "Diese Karte ist nicht zugeordnet.",
    "requestId": "req_01JAE7R0G1H2",
    "details": null
  }
}
```

Validierung: UID als Hex-String ohne Trennzeichen; zulässige Länge und Normalisierung müssen serverseitig für eingesetzte Kartentypen festgelegt werden. Karte, Mitarbeiter, Gerätestatus und Filialbindung werden geprüft. Die Aktionsliste wird für jeden Scan frisch vom Server geliefert. Mögliche Zustände und Aktionen:

| Zustand | `allowed_actions` |
|---|---|
| `off_duty` | `["shift_start"]` |
| `on_shift` | `["shift_end", "break_start"]` |
| `on_break` | `["break_end"]` |

Für `CARD_BLOCKED` oder `CARD_REVOKED` wird keine Mitarbeiter- oder Aktionsinformation zurückgegeben. Das Terminal bietet keine Aktion an, die nicht in `allowed_actions` enthalten ist.

## 6. Buchung

### `POST /v1/terminal/bookings`

Führt eine Buchung aus. Authentifizierung: Geräte-Bearer-Token. Die API prüft Karte, Gerät, Filiale, Aktion und aktuellen Zustand erneut; eine zuvor erlaubte Aktion ist keine Erfolgsgarantie.

Request:

```json
{
  "device_id": "dev_01JAE7K5J9Z4T6Q2",
  "branch_id": "42",
  "card_uid": "04A1B2C3D4E5F6",
  "action": "shift_start",
  "idempotency_key": "01JAE7S8J9K0M1N2P3Q4R5T6V7",
  "client_timestamp": "2026-10-17T08:16:02Z"
}
```

`Idempotency-Key` darf alternativ als Header übertragen werden; für v1 wird das JSON-Feld als verbindliche Form vorgeschlagen. Der Schlüssel ist pro Gerät eindeutig und mindestens 24 Stunden serverseitig aufzubewahren.

Erfolgsantwort `200`:

```json
{
  "result": "success",
  "booking_id": "booking_01JAE7T2U3V4",
  "action": "shift_start",
  "message": "Dienstbeginn wurde gebucht.",
  "state": "on_shift",
  "allowed_actions": ["shift_end", "break_start"],
  "completed_at": "2026-10-17T08:16:03Z",
  "idempotency_key": "01JAE7S8J9K0M1N2P3Q4R5T6V7"
}
```

Fachliche Ablehnung `409`:

```json
{
  "result": "rejected",
  "error": {
    "code": "ACTION_NOT_ALLOWED",
    "userMessage": "Diese Buchung ist aktuell nicht möglich.",
    "requestId": "req_01JAE7U5W6X7",
    "details": null
  },
  "state": "on_shift",
  "allowed_actions": ["shift_end", "break_start"],
  "idempotency_key": "01JAE7S8J9K0M1N2P3Q4R5T6V7"
}
```

Technische Fehler verwenden die Fehlerstruktur aus Abschnitt 2 und die Fehlercodes aus Abschnitt 8. Ein Transportfehler kann bedeuten, dass der Server gebucht hat, die Antwort aber verloren ging. Das Terminal darf deshalb denselben Request nur mit demselben Idempotenz-Key wiederholen. Derselbe Key mit identischem Request liefert dieselbe gespeicherte HTTP-Antwort und denselben Body wie beim ersten Versuch. Derselbe Key mit abweichender UID, Filiale oder Aktion liefert `409 VALIDATION_ERROR` und verändert keine Buchung. Unterschiedliche Keys stehen für unterschiedliche Buchungsversuche.

Validierung: alle Identitäts- und Kontextfelder müssen zum Token und zur Serverzuordnung passen. `action` ist genau einer der vier definierten Aktionsnamen. Die Aktion muss im aktuellen Serverzustand erlaubt sein. Die Serverzeit bestimmt `completed_at`. Bei Netzwerk- oder Serverfehlern darf das Terminal keine lokale Buchung als erfolgt darstellen und speichert keine Buchung zur späteren Übermittlung.

## 7. Kartenverwaltung für die Smartphone-App

Die Endpunkte verwenden ein Benutzer-Bearer-Token aus der bestehenden Benutzeranmeldung. Nur Benutzer mit passender Berechtigung für die betroffene Filiale, zum Beispiel Filialleitung, dürfen Karten zuordnen oder sperren. Das Terminal nimmt nicht an der Kartenregistrierung teil.

### Karte zuordnen

#### `POST /v1/app/cards/assign`

Ordnet eine UID einem Mitarbeiter zu. Authentifizierung: Benutzer-Bearer-Token mit Berechtigung `cards:write` für `branch_id`.

Request:

```json
{
  "card_uid": "04A1B2C3D4E5F6",
  "employee_id": "emp_123",
  "branch_id": "42"
}
```

Erfolgsantwort `201`:

```json
{
  "card": {
    "card_uid": "04A1B2C3D4E5F6",
    "employee_id": "emp_123",
    "branch_id": "42",
    "status": "active",
    "assigned_at": "2026-10-17T10:00:00Z"
  }
}
```

Fehlerantwort `409` bei bereits zugeordneter Karte:

```json
{
  "error": {
    "code": "VALIDATION_ERROR",
    "userMessage": "Diese Karte ist bereits einer Person zugeordnet.",
    "requestId": "req_01JAE7V8Y9Z0",
    "details": {
      "reason": "card_already_assigned"
    }
  }
}
```

Validierung: Mitarbeiter und Filiale müssen existieren und dem Benutzer zugänglich sein. **Vorschlag:** erneute Zuordnung wird standardmäßig abgelehnt; bestehende Zuordnung wird nie still überschrieben. Eine Änderung erfolgt als expliziter App-Schritt mit Anzeige der bisherigen Zuordnung und Bestätigung, danach kontrolliertes Umhängen. Ob dies ein eigener Transfer-Endpunkt oder ein bestätigter Parameter wird, ist offen.

### Karte sperren oder widerrufen

#### `POST /v1/app/cards/{card_uid}/revoke`

Sperrt eine Karte, sodass nachfolgende Auflösungen und Buchungen abgelehnt werden. Authentifizierung: Benutzer-Bearer-Token mit `cards:write` für die Filiale der Karte.

Request:

```json
{
  "reason": "lost"
}
```

Erfolgsantwort `200`:

```json
{
  "card_uid": "04A1B2C3D4E5F6",
  "status": "revoked",
  "revoked_at": "2026-10-17T10:05:00Z"
}
```

Fehlerantwort `404`:

```json
{
  "error": {
    "code": "CARD_UNKNOWN",
    "userMessage": "Diese Karte ist nicht registriert.",
    "requestId": "req_01JAE7W1A2B3",
    "details": null
  }
}
```

Validierung: UID muss syntaktisch gültig sein; Karte muss im Berechtigungsbereich des Benutzers liegen. Wiederholter Widerruf soll idempotent `200` mit `status: revoked` liefern.

## 8. Fehlercode-Referenz

| HTTP | Code | Bedeutung | `userMessage`-Beispiel |
|---:|---|---|---|
| 401 | `DEVICE_UNAUTHORIZED` | Gerätezugangsdaten fehlen oder sind ungültig. | „Das Terminal ist nicht angemeldet.“ |
| 401 | `TOKEN_EXPIRED` | Access-Token ist abgelaufen. | „Die Verbindung muss erneuert werden.“ |
| 403 | `DEVICE_REVOKED` | Gerät wurde gesperrt. | „Dieses Terminal ist deaktiviert.“ |
| 404 | `CARD_UNKNOWN` | UID ist nicht registriert. | „Diese Karte ist nicht zugeordnet.“ |
| 403 | `CARD_BLOCKED` | Karte ist vorübergehend gesperrt. | „Diese Karte ist gesperrt.“ |
| 403 | `CARD_REVOKED` | Karte wurde widerrufen. | „Diese Karte ist nicht mehr gültig.“ |
| 409 | `ACTION_NOT_ALLOWED` | Aktion ist im aktuellen Zustand nicht zulässig. | „Diese Buchung ist aktuell nicht möglich.“ |
| 403 | `BRANCH_MISMATCH` | Gerät, Karte, Mitarbeiter oder Anfrage gehören nicht zur Filiale. | „Die Buchung ist für dieses Terminal nicht möglich.“ |
| 400 | `VALIDATION_ERROR` | Feld fehlt, ist ungültig oder Idempotenz-Key wurde mit anderem Body wiederverwendet. | „Die Anfrage konnte nicht verarbeitet werden.“ |
| 500 | `SERVER_ERROR` | Unerwarteter interner Fehler. | „Der Vorgang ist fehlgeschlagen. Bitte erneut versuchen.“ |
| 503 | `SERVICE_UNAVAILABLE` | Dienst ist vorübergehend nicht verfügbar. | „Der Server ist gerade nicht erreichbar.“ |
HTTP 	Code 	Bedeutung 	userMessage-Beispiel 

Ein Fehler kann zusätzlich `Retry-After` enthalten. Clients wiederholen nur nach einer klaren Wiederholungsregel. Buchungen werden ausschließlich mit unverändertem Body und demselben Idempotenz-Key wiederholt. `CARD_UNKNOWN`, `CARD_BLOCKED`, `CARD_REVOKED`, `ACTION_NOT_ALLOWED`, `BRANCH_MISMATCH` und `VALIDATION_ERROR` sind nicht automatisch wiederholbar.

## 9. Ablaufbeispiele

Alle Abläufe beginnen mit einem gültigen Gerätetoken. Für jede neue Buchung wird die Karte erneut gelesen und `/cards/resolve` erneut aufgerufen.

### Abwesend → Dienstbeginn

1. Terminal sendet UID an `POST /v1/terminal/cards/resolve`.
2. Server antwortet mit `state: off_duty` und `allowed_actions: ["shift_start"]`.
3. Terminal zeigt nur „Dienstbeginn“ als zulässige Aktion an.
4. Nach Tastendruck sendet Terminal `POST /v1/terminal/bookings` mit `action: shift_start` und neuem Idempotenz-Key.
5. Server antwortet `result: success`, `state: on_shift`, `allowed_actions: ["shift_end", "break_start"]`. Erst jetzt zeigt das Terminal Erfolg und spielt den Erfolgston.

### Im Dienst → Pause oder Dienstende

1. Kartenauflösung liefert `state: on_shift` und `allowed_actions: ["shift_end", "break_start"]`.
2. Terminal zeigt nur diese beiden Aktionen.
3. Buchung mit der gewählten Aktion und eigenem Idempotenz-Key wird serverseitig erneut geprüft.
4. Bei `break_start` liefert Erfolg `state: on_break`, `allowed_actions: ["break_end"]`; bei `shift_end` liefert die API den resultierenden Zustand, üblicherweise `off_duty`, und dessen Aktionsliste.

### In der Pause → Pausenende

1. Kartenauflösung liefert `state: on_break` und `allowed_actions: ["break_end"]`.
2. Terminal zeigt nur „Pausenende“.
3. Erfolgreiche Buchung liefert `state: on_shift` und `allowed_actions: ["shift_end", "break_start"]`.

### Unbekannte oder gesperrte Karte

Auflösung liefert `404 CARD_UNKNOWN` oder `403 CARD_BLOCKED`/`CARD_REVOKED`. Das Terminal zeigt `userMessage`, bietet keine Aktion an und sendet keine Buchung.

### Unzulässige Aktion nach Zustandsänderung

Die Karte wurde aufgelöst und `break_start` war erlaubt. Zwischenzeitlich wurde der Zustand serverseitig geändert. Buchung antwortet mit `409 ACTION_NOT_ALLOWED` und der aktuellen `state`- und `allowed_actions`-Liste, sofern der Benutzerkontext dies zulässt. Terminal zeigt die Ablehnung und muss für einen neuen Versuch einen neuen Kartenscan ausführen.

### Abgelaufenes Token

Ein API-Aufruf antwortet mit `401 TOKEN_EXPIRED`. Das Terminal fordert einmal über `/v1/device-auth/token` ein frisches Token an und wiederholt anschließend den ursprünglichen Aufruf. Bei einer Buchung bleibt der Idempotenz-Key unverändert. Bei `DEVICE_REVOKED` oder `DEVICE_UNAUTHORIZED` wird nicht weiter versucht; das Terminal zeigt eine technische Störung.

### Netzwerkabbruch und Idempotenz

Terminal sendet eine Buchung mit Key `K1`, erhält aber wegen Verbindungsabbruch keine Antwort. Es gibt keinen lokalen Buchungspuffer und keine Erfolgsmeldung. Bei wiederhergestellter Verbindung sendet es exakt denselben Request mit `K1` erneut. Hat der Server den ersten Request verarbeitet, liefert er dieselbe gespeicherte Antwort; andernfalls verarbeitet er ihn einmalig. Ein neuer Key wäre eine neue Buchung und darf für diesen Wiederholungsfall nicht verwendet werden.

### Serverfehler oder Offline-Betrieb

Bei `503 SERVICE_UNAVAILABLE`, DNS-/TLS-Fehler oder Timeout zeigt das Terminal eine technische Fehlermeldung. Es nimmt keine Buchung lokal an. Für eine erneute Bedienung muss die Karte erneut vorgehalten werden; nur die technische Wiederholung eines möglicherweise bereits übermittelten Buchungsrequests verwendet denselben Idempotenz-Key.

## 10. Offene Punkte / zu klärende Entscheidungen

- ❓ Welcher konkrete API-Host, Pfadpräfix und Mandantenkontext gelten in Entwicklung und Produktion?
- ❓ Werden die neuen Verträge als `/v1` bereitgestellt oder an die vorhandene API-Versionierung angepasst?
- ❓ Unterstützt die bestehende Serverplattform OAuth 2.0 Client Credentials? Falls nicht, wird die direkte Geheimnis-Alternative akzeptiert?
- ❓ Wie werden Geräte initial provisioniert, und welche Rolle darf Geräte anlegen, Filialen ändern, Geheimnisse rotieren und Geräte widerrufen?
- ❓ Wie wird ein Gerätegeheimnis sicher an das physische Terminal übertragen und dort geschützt gespeichert?
- ❓ Welche Token-Laufzeit, Scopes und Widerrufslatenz sind verbindlich?
- ❓ Welche NFC-UID-Längen und Byte-Reihenfolge liefern die eingesetzten Karten und der MFRC522? Wie wird die UID kanonisch normalisiert?
- ❓ Ist `branch_id` immer fest an ein Terminal gebunden, und wie werden Filialwechsel administriert?
- ❓ Welche Felder enthält der aktuelle Buchungszustand zusätzlich zu `off_duty`, `on_shift` und `on_break` (z. B. Krankheit, Urlaub, Sonderzustände)?
- ❓ Welche HTTP-Statuscodes und Fehlercodes sind mit vorhandener Server-Fehlerbehandlung kompatibel?
- ❓ Wie lange muss der Server Idempotenz-Keys speichern? Vorschlag: mindestens 24 Stunden; ist das für Offline-/Wiederanlaufzeiten ausreichend?
- ❓ Ist `idempotency_key` im JSON-Body oder der Standardheader `Idempotency-Key` verbindlich? Der Draft verwendet vorläufig das JSON-Feld.
- ❓ Soll eine Buchungsablehnung den aktuellen Zustand immer mitliefern dürfen, oder sind Fälle ohne Berechtigung davon auszunehmen?
- ❓ Wie lange gelten Mitarbeitername und Aktionsliste als anzeigbar? Der Firmwareablauf sieht zehn Sekunden bis zur Aktionsauswahl vor; der Server prüft beim Buchen trotzdem erneut.
- ❓ Wie soll Kartenübertragung zwischen Mitarbeitern in der App bestätigt und auditiert werden? Draft-Vorschlag: vorhandene Zuordnung standardmäßig nicht überschreiben.
- ❓ Sollen Kartenstatus `blocked` (reversibel) und `revoked` (dauerhaft) fachlich getrennt geführt werden?
- ❓ Welche Rollen und Filialrechte der bestehenden Benutzer-API entsprechen der vorgeschlagenen Berechtigung `cards:write`?
- ❓ Werden `device_id` und `branch_id` im Body zusätzlich zu Token-Claims benötigt, oder genügt der authentifizierte Kontext?
- ❓ Welche Retry-Regeln und Zeitlimits gelten für Token-, Auflösungs- und Buchungsaufrufe? Der Firmwareclient muss begrenzte Timeouts verwenden und darf Buchungen niemals mit neuem Idempotenz-Key wiederholen.
