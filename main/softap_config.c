#include "esp_log.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "SOFTAP_CONFIG";
#define SOFTAP_SSID_PREFIX "Dienstplan-Terminal"

esp_err_t softap_config_from_mac(wifi_config_t *ap_cfg) {
  uint8_t mac[6];
  esp_err_t err = esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "MAC lesen fehlgeschlagen: %s", esp_err_to_name(err));
    return err;
  }

  /* --- SSID: Präfix + letzte 2 MAC-Bytes -> "Dienstplan-Terminal-A1B2" --- */
  int n = snprintf((char *)ap_cfg->ap.ssid, sizeof(ap_cfg->ap.ssid),
                   SOFTAP_SSID_PREFIX "-%02X%02X", mac[4], mac[5]);
  if (n < 0 || (size_t)n >= sizeof(ap_cfg->ap.ssid)) {
    return ESP_ERR_INVALID_SIZE;
  }
  ap_cfg->ap.ssid_len = (uint8_t)n;

  /* --- Passwort: 8 Hex-Zeichen, verwürfelt aus den letzten 4 MAC-Bytes --- */
  uint32_t mix = ((uint32_t)mac[2] << 24) | ((uint32_t)mac[3] << 16) |
                 ((uint32_t)mac[4] << 8) | (uint32_t)mac[5];
  mix ^=
      0xA5C3E1F7u; /* einfache Verwürfelung, damit SSID nicht das PW verrät */

  snprintf((char *)ap_cfg->ap.password, sizeof(ap_cfg->ap.password), "%08lX",
           (unsigned long)mix); /* exakt 8 Zeichen */

  ap_cfg->ap.channel = 1;
  ap_cfg->ap.authmode = WIFI_AUTH_WPA2_PSK;
  ap_cfg->ap.max_connection = 4;
  ap_cfg->ap.pmf_cfg.required = false;

  ESP_LOGI(TAG, "SoftAP SSID='%s' PW='%s'", ap_cfg->ap.ssid,
           ap_cfg->ap.password);

  return ESP_OK;
}
