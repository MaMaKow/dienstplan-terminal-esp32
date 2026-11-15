#pragma once
#include "esp_wifi.h"
#include <stdint.h>

esp_err_t softap_config_from_mac(wifi_config_t *ap_cfg);
