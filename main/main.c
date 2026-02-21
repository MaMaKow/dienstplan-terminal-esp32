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

static const char *TAG = "DIENSTPLAN_TERMINAL_MAIN";

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

    // Zeitzone setzen (Deutschland)
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();

    xTaskCreate(time_task, "time_task", 4096, NULL, 5, NULL);
}
