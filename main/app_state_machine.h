#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#include "esp_err.h"

typedef enum {
  APP_STATE_STARTUP,
  APP_STATE_SOFTAP_EINRICHTUNG,
  APP_STATE_WLAN_VERBINDEN,
  APP_STATE_BEREIT_ZUR_REGISTRIERUNG,
  APP_STATE_REGISTRIERUNG_LAEUFT,
  APP_STATE_REGISTRIERUNG_FREIGABE_AUSSTEHEND,
  APP_STATE_CREDENTIALS_SPEICHERN,
  APP_STATE_TOKEN_ABRUFEN,
  APP_STATE_IDLE,                          // Uhrzeit anzeigen, auf NFC warten
  APP_STATE_KARTE_GELESEN,                 // UID erkannt
  APP_STATE_BUCHUNG_MITARBEITER_ERMITTELN, // API-Kartenauflösung
  APP_STATE_BUCHUNG_AKTION_AUSWAEHLEN,     // Erlaubte Tasten anzeigen
  APP_STATE_BUCHUNG_LAEUFT,                // Buchungsanfrage
  APP_STATE_BUCHUNG_ERFOLGREICH,           // Server bestätigt
  APP_STATE_BUCHUNG_ABGELEHNT,             // Server lehnt ab
  APP_STATE_FEHLER,
} app_state_t;

typedef enum {
  APP_EVENT_STARTUP_COMPLETE,
  APP_EVENT_SOFT_AP_STARTED,
  APP_EVENT_WIFI_CREDENTIALS_RECEIVED,
  APP_EVENT_WIFI_CONNECTED,
  APP_EVENT_WIFI_FAILED,
  APP_EVENT_REGISTRATION_STARTED,
  APP_EVENT_REGISTRATION_REQUEST_SENT,
  APP_EVENT_REGISTRATION_APPROVED,
  APP_EVENT_REGISTRATION_REJECTED,
  APP_EVENT_TOKEN_ACQUIRED,
  APP_EVENT_AUTH_SUCCESS,
  APP_EVENT_CREDENTIALS_SAVED,
  APP_EVENT_TIMEOUT
} app_event_type_t;

typedef struct {
  app_event_type_t type;
} app_event_t;
esp_err_t app_state_machine_start(void);
esp_err_t app_state_machine_post_event(const app_event_t *event);
app_state_t app_state_machine_get_state(void);

#ifdef __cplusplus
}
#endif
