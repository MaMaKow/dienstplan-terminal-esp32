#include "epaper_manager.h"
#include <stdio.h>

static const char *TAG = "EPAPER";
// #include "epd_2in9.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/task.h"
#include "esp_log.h"

// SSD1680-Speichergeometrie für die Controller-Konfiguration.
#define EPD_RAM_WIDTH 128
#define EPD_RAM_HEIGHT 296

// GPIOs
#define PIN_CS 5
#define PIN_DC 17
#define PIN_RST 16
#define PIN_BUSY 4
// SPI
#define PIN_CLK 18 // SCLK
#define PIN_DIN 23 // MOSI
#define SPI_HOST VSPI_HOST

static spi_device_handle_t spi = NULL;
// SSD1680 Commands
#define CMD_DRIVER_OUTPUT_CONTROL 0x01
#define CMD_BOOSTER_SOFT_START 0x0C
#define CMD_GATE_SCAN_START_POSITION 0x0F
#define CMD_DEEP_SLEEP 0x10
#define CMD_DATA_ENTRY_MODE_SETTING 0x11
#define CMD_SW_RESET 0x12
#define CMD_TEMPERATURE_SENSOR 0x1A
#define CMD_MASTER_ACTIVATION 0x20
#define CMD_DISPLAY_UPDATE_CONTROL_1 0x21
#define CMD_DISPLAY_UPDATE_CONTROL_2 0x22
#define CMD_WRITE_RAM_BW 0x24
#define CMD_WRITE_RAM_RED 0x26
#define CMD_READ_RAM 0x25
#define CMD_VCOM_SENSE_DURATION 0x29
#define CMD_VCOM_SETTING 0x2C
#define CMD_DUMMY_LINE_PERIOD 0x2D
#define CMD_GATE_TIME_SETTING 0x2E
#define CMD_SET_RAM_X_ADDRESS_START_END 0x44
#define CMD_SET_RAM_Y_ADDRESS_START_END 0x45
#define CMD_SET_RAM_X_ADDRESS_COUNTER 0x4E
#define CMD_SET_RAM_Y_ADDRESS_COUNTER 0x4F
#define CMD_BORDER_WAVEFORM_CONTROL 0x3C
#define CMD_END_OPTION 0x22

static void epd_send_command(uint8_t cmd)
{
    gpio_set_level(PIN_DC, 0); // DC = 0 für Befehl

    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    spi_device_polling_transmit(spi, &t);
}

static void epd_send_data(uint8_t data)
{
    gpio_set_level(PIN_DC, 1); // DC = 1 für Daten

    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &data,
    };
    spi_device_polling_transmit(spi, &t);
}

static void epd_send_data_buffer(const uint8_t *data, size_t len)
{
    gpio_set_level(PIN_DC, 1); // DC = 1 für Daten

    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data,
    };
    spi_device_polling_transmit(spi, &t);
}

static void epd_wait_busy(void)
{
    ESP_LOGD(TAG, "Waiting for BUSY...");
    int timeout = 400; // 4 seconds timeout
    while (gpio_get_level(PIN_BUSY) == 1 && timeout > 0)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
        timeout--;
    }

    if (timeout <= 0)
    {
        ESP_LOGW(TAG, "BUSY timeout!");
    }
    else
    {
        ESP_LOGD(TAG, "BUSY released");
    }

    vTaskDelay(pdMS_TO_TICKS(10));
}

static void epd_reset(void)
{
    ESP_LOGD(TAG, "Display reset");
    gpio_set_level(PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(200));
    gpio_set_level(PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(200));
}

