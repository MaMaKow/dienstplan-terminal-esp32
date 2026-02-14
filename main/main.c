#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_sleep.h"

#include "epd_2in9.h"
#include "fonts.h"

static const char *TAG = "EPAPER";

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("EPD_2IN9", ESP_LOG_INFO);

    ESP_LOGI(TAG, "=== E-Paper Test Start ===");
    epd_init();

    // Test 1: Komplett weiß
    ESP_LOGI(TAG, "\n=== TEST 1: Komplett WEISS ===");
    epd_clear(EPD_WHITE);
    epd_display();
    vTaskDelay(pdMS_TO_TICKS(3000));

    // Test 2: Komplett schwarz
    ESP_LOGI(TAG, "\n=== TEST 2: Komplett SCHWARZ ===");
    epd_clear(EPD_BLACK);
    epd_display();
    vTaskDelay(pdMS_TO_TICKS(3000));

    // Test 3: Horizontale Linien
    ESP_LOGI(TAG, "\n=== TEST 3: Horizontale Linien ===");
    epd_test_pattern();
    epd_display();
    vTaskDelay(pdMS_TO_TICKS(3000));

    // Test 4: Schachbrettmuster
    ESP_LOGI(TAG, "\n=== TEST 4: Schachbrettmuster ===");
    epd_test_checkerboard();
    epd_display();
    vTaskDelay(pdMS_TO_TICKS(3000));

    // Test 5: Text (original)
    ESP_LOGI(TAG, "\n=== TEST 5: Text ===");
    epd_clear(EPD_WHITE);
    epd_draw_string(10, 40, "Hello e-Paper", &Font24, EPD_BLACK);
    epd_draw_string(10, 80, "ESP32 + Waveshare", &Font16, EPD_BLACK);
    epd_display();

    ESP_LOGI(TAG, "\n=== Alle Tests abgeschlossen ===");
    ESP_LOGI(TAG, "Display bleibt mit letztem Bild (Text) aktiv");

    // Nicht in Deep Sleep gehen, damit Sie alle Tests sehen können
    // esp_deep_sleep_start();
}