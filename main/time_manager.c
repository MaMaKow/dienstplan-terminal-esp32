#include "time_manager.h"
#include <time.h>
#include "esp_sntp.h"
#include "esp_log.h"

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
        vTaskDelay(pdMS_TO_TICKS(2000));
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
    while (1)
    {
        time_t now;
        struct tm timeinfo;

        time(&now);
        localtime_r(&now, &timeinfo);

        char strftime_buf[64];
        strftime(strftime_buf, sizeof(strftime_buf),
                 "%d.%m.%Y %H:%M:%S", &timeinfo);

        ESP_LOGI(TAG, "Aktuelle Zeit: %s", strftime_buf);

        vTaskDelay(pdMS_TO_TICKS(60000)); // 60 Sekunden
    }
}
