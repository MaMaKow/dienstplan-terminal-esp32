#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

typedef struct {
    uint8_t uid[4];
} nfc_event_t;

// Muss nach epd_init() aufgerufen werden, da beide Geräte den SPI-Bus teilen.
esp_err_t nfc_reader_start(void);

// Wartet bis zu timeout auf eine neu erkannte UID.
esp_err_t nfc_reader_wait_for_uid(nfc_event_t *event, TickType_t timeout);
