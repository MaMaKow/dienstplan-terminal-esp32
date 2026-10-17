#include "nfc_reader.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#define NFC_CS 27
#define NFC_RST 26
#define NFC_SPI_HOST VSPI_HOST

// MFRC522 register addresses (the low address byte is shifted for SPI).
#define CommandReg 0x01
#define ComIEnReg 0x02
#define ComIrqReg 0x04
#define ErrorReg 0x06
#define FIFODataReg 0x09
#define FIFOLevelReg 0x0A
#define ControlReg 0x0C
#define BitFramingReg 0x0D
#define ModeReg 0x11
#define TxControlReg 0x14
#define TxASKReg 0x15
#define TModeReg 0x2A
#define TPrescalerReg 0x2B
#define TReloadRegH 0x2C
#define TReloadRegL 0x2D
#define VersionReg 0x37

#define PCD_IDLE 0x00
#define PCD_TRANSCEIVE 0x0C
#define PICC_REQIDL 0x26
#define PICC_ANTICOLL 0x93

static const char *TAG = "NFC";
static spi_device_handle_t nfc_spi;
static QueueHandle_t nfc_event_queue;
static bool reader_started;
static TickType_t last_diag_tick;
static const char *last_failure;
static bool queue_full_warning_active;

static esp_err_t register_transfer(uint8_t address, uint8_t *value, bool write)
{
    uint8_t tx[2] = {
        (uint8_t)(((address << 1) & 0x7E) | (write ? 0x00 : 0x80)),
        write ? *value : 0x00,
    };
    uint8_t rx[2] = {0};
    spi_transaction_t transaction = {
        .length = 16,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    esp_err_t err = spi_device_polling_transmit(nfc_spi, &transaction);
    if (err == ESP_OK && !write) {
        *value = rx[1];
    }
    return err;
}

static void write_reg(uint8_t reg, uint8_t value)
{
    (void)register_transfer(reg, &value, true);
}

static uint8_t read_reg(uint8_t reg)
{
    uint8_t value = 0;
    (void)register_transfer(reg, &value, false);
    return value;
}

static void set_bits(uint8_t reg, uint8_t mask)
{
    write_reg(reg, read_reg(reg) | mask);
}

static void clear_bits(uint8_t reg, uint8_t mask)
{
    write_reg(reg, read_reg(reg) & (uint8_t)~mask);
}

static void antenna_on(void)
{
    if ((read_reg(TxControlReg) & 0x03) != 0x03) {
        set_bits(TxControlReg, 0x03);
    }
}

static void mfrc522_init(void)
{
    write_reg(CommandReg, 0x0F); // Soft reset
    vTaskDelay(pdMS_TO_TICKS(50));
    write_reg(TModeReg, 0x8D);
    write_reg(TPrescalerReg, 0x3E);
    write_reg(TReloadRegL, 30);
    write_reg(TReloadRegH, 0);
    write_reg(TxASKReg, 0x40);
    write_reg(ModeReg, 0x3D);
    antenna_on();
}

// Sends a short ISO14443A command and receives its answer.
static bool transceive(const char *phase, const uint8_t *send, size_t send_len,
                       uint8_t tx_last_bits,
                       uint8_t *receive, size_t receive_cap,
                       size_t *receive_len)
{
    write_reg(CommandReg, PCD_IDLE);
    write_reg(ComIrqReg, 0x7F);
    write_reg(FIFOLevelReg, 0x80);
    for (size_t i = 0; i < send_len; ++i) {
        uint8_t byte = send[i];
        (void)register_transfer(FIFODataReg, &byte, true);
    }
    write_reg(BitFramingReg, tx_last_bits & 0x07);
    write_reg(CommandReg, PCD_TRANSCEIVE);
    set_bits(BitFramingReg, 0x80);

    int timeout = 40; // 40 x 1 ms
    uint8_t irq = 0;
    do {
        vTaskDelay(pdMS_TO_TICKS(1));
        irq = read_reg(ComIrqReg);
        if (irq & 0x01) {
            break;
        }
    } while (--timeout > 0 && !(irq & 0x30));
    clear_bits(BitFramingReg, 0x80);

    uint8_t errors = read_reg(ErrorReg);
    if (timeout <= 0 || !(irq & 0x30) || (errors & 0x1B)) {
        TickType_t now = xTaskGetTickCount();
        if (last_failure == NULL || strcmp(last_failure, phase) != 0 ||
            (TickType_t)(now - last_diag_tick) >= pdMS_TO_TICKS(2000)) {
            ESP_LOGW(TAG, "%s fehlgeschlagen: IRQ=0x%02X ErrorReg=0x%02X FIFO=%u timeout=%d",
                     phase, irq, errors, read_reg(FIFOLevelReg), timeout);
            last_failure = phase;
            last_diag_tick = now;
        }
        return false;
    }
    size_t count = read_reg(FIFOLevelReg);
    if (count > receive_cap) {
        ESP_LOGW(TAG, "%s: Antwort zu gross (%u Bytes, Puffer %u)",
                 phase, (unsigned)count, (unsigned)receive_cap);
        return false;
    }
    for (size_t i = 0; i < count; ++i) {
        receive[i] = read_reg(FIFODataReg);
    }
    *receive_len = count;
    return true;
}

// Supports common 4-byte UIDs (cascade level 1), as used by typical test tags.
static bool read_uid(uint8_t uid[4])
{
    uint8_t request[] = {PICC_REQIDL};
    uint8_t response[5];
    size_t response_len = 0;
    if (!transceive("REQA", request, sizeof(request), 7,
                    response, sizeof(response), &response_len) ||
        response_len < 2) {
        return false;
    }

    uint8_t anticollision[] = {PICC_ANTICOLL, 0x20};
    if (!transceive("Antikollision", anticollision, sizeof(anticollision), 0, response,
                    sizeof(response), &response_len) || response_len != 5) {
        return false;
    }
    if ((uint8_t)(response[0] ^ response[1] ^ response[2] ^ response[3]) != response[4]) {
        ESP_LOGW(TAG, "UID-Pruefsumme ungueltig");
        return false;
    }
    memcpy(uid, response, 4);
    last_failure = NULL;
    return true;
}

static void nfc_task(void *arg)
{
    (void)arg;
    uint8_t displayed_uid[4] = {0};
    bool have_displayed_uid = false;
    bool card_present = false;
    bool absence_timer_running = false;
    TickType_t absence_started = 0;
    const TickType_t removal_delay = pdMS_TO_TICKS(10000);

    while (true) {
        uint8_t uid[4];
        if (read_uid(uid)) {
            // Jede erfolgreiche UID-Lesung setzt das durchgehende 10-Sekunden-Fenster zurück.
            card_present = true;
            absence_timer_running = false;
            if (!have_displayed_uid || memcmp(uid, displayed_uid, sizeof(uid)) != 0) {
                nfc_event_t event;
                memcpy(event.uid, uid, sizeof(event.uid));
                if (xQueueSend(nfc_event_queue, &event, 0) != pdTRUE) {
                    if (!queue_full_warning_active) {
                        ESP_LOGW(TAG, "Event-Queue voll; UID wird erneut versucht");
                        queue_full_warning_active = true;
                    }
                } else {
                    ESP_LOGI(TAG, "Lesen abgeschlossen, UID=%02X%02X%02X%02X",
                             uid[0], uid[1], uid[2], uid[3]);
                    memcpy(displayed_uid, uid, sizeof(displayed_uid));
                    have_displayed_uid = true;
                    queue_full_warning_active = false;
                }
            }
        } else {
            TickType_t now = xTaskGetTickCount();
            if (!absence_timer_running) {
                absence_started = now;
                absence_timer_running = true;
            } else if (card_present &&
                       (TickType_t)(now - absence_started) >= removal_delay) {
                card_present = false;
                ESP_LOGI(TAG, "Karte seit mindestens 10 Sekunden nicht lesbar; als entfernt gewertet");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(150));
    }
}

esp_err_t nfc_reader_start(void)
{
    if (reader_started) {
        ESP_LOGE(TAG, "nfc_reader_start() wurde bereits aufgerufen");
        return ESP_ERR_INVALID_STATE;
    }
    gpio_config_t reset_config = {
        .pin_bit_mask = 1ULL << NFC_RST,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&reset_config));
    gpio_set_level(NFC_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(NFC_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    spi_device_interface_config_t config = {
        .clock_speed_hz = 1000000,
        .mode = 0,
        .spics_io_num = NFC_CS,
        .queue_size = 1,
    };
    esp_err_t err = spi_bus_add_device(NFC_SPI_HOST, &config, &nfc_spi);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "MFRC522 konnte nicht am SPI-Bus registriert werden: %s",
                 esp_err_to_name(err));
        return err;
    }

    mfrc522_init();
    uint8_t version = read_reg(VersionReg);
    ESP_LOGI(TAG, "MFRC522 VersionReg=0x%02X TxControl=0x%02X", version,
             read_reg(TxControlReg));
    if (version == 0x00 || version == 0xFF) {
        ESP_LOGE(TAG, "Keine gueltige MFRC522-Antwort; Verdrahtung pruefen");
        spi_bus_remove_device(nfc_spi);
        nfc_spi = NULL;
        return ESP_FAIL;
    }
    nfc_event_queue = xQueueCreate(5, sizeof(nfc_event_t));
    if (nfc_event_queue == NULL) {
        spi_bus_remove_device(nfc_spi);
        nfc_spi = NULL;
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "Initialisierung abgeschlossen; Polling startet (CS=GPIO%d, RST=GPIO%d, SPI=1 MHz)",
             NFC_CS, NFC_RST);
    BaseType_t created = xTaskCreate(nfc_task, "nfc_reader", 4096, NULL, 5, NULL);
    if (created != pdPASS) {
        vQueueDelete(nfc_event_queue);
        nfc_event_queue = NULL;
        spi_bus_remove_device(nfc_spi);
        nfc_spi = NULL;
        return ESP_ERR_NO_MEM;
    }
    reader_started = true;
    return ESP_OK;
}

esp_err_t nfc_reader_wait_for_uid(nfc_event_t *event, TickType_t timeout)
{
    if (!reader_started) {
        return ESP_ERR_INVALID_STATE;
    }
    if (event == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return xQueueReceive(nfc_event_queue, event, timeout) == pdTRUE
               ? ESP_OK
               : ESP_ERR_TIMEOUT;
}
