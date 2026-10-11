#include "epaper_manager.h"
#include <stdbool.h>
#include <string.h>

static const char *TAG = "EPAPER";
// #include "epd_2in9.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "freertos/task.h"

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
static uint8_t last_command;
// SSD1680 Commands
#define CMD_DRIVER_OUTPUT_CONTROL 0x01
#define CMD_BOOSTER_SOFT_START 0x0C
#define CMD_GATE_SCAN_START_POSITION 0x0F
#define CMD_DEEP_SLEEP 0x10
#define CMD_DATA_ENTRY_MODE_SETTING 0x11
#define CMD_SW_RESET 0x12
#define CMD_TEMPERATURE_SENSOR_CONTROL 0x18
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
#define CMD_WRITE_OLD_RAM 0x26
#define CMD_WRITE_LUT 0x32
#define CMD_WRITE_OTP_SELECTION 0x37
#define EPD_PARTIAL_LUT_SIZE 159
#define EPD_FULL_REFRESH_INTERVAL 60
#define EPD_SPI_DATA_CHUNK_SIZE 1024

static uint8_t displayed_framebuffer[EPD_FRAMEBUFFER_SIZE];
static bool displayed_framebuffer_valid;
static unsigned partial_refresh_count;

// SSD1680 partial waveform for the Waveshare 2.9-inch B/W panel family.
static const uint8_t partial_lut[EPD_PARTIAL_LUT_SIZE] = {
    0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x40, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00, 0x22, 0x17, 0x41,
    0xB0, 0x32, 0x36};

static esp_err_t epd_spi_transmit(const void *data, size_t len) {
  spi_transaction_t t = {
      .length = len * 8,
      .tx_buffer = data,
  };
  const esp_err_t err = spi_device_polling_transmit(spi, &t);
  return err;
}

static void epd_send_command(uint8_t cmd) {
  gpio_set_level(PIN_DC, 0); // DC = 0 für Befehl
  (void)epd_spi_transmit(&cmd, sizeof(cmd));
  last_command = cmd;
}

static void epd_send_data(uint8_t data) {
  gpio_set_level(PIN_DC, 1); // DC = 1 für Daten

  (void)epd_spi_transmit(&data, sizeof(data));
}

static bool epd_send_data_buffer(const uint8_t *data, size_t len) {
  gpio_set_level(PIN_DC, 1); // DC = 1 für Daten

  size_t offset = 0;
  while (offset < len) {
    const size_t chunk_size = (len - offset > EPD_SPI_DATA_CHUNK_SIZE)
                                  ? EPD_SPI_DATA_CHUNK_SIZE
                                  : len - offset;
    const esp_err_t err = epd_spi_transmit(data + offset, chunk_size);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "SPI data transfer failed at %u/%u bytes: %s",
               (unsigned)offset, (unsigned)len, esp_err_to_name(err));
      return false;
    }
    offset += chunk_size;
  }
  return true;
}