void epd_init(void)
{
    ESP_LOGI(TAG, "Initializing e-Paper display 2.9\"");

    // GPIO Konfiguration
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << PIN_CS) | (1ULL << PIN_DC) | (1ULL << PIN_RST),
        .pull_down_en = 0,
        .pull_up_en = 0,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&io_conf);

    gpio_config_t busy_conf = {
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << PIN_BUSY),
        .pull_down_en = 0,
        .pull_up_en = 1,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&busy_conf);

    // SPI Initialisierung
    spi_bus_config_t buscfg = {
        .miso_io_num = -1,
        .mosi_io_num = PIN_DIN,
        .sclk_io_num = PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 4 * 1000 * 1000, // 4 MHz
        .mode = 0,
        .spics_io_num = PIN_CS,
        .queue_size = 7,
    };

    ESP_ERROR_CHECK(spi_bus_initialize(SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
    ESP_ERROR_CHECK(spi_bus_add_device(SPI_HOST, &devcfg, &spi));

    epd_reset();
    epd_wait_busy();

    // Software Reset
    epd_send_command(CMD_SW_RESET);
    epd_wait_busy();

    // Driver Output Control (MUX Gates - verwende RAM HEIGHT!)
    epd_send_command(CMD_DRIVER_OUTPUT_CONTROL);
    epd_send_data((EPD_RAM_HEIGHT - 1) & 0xFF);        // 295 & 0xFF = 0x27
    epd_send_data(((EPD_RAM_HEIGHT - 1) >> 8) & 0xFF); // 295 >> 8 = 0x01
    epd_send_data(0x00);                               // GD = 0, SM = 0, TB = 0

    // Data Entry Mode Setting
    epd_send_command(CMD_DATA_ENTRY_MODE_SETTING);
    epd_send_data(0x03); // X increment, Y increment

    // Set RAM X address (horizontal im RAM): 0 bis (128/8)-1 = 0 bis 15
    epd_send_command(CMD_SET_RAM_X_ADDRESS_START_END);
    epd_send_data(0x00);
    epd_send_data((EPD_RAM_WIDTH / 8) - 1); // (128/8)-1 = 15 = 0x0F

    // Set RAM Y address (vertical im RAM): 0 bis 295
    epd_send_command(CMD_SET_RAM_Y_ADDRESS_START_END);
    epd_send_data(0x00);                               // Start LSB
    epd_send_data(0x00);                               // Start MSB
    epd_send_data((EPD_RAM_HEIGHT - 1) & 0xFF);        // End LSB = 295 & 0xFF = 0x27
    epd_send_data(((EPD_RAM_HEIGHT - 1) >> 8) & 0xFF); // End MSB = 295 >> 8 = 0x01

    // Border Waveform Control
    epd_send_command(CMD_BORDER_WAVEFORM_CONTROL);
    epd_send_data(0x05);

    // Temperature Sensor
    epd_send_command(CMD_TEMPERATURE_SENSOR);
    epd_send_data(0x80);

    // Display Update Control 1
    epd_send_command(CMD_DISPLAY_UPDATE_CONTROL_1);
    epd_send_data(0x00);
    epd_send_data(0x80);

    // Set RAM counter to origin
    epd_send_command(CMD_SET_RAM_X_ADDRESS_COUNTER);
    epd_send_data(0x00);

    epd_send_command(CMD_SET_RAM_Y_ADDRESS_COUNTER);
    epd_send_data(0x00);
    epd_send_data(0x00);

    epd_wait_busy();

    ESP_LOGI(TAG, "e-Paper display initialized");
    ESP_LOGI(TAG, "Framebuffer size: %u bytes", (unsigned)epd_fb_size());
}

void epd_clear(uint8_t color)
{
    ESP_LOGI(TAG, "Clearing framebuffer with color 0x%02X", color);
    epd_fb_clear(color);
}

void epd_display(void)
{
    const uint8_t *framebuffer = epd_fb_data();
    const size_t framebuffer_size = epd_fb_size();
    ESP_LOGI(TAG, "Updating display - Framebuffer size: %u bytes", (unsigned)framebuffer_size);

    // Debug: Zeige Framebuffer-Inhalt
    ESP_LOGI(TAG, "FB before send - First 16 bytes:");
    for (size_t i = 0; i < 16 && i < framebuffer_size; i++)
    {
        printf("%02X ", framebuffer[i]);
    }
    printf("\n");

    // Set RAM counter to start
    epd_send_command(CMD_SET_RAM_X_ADDRESS_COUNTER);
    epd_send_data(0x00);

    epd_send_command(CMD_SET_RAM_Y_ADDRESS_COUNTER);
    epd_send_data(0x00);
    epd_send_data(0x00);

    // Write RAM
    epd_send_command(CMD_WRITE_RAM_BW);
    epd_send_data_buffer(framebuffer, framebuffer_size);

    // Display Update
    epd_send_command(CMD_DISPLAY_UPDATE_CONTROL_2);
    epd_send_data(0xF7);

    epd_send_command(CMD_MASTER_ACTIVATION);

    epd_wait_busy();

    ESP_LOGI(TAG, "Display updated");
}

void epd_test_pattern(void)
{
    epd_fb_test_pattern();
}

void epd_test_checkerboard(void)
{
    epd_fb_test_checkerboard();
}

void epd_draw_string(int x, int y, const char *text,
                     const sFONT *font, uint8_t color)
{
    epd_fb_draw_string(x, y, text, font, color);
}
