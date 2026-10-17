#include "time_manager.h"
#include <time.h>
#include "esp_sntp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "epaper_manager.h"
#include "fonts.h"

/* ============================= */
/*            NTP                */
/* ============================= */
static const char *TAG = "TIME_NTP";

void ntp_init(void)
{
    ESP_LOGI(TAG, "Starte NTP...");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "de.pool.ntp.org");
    esp_sntp_init();
}

void obtain_time(void)
{
    time_t now = 0;
    struct tm timeinfo = {0};

    int retry = 0;
    const int retry_count = 15;

    while (timeinfo.tm_year < (2020 - 1900) && ++retry < retry_count)
    {
        ESP_LOGI(TAG, "Warte auf Zeit-Sync...");
        vTaskDelay(pdMS_TO_TICKS(1000));
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    if (retry < retry_count)
    {
        ESP_LOGI(TAG, "Zeit synchronisiert.");
    }
    else
    {
        ESP_LOGW(TAG, "Zeit konnte nicht synchronisiert werden.");
    }
}
/* ============================= */
/*           TIME TASK           */
/* ============================= */

void time_task(void *pvParameters)
{
    const TickType_t update_period = pdMS_TO_TICKS(60000);
    TickType_t last_wake = xTaskGetTickCount();

    while (1)
    {
        time_t now;
        struct tm timeinfo;

        time(&now);
        localtime_r(&now, &timeinfo);

        char strftime_buf[64];
        strftime(strftime_buf, sizeof(strftime_buf),
                 "%d.%m.%Y %H:%M:%S", &timeinfo);
        char date_buf[64];
        strftime(date_buf, sizeof(date_buf),
                 "%d.%m.%Y", &timeinfo);
        char time_buf[64];
        strftime(time_buf, sizeof(time_buf),
                 "%H:%M", &timeinfo);

        ESP_LOGD(TAG, "Aktuelle Zeit: %s", strftime_buf);
        epd_lock();
        epd_draw_string(EPD_TIME_X, EPD_TIME_Y, time_buf, &EPD_TIME_FONT, EPD_BLACK);
        epd_draw_string(10, 90, date_buf, &Font16, EPD_BLACK);
        epd_display();
        epd_unlock();

        // If a full panel refresh exceeded the 1 s period, skip missed ticks
        // instead of immediately replaying them and issuing back-to-back EPD
        // updates. Keep the regular cadence when the refresh finishes on time.
        const TickType_t now_ticks = xTaskGetTickCount();
        if ((TickType_t)(now_ticks - last_wake) >= update_period) {
            last_wake = now_ticks;
            // The display call is synchronous and may take longer than one
            // second for a full refresh. Render the current time immediately
            // after it becomes ready instead of adding another full second.
            continue;
        }
        vTaskDelayUntil(&last_wake, update_period);
    }
}