static void epd_wait_busy(const char *phase, bool expect_busy_assertion) {
  const TickType_t start_ticks = xTaskGetTickCount();
  const int initial_level = gpio_get_level(PIN_BUSY);
  int timeout = 1000; // 10 seconds timeout
  bool busy_asserted = initial_level == 1;

  ESP_LOGI(TAG, "BUSY start: phase=%s level=%d expect_assertion=%d", phase,
           initial_level, expect_busy_assertion);

  // A newly started controller sequence may assert BUSY a short time after
  // command 0x20. Wait for that edge so an immediate low sample is not
  // mistaken for a completed refresh.
  int assertion_waits = 10; // 10 x 10 ms = 100 ms
  while (expect_busy_assertion && !busy_asserted && assertion_waits > 0) {
    vTaskDelay(pdMS_TO_TICKS(10));
    --assertion_waits;
    busy_asserted = gpio_get_level(PIN_BUSY) == 1;
  }

  const TickType_t asserted_ticks = xTaskGetTickCount();
  const unsigned asserted_after_ms =
      (unsigned)((asserted_ticks - start_ticks) * portTICK_PERIOD_MS);
  if (expect_busy_assertion && !busy_asserted) {
    ESP_LOGW(TAG, "BUSY never asserted: phase=%s initial=%d wait=%ums", phase,
             initial_level, asserted_after_ms);
    vTaskDelay(pdMS_TO_TICKS(10));
    return;
  }

  while (gpio_get_level(PIN_BUSY) == 1 && timeout > 0) {
    vTaskDelay(pdMS_TO_TICKS(10));
    timeout--;
  }

  if (timeout <= 0) {
    ESP_LOGW(
        TAG,
        "BUSY timeout: phase=%s asserted_after=%ums high_for_at_least=%ums",
        phase, asserted_after_ms, (unsigned)(1000 * 10));
  } else {
    const TickType_t end_ticks = xTaskGetTickCount();
    const unsigned total_ms =
        (unsigned)((end_ticks - start_ticks) * portTICK_PERIOD_MS);
    ESP_LOGI(
        TAG,
        "BUSY released: phase=%s asserted=%d asserted_after=%ums total=%ums",
        phase, busy_asserted, asserted_after_ms, total_ms);
  }

  vTaskDelay(pdMS_TO_TICKS(10));
}

static void epd_reset(void) {
  ESP_LOGD(TAG, "Display reset");
  gpio_set_level(PIN_RST, 1);
  vTaskDelay(pdMS_TO_TICKS(200));
  gpio_set_level(PIN_RST, 0);
  vTaskDelay(pdMS_TO_TICKS(10));
  gpio_set_level(PIN_RST, 1);
  vTaskDelay(pdMS_TO_TICKS(200));
}

