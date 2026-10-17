#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "wifi_manager.h"
#include "epaper_manager.h"
#include "time_manager.h"
#include "secrets.h"
#include "nfc_reader.h"

static const char *TAG = "DIENSTPLAN_TERMINAL_MAIN";

static void ui_display_task(void *pvParameters)
{
    (void)pvParameters;
    nfc_event_t event;
    char uid_text[9];
    esp_err_t last_wait_error = ESP_OK;

    ESP_LOGI(TAG, "UI-Anzeige-Task gestartet; wartet auf NFC-Ereignisse");
    while (true) {
        esp_err_t err = nfc_reader_wait_for_uid(&event, portMAX_DELAY);
        if (err == ESP_OK) {
            last_wait_error = ESP_OK;
            snprintf(uid_text, sizeof(uid_text), "%02X%02X%02X%02X",
                     event.uid[0], event.uid[1], event.uid[2], event.uid[3]);
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

void app_main(void)
{
    ESP_LOGI(TAG, "App gestartet!");
    printf("HELLO\n");
    ESP_ERROR_CHECK(nvs_flash_init());

    wifi_init();

    // etwas warten bis WLAN steht
    vTaskDelay(pdMS_TO_TICKS(5000));

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
    ESP_ERROR_CHECK(nfc_reader_start());
    xTaskCreate(time_task, "time_task", 4096, NULL, 5, NULL);
    BaseType_t ui_task_created =
        xTaskCreate(ui_display_task, "ui_display", 4096, NULL, 3, NULL);
    if (ui_task_created != pdPASS) {
        ESP_LOGE(TAG, "UI-Anzeige-Task konnte nicht gestartet werden");
    }
}
