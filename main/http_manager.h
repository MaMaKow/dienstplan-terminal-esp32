#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"

#define HTTP_PATH_MAX 128
#define HTTP_BODY_MAX 256
#define HTTP_RESP_MAX 2048

/* Ergebnis eines Requests. Der Zeiger im Callback ist nur während des
 * Callbacks gültig -> bei Bedarf kopieren. */
typedef struct {
  esp_err_t err; // Transportfehler (ESP_OK = Antwort erhalten)
  int status;    // HTTP-Statuscode
  size_t body_len;
  char body[HTTP_RESP_MAX];
} http_response_t;

typedef void (*http_callback_t)(const http_response_t *resp, void *ctx);

/* Ein Queue-Eintrag. Wird beim Einreihen BY VALUE in die Queue kopiert,
 * daher feste Puffer statt Zeigern auf Stack-Variablen des Aufrufers. */
typedef struct {
  char path[HTTP_PATH_MAX]; // z.B. "/v1/device-auth/token/"
  esp_http_client_method_t method;
  char post_body[HTTP_BODY_MAX]; // leer = kein Body
  http_callback_t cb;            // darf NULL sein
  void *cb_ctx;                  // muss die Lebensdauer des Requests überdauern
} http_request_t;

/* Queue + HTTP-Client + Task anlegen. Einmal aufrufen (nach nvs/wifi_init). */
esp_err_t http_manager_start(void);

/* Request einreihen. timeout=0 -> sofort ESP_ERR_TIMEOUT, wenn die Queue voll
 * ist. Aus jedem Task aufrufbar. */
esp_err_t http_manager_enqueue(const http_request_t *req, TickType_t timeout);
#ifdef __cplusplus
}
#endif
