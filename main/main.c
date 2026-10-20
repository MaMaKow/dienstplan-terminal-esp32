#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#include "buzzer.h"
#include "epaper_manager.h"
#include "nfc_reader.h"
#include "secrets.h"
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
          buzzer_play(BUZZER_SOUND_FAILURE); // sauber aus Task-Kontext!
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

void app_main(void) {
  ESP_LOGI(TAG, "App gestartet!");
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

  // etwas warten bis WLAN steht
  vTaskDelay(pdMS_TO_TICKS(5000));
  ESP_LOGI(TAG, "Buzzer init:");
  buzzer_init(25);
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
