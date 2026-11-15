#include "app_state_machine.h"
#include "esp_err.h"

static app_state_t current_state = APP_STATE_STARTUP;
esp_err_t app_state_machine_start(void) { current_state = APP_STATE_STARTUP; }
esp_err_t app_state_machine_post_event(const app_event_t *event) {
  if (event == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  switch (current_state) {

  case APP_STATE_STARTUP:
    if (event->type == APP_EVENT_STARTUP_COMPLETE) {
      current_state = APP_STATE_SOFTAP_EINRICHTUNG;
      return ESP_OK;
    }

    break;
  case APP_STATE_SOFTAP_EINRICHTUNG:
    if (event->type == APP_EVENT_SOFT_AP_STARTED) {
      current_state = APP_STATE_WLAN_VERBINDEN;
      return ESP_OK;
    }

    break;

  case APP_STATE_WLAN_VERBINDEN:

    if (event->type == APP_EVENT_WIFI_CONNECTED) {
      current_state = APP_STATE_BEREIT_ZUR_REGISTRIERUNG;
      return ESP_OK;
    }

    if (event->type == APP_EVENT_WIFI_FAILED) {
      current_state = APP_STATE_FEHLER;
      return ESP_OK; // Oder soll hier ein Fehler ausgegeben werden?
    }

    break;

  case APP_STATE_BEREIT_ZUR_REGISTRIERUNG:

    if (event->type == APP_EVENT_REGISTRATION_STARTED) {
      current_state = APP_STATE_REGISTRIERUNG_LAEUFT;
      return ESP_OK;
    }

    break;

  case APP_STATE_REGISTRIERUNG_LAEUFT:

    if (event->type == APP_EVENT_REGISTRATION_STARTED) {
      current_state = APP_STATE_REGISTRIERUNG_FREIGABE_AUSSTEHEND;
      return ESP_OK;
    }

    break;

  case APP_STATE_REGISTRIERUNG_FREIGABE_AUSSTEHEND:

    if (event->type == APP_EVENT_REGISTRATION_APPROVED) {
      current_state = APP_STATE_CREDENTIALS_SPEICHERN;
      return ESP_OK;
    }
    if (event->type == APP_EVENT_REGISTRATION_REJECTED) {
      current_state =
          APP_STATE_FEHLER; // Wir könnten zurück zu
                            // APP_STATE_BEREIT_ZUR_REGISTRIERUNG, aber das
                            // könnte in einen Kreis führen.
      return ESP_OK;
    }

  case APP_STATE_CREDENTIALS_SPEICHERN:

    if (event->type == APP_EVENT_TOKEN_ACQUIRED) {
      current_state = APP_STATE_TOKEN_ABRUFEN;
      return ESP_OK;
    }

    break;
  case APP_STATE_TOKEN_ABRUFEN:

    if (event->type == APP_EVENT_AUTH_SUCCESS) {
      current_state = APP_STATE_IDLE;
      return ESP_OK;
    }

    break;

  default:
    return ESP_ERR_INVALID_STATE;
    break;
  }

  return ESP_ERR_INVALID_STATE;
}
app_state_t app_state_machine_get_state(void) { return current_state; }