void epd_init(void) {
  ESP_LOGI(TAG, "Initializing e-Paper display 2.9\"");

  // GPIO Konfiguration
  gpio_config_t io_conf = {.mode = GPIO_MODE_OUTPUT,
                           .pin_bit_mask = (1ULL << PIN_CS) | (1ULL << PIN_DC) |
                                           (1ULL << PIN_RST),
                           .pull_down_en = 0,
                           .pull_up_en = 0,
                           .intr_type = GPIO_INTR_DISABLE};
  gpio_config(&io_conf);

  gpio_config_t busy_conf = {.mode = GPIO_MODE_INPUT,
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
  epd_wait_busy("init-hw-reset", false);

  // Software Reset
  epd_send_command(CMD_SW_RESET);
  epd_wait_busy("init-sw-reset", true);

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
  epd_send_data(0x00);                        // Start LSB
  epd_send_data(0x00);                        // Start MSB
  epd_send_data((EPD_RAM_HEIGHT - 1) & 0xFF); // End LSB = 295 & 0xFF = 0x27
  epd_send_data(((EPD_RAM_HEIGHT - 1) >> 8) &
                0xFF); // End MSB = 295 >> 8 = 0x01

  // Border Waveform Control
  epd_send_command(CMD_BORDER_WAVEFORM_CONTROL);
  epd_send_data(0x05);

  // Temperature Sensor
  epd_send_command(CMD_TEMPERATURE_SENSOR_CONTROL);
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

  epd_wait_busy("init-config", false);

  ESP_LOGI(TAG, "e-Paper display initialized");
  ESP_LOGI(TAG, "Framebuffer size: %u bytes", (unsigned)epd_fb_size());
}

void epd_clear(uint8_t color) {
  ESP_LOGI(TAG, "Clearing framebuffer with color 0x%02X", color);
  epd_fb_clear(color);
}

static void epd_set_full_window(void) {
  epd_send_command(CMD_SET_RAM_X_ADDRESS_START_END);
  epd_send_data(0x00);
  epd_send_data((EPD_RAM_WIDTH / 8) - 1);
  epd_send_command(CMD_SET_RAM_Y_ADDRESS_START_END);
  epd_send_data(0x00);
  epd_send_data(0x00);
  epd_send_data((EPD_RAM_HEIGHT - 1) & 0xFF);
  epd_send_data((EPD_RAM_HEIGHT - 1) >> 8);
  epd_send_command(CMD_SET_RAM_X_ADDRESS_COUNTER);
  epd_send_data(0x00);
  epd_send_command(CMD_SET_RAM_Y_ADDRESS_COUNTER);
  epd_send_data(0x00);
  epd_send_data(0x00);
}

void epd_display(void) {
  const uint8_t *framebuffer = epd_fb_data();
  const size_t framebuffer_size = epd_fb_size();
  bool changed = false;

  for (size_t i = 0; i < framebuffer_size; ++i) {
    if (!displayed_framebuffer_valid ||
        framebuffer[i] != displayed_framebuffer[i]) {
      changed = true;
      break;
    }
  }
  if (!changed)
    return;

  const bool full_refresh =
      !displayed_framebuffer_valid ||
      partial_refresh_count >= EPD_FULL_REFRESH_INTERVAL - 1;
  if (full_refresh) {
    epd_set_full_window();
    epd_send_command(CMD_WRITE_RAM_BW);
    if (!epd_send_data_buffer(framebuffer, framebuffer_size))
      return;
    // The SSD1680 swaps RAM banks after refresh, so both must hold this frame.
    epd_set_full_window();
    epd_send_command(CMD_WRITE_OLD_RAM);
    if (!epd_send_data_buffer(framebuffer, framebuffer_size))
      return;
    epd_send_command(CMD_DISPLAY_UPDATE_CONTROL_2);
    epd_send_data(0xF7);
    epd_send_command(CMD_MASTER_ACTIVATION);
    epd_wait_busy("full-refresh", true);
    partial_refresh_count = 0;
  } else {
    // SSD1680 quick refresh is based on a full old/new frame pair. The
    // panel waveform drives only differing pixels; a 0x90 window alone
    // is not the supported partial-refresh mechanism for this module.
    epd_set_full_window();
    epd_send_command(CMD_WRITE_OLD_RAM);
    if (!epd_send_data_buffer(displayed_framebuffer, framebuffer_size))
      return;

    epd_reset();
    epd_wait_busy("partial-reset", false);
    epd_send_command(CMD_WRITE_LUT);
    if (!epd_send_data_buffer(partial_lut, sizeof(partial_lut)))
      return;
    epd_send_command(CMD_WRITE_OTP_SELECTION);
    const uint8_t otp_selection[10] = {0x00, 0x00, 0x00, 0x00, 0x00,
                                       0x40, 0x00, 0x00, 0x00, 0x00};
    if (!epd_send_data_buffer(otp_selection, sizeof(otp_selection)))
      return;
    epd_send_command(CMD_BORDER_WAVEFORM_CONTROL);
    epd_send_data(0x80);
    epd_send_command(CMD_DISPLAY_UPDATE_CONTROL_2);
    epd_send_data(0xC0);
    epd_send_command(CMD_MASTER_ACTIVATION);
    epd_wait_busy("partial-prepare", true);

    epd_set_full_window();
    epd_send_command(CMD_WRITE_RAM_BW);
    if (!epd_send_data_buffer(framebuffer, framebuffer_size))
      return;
    epd_send_command(CMD_DISPLAY_UPDATE_CONTROL_2);
    epd_send_data(0x0F);
    epd_send_command(CMD_MASTER_ACTIVATION);
    epd_wait_busy("partial-refresh", true);
    ++partial_refresh_count;
  }

  memcpy(displayed_framebuffer, framebuffer, framebuffer_size);
  displayed_framebuffer_valid = true;
  ESP_LOGI(TAG, "%s refresh complete (partial count: %u)",
           full_refresh ? "Full" : "Quick", partial_refresh_count);
}

void epd_test_pattern(void) { epd_fb_test_pattern(); }

void epd_test_checkerboard(void) { epd_fb_test_checkerboard(); }

void epd_draw_string(int x, int y, const char *text, const sFONT *font,
                     uint8_t color) {
  epd_fb_draw_string(x, y, text, font, color);
}
