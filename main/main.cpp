#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "buzzer.h"
#include "epaper_manager.h"
#include "http_manager.h"
#include "nfc_reader.h"
#include "secrets.h"
#include "soc/gpio_num.h"
#include "time_manager.h"
#include "wifi_manager.h"

static const char *TAG = "DIENSTPLAN_TERMINAL_MAIN";
static void buzzer_task(void *pvParameters) {
  (void)pvParameters;
  while (true) {
    buzzer_update();
    // FREERTOS_HZ=100: pdMS_TO_TICKS(5) == 0, sonst Busy-Loop auf CPU1
    TickType_t delay = pdMS_TO_TICKS(10);
    vTaskDelay(delay > 0 ? delay : 1);
  }
}
static void on_token(const http_response_t *r, void *ctx) {

  if (r->err == ESP_OK && r->status == 200) {
    ESP_LOGI(TAG, "Got status 200 in response");
    ESP_LOGI(TAG, "Status: %d", r->status);
    ESP_LOGI(TAG, "Body Length: %d", r->body_len);
    ESP_LOGI(TAG, "Body: %s", r->body);
  }
}

static void button_task(void *pvParameters) {
  bool last_stable = true; // nicht gedrückt
  while (true) {
    bool raw = (gpio_get_level(GPIO_NUM_33) == 0); // active-low
    if (raw != last_stable) {
      vTaskDelay(pdMS_TO_TICKS(20)); // Prellzeit abwarten
      if ((gpio_get_level(GPIO_NUM_33) == 0) == raw) {
        last_stable = raw;
        if (raw) {
          // Hier kommen später die Buchungsaktionen hin.
          /**
           * Authentifizierung: HTTP Basic mit `client_id:client_secret`;
           * ausschließlich über HTTPS.
          http_request_t request = {
              .path = "/v1/device-auth/token/",
              .method = HTTP_METHOD_POST,
              .post_body = "grant_type=client_credentials&scope=terminal",
              .cb = on_token,
          };
           */
          time_t now = time(NULL);
          char time_buf[32];
          strftime(time_buf, sizeof(time_buf), "%Y-%m-%d_%H-%M-%S",
                   localtime(&now));

          // Zufallscode direkt in die Formatierung einbauen
          const char charset[] = "abcdefghijklmnopqrstuvwxyz";
          char security_code[9]; // 4 Buchstaben + 4 Ziffern + Nullterminator
          for (int i = 0; i < 4; i++) {
            security_code[i] = charset[esp_random() % (sizeof(charset) - 1)];
          }
          for (int i = 4; i < 8; i++) {
            security_code[i] = '0' + (esp_random() % 10);
          }
          security_code[8] = '\0'; // Wichtig: String terminieren

          // Request-Struktur initialisieren
          http_request_t request = {
              .path = "/v1/device-registrations",
              .method = HTTP_METHOD_POST,
              .cb = on_token,
          };
          snprintf(request.post_body, sizeof(request.post_body),
                   "device_name=Dienstplan-Terminal-%s&security_code=%s",
                   time_buf, security_code);

          esp_err_t err = http_manager_enqueue(&request, pdMS_TO_TICKS(5000));
          if (err != ESP_OK) {
            ESP_LOGE(TAG, "Enqueue fehlgeschlagen: %s", esp_err_to_name(err));
            buzzer_play(BUZZER_SOUND_FAILURE);
          }
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

static void ui_display_task(void *pvParameters) {
  (void)pvParameters;
  nfc_event_t event;
  char uid_text[9];
  esp_err_t last_wait_error = ESP_OK;

  ESP_LOGI(TAG, "UI-Anzeige-Task gestartet; wartet auf NFC-Ereignisse");
  while (true) {
    esp_err_t err = nfc_reader_wait_for_uid(&event, portMAX_DELAY);
    if (err == ESP_OK) {
      last_wait_error = ESP_OK;
      snprintf(uid_text, sizeof(uid_text), "%02X%02X%02X%02X", event.uid[0],
               event.uid[1], event.uid[2], event.uid[3]);
      epd_lock();
      epd_clear(EPD_WHITE);
      epd_draw_string(10, 8, "NFC Lesetest", &Font16, EPD_BLACK);
      epd_draw_string(10, 50, "UID:", &Font12, EPD_BLACK);
      epd_draw_string(10, 72, uid_text, &Font24, EPD_BLACK);
      epd_display();
      epd_unlock();
    } else {
      if (err != last_wait_error) {
        ESP_LOGE(TAG, "Warten auf NFC-UID fehlgeschlagen: %s",
                 esp_err_to_name(err));
        last_wait_error = err;
      }
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }
}

/* ============================= */
/*           MAIN                */
/* ============================= */

extern "C" void app_main(void) {
  ESP_LOGI(TAG, "App gestartet!");
  extern const char dev_ca_pem_start[] asm("_binary_dev_ca_pem_start");
  extern const char dev_ca_pem_end[] asm("_binary_dev_ca_pem_end");
  ESP_LOGI(TAG, "Embedded CA length=%d",
           (int)(dev_ca_pem_end - dev_ca_pem_start));
  ESP_LOGI(TAG, "Embedded CA head: %.80s", dev_ca_pem_start);
  printf("HELLO\n");
  ESP_ERROR_CHECK(nvs_flash_init());
  gpio_config_t io_conf = {
      .pin_bit_mask = (1ULL << GPIO_NUM_33),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  ESP_ERROR_CHECK(gpio_config(&io_conf));
  wifi_init();
  ESP_ERROR_CHECK(http_manager_start());
  // etwas warten bis WLAN steht
  vTaskDelay(pdMS_TO_TICKS(5000));
  ESP_LOGI(TAG, "Buzzer init:");
  buzzer_init(GPIO_NUM_25);
  ESP_LOGI(TAG, "Create buzzer task:");
  xTaskCreate(buzzer_task, "buzzer", 4096, NULL, 4, NULL);
  xTaskCreate(button_task, "button", 4096, NULL, 4, NULL);
  ntp_init();
  obtain_time();
  epd_init();
  epd_lock();
  epd_clear(EPD_WHITE);
  epd_display();
  // Zeitzone setzen (Deutschland)
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  epd_draw_string(0, 0, "NFC Terminal Dienstplan", &Font12, EPD_BLACK);
  epd_unlock();
  {
    esp_err_t nfc_err = nfc_reader_start();
    if (nfc_err != ESP_OK) {
      ESP_LOGE(TAG, "NFC-Start fehlgeschlagen: %s", esp_err_to_name(nfc_err));
    }
  }
  xTaskCreate(time_task, "time_task", 4096, NULL, 5, NULL);
  BaseType_t ui_task_created =
      xTaskCreate(ui_display_task, "ui_display", 4096, NULL, 3, NULL);
  if (ui_task_created != pdPASS) {
    ESP_LOGE(TAG, "UI-Anzeige-Task konnte nicht gestartet werden");
  }
}
