#include "epaper_manager.h"
// #include "epd_2in9.h"
#include "fonts.h"

static const char *TAG = "EPAPER";
// #include "epd_2in9.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>

// Physikalische Display-Größe
#define EPD_WIDTH 296
#define EPD_HEIGHT 128

// RAM-Layout des SSD1680 (um 90° gedreht!)
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
static uint8_t framebuffer[EPD_RAM_WIDTH * EPD_RAM_HEIGHT / 8]; // 128 * 296 / 8 = 4736 bytes

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
    ESP_LOGI(TAG, "Framebuffer size: %d bytes", sizeof(framebuffer));
}

void epd_clear(uint8_t color)
{
    ESP_LOGI(TAG, "Clearing framebuffer with color 0x%02X", color);
    memset(framebuffer, color, sizeof(framebuffer));

    // Debug: Zeige erste und letzte Bytes
    ESP_LOGI(TAG, "FB after clear - First 8 bytes: %02X %02X %02X %02X %02X %02X %02X %02X",
             framebuffer[0], framebuffer[1], framebuffer[2], framebuffer[3],
             framebuffer[4], framebuffer[5], framebuffer[6], framebuffer[7]);

    size_t last = sizeof(framebuffer) - 1;
    ESP_LOGI(TAG, "FB after clear - Last byte [%d]: %02X", last, framebuffer[last]);
}

void epd_display(void)
{
    ESP_LOGI(TAG, "Updating display - Framebuffer size: %d bytes", sizeof(framebuffer));

    // Debug: Zeige Framebuffer-Inhalt
    ESP_LOGI(TAG, "FB before send - First 16 bytes:");
    for (int i = 0; i < 16; i++)
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
    epd_send_data_buffer(framebuffer, sizeof(framebuffer));

    // Display Update
    epd_send_command(CMD_DISPLAY_UPDATE_CONTROL_2);
    epd_send_data(0xF7);

    epd_send_command(CMD_MASTER_ACTIVATION);

    epd_wait_busy();

    ESP_LOGI(TAG, "Display updated");
}

// Neue Testfunktionen
void epd_test_pattern(void)
{
    ESP_LOGI(TAG, "Drawing test pattern");

    // Komplett weiß
    memset(framebuffer, 0xFF, sizeof(framebuffer));

    // Schwarze horizontale Linien alle 16 Pixel
    // Im gedrehten Layout: horizontale Linien = verschiedene Y-Werte
    for (int y = 0; y < EPD_RAM_HEIGHT; y += 16)
    {
        for (int x = 0; x < EPD_RAM_WIDTH / 8; x++)
        {
            framebuffer[y * (EPD_RAM_WIDTH / 8) + x] = 0x00;
        }
    }

    ESP_LOGI(TAG, "Test pattern ready");
}

void epd_test_checkerboard(void)
{
    ESP_LOGI(TAG, "Drawing checkerboard");

    for (int y = 0; y < EPD_RAM_HEIGHT; y++)
    {
        for (int x = 0; x < EPD_RAM_WIDTH / 8; x++)
        {
            // Schachbrettmuster: wechselnde Bytes
            if ((y / 8 + x) % 2 == 0)
            {
                framebuffer[y * (EPD_RAM_WIDTH / 8) + x] = 0xAA; // 10101010
            }
            else
            {
                framebuffer[y * (EPD_RAM_WIDTH / 8) + x] = 0x55; // 01010101
            }
        }
    }

    ESP_LOGI(TAG, "Checkerboard ready");
}

void epd_draw_string(int x, int y, const char *text,
                     const sFONT *font, uint8_t color)
{
    if (font == NULL)
    {
        ESP_LOGW(TAG, "Font is NULL");
        return;
    }

    ESP_LOGI(TAG, "Drawing string '%s' at (%d,%d) with color 0x%02X", text, x, y, color);

    uint32_t x_pos = x;
    uint32_t y_pos = y;
    int char_count = 0;

    while (*text)
    {
        uint32_t char_idx = *text - ' ';

        if (char_idx >= 95)
        {
            ESP_LOGW(TAG, "Character '%c' (0x%02X) out of range", *text, *text);
            text++;
            continue;
        }

        const uint8_t *glyph_data = font->table + char_idx * font->Height * ((font->Width + 7) / 8);

        if (char_count == 0)
        {
            ESP_LOGI(TAG, "First char '%c': idx=%d, width=%d, height=%d",
                     *text, char_idx, font->Width, font->Height);
            ESP_LOGI(TAG, "Glyph data: %02X %02X %02X %02X",
                     glyph_data[0], glyph_data[1], glyph_data[2], glyph_data[3]);
        }

        // Schreibe Pixel - WICHTIG: Koordinatenumrechnung für gedrehtes Display
        // Physikalische Position (x, y) → RAM Position
        // Das Display ist um 90° gedreht: phys_x → ram_y, phys_y → ram_x
        for (uint32_t row = 0; row < font->Height && y_pos + row < EPD_HEIGHT; row++)
        {
            for (uint32_t col = 0; col < font->Width && x_pos + col < EPD_WIDTH; col++)
            {
                uint32_t byte_idx = row * ((font->Width + 7) / 8) + col / 8;
                uint32_t bit_idx = 7 - (col % 8);

                uint8_t pixel = (glyph_data[byte_idx] >> bit_idx) & 1;

                if (pixel)
                {
                    // Koordinaten-Transformation: 90° Drehung
                    // phys(x,y) -> ram(y, width-1-x)
                    // uint32_t ram_x = y_pos + row;
                    uint32_t ram_x = (EPD_RAM_WIDTH - 1) - (y_pos + row);
                    uint32_t ram_y = x_pos + col;

                    // Prüfe Grenzen im RAM-Koordinatensystem
                    if (ram_x < EPD_RAM_WIDTH && ram_y < EPD_RAM_HEIGHT)
                    {
                        uint32_t fb_idx = ram_y * (EPD_RAM_WIDTH / 8) + ram_x / 8;
                        uint32_t fb_bit = 7 - (ram_x % 8);
                        // uint32_t fb_bit = ram_x % 8;

                        if (fb_idx < sizeof(framebuffer))
                        {
                            if (color == EPD_BLACK)
                            {
                                framebuffer[fb_idx] &= ~(1 << fb_bit);
                            }
                            else
                            {
                                framebuffer[fb_idx] |= (1 << fb_bit);
                            }
                        }
                    }
                }
            }
        }

        x_pos += font->Width;
        text++;
        char_count++;
    }

    ESP_LOGI(TAG, "Drew %d characters", char_count);

    // Debug
    ESP_LOGI(TAG, "FB after text - bytes 400-407:");
    for (int i = 400; i < 408 && i < sizeof(framebuffer); i++)
    {
        printf("%02X ", framebuffer[i]);
    }
    printf("\n");
}