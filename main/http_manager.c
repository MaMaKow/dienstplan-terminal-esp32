#include "http_manager.h"
#include "http_config.h" // Wird erst durch tools/mock-server/start.sh geschrieben @TODO Später durch echte Konfiguration ersetzen!
#include "lwip/inet.h"   // inet_ntoa_r, ip4_addr_t
#include "lwip/netdb.h"  // getaddrinfo, freeaddrinfo, struct addrinfo
#include "lwip/sockets.h" // struct sockaddr_in, sockaddr_storage

#include <string.h>
#include <sys/param.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define HTTP_QUEUE_LEN 8
#define HTTP_TASK_STACK 8192 // TLS (mbedTLS) braucht deutlich mehr als 4096

static const char *TAG = "HTTP_MANAGER";

extern const char dev_ca_pem_start[] asm("_binary_dev_ca_pem_start");

static QueueHandle_t s_queue;
static esp_http_client_handle_t s_client;
static http_response_t s_resp; // nur vom http_task benutzt -> kein Mutex nötig

/* Sammelt den Response-Body in s_resp (über config.user_data). */
static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
  http_response_t *r = (http_response_t *)evt->user_data;
  if (evt->event_id == HTTP_EVENT_ON_DATA && r != NULL) {
    size_t space = sizeof(r->body) - 1 - r->body_len; // 1 Byte für '\0'
    size_t n = MIN((size_t)evt->data_len, space);
    if (n > 0) {
      memcpy(r->body + r->body_len, evt->data, n);
      r->body_len += n;
      r->body[r->body_len] = '\0';
    }
  }
  return ESP_OK;
}

static void http_task(void *arg) {
  (void)arg;
  http_request_t req;

  while (true) {
    // Blockiert, bis ein Eintrag da ist: keine CPU-Last, kein Polling.
    if (xQueueReceive(s_queue, &req, portMAX_DELAY) != pdTRUE) {
      continue;
    }

    memset(&s_resp, 0, sizeof(s_resp));

    // Beginnt die URL mit '/', wird nur der Pfad ersetzt; Host, Port und TLS
    // bleiben aus der Config von http_manager_start().
    esp_http_client_set_url(s_client, req.path);
    esp_http_client_set_method(s_client, req.method);
    if (req.post_body[0] != '\0') {
      // esp_http_client_set_header(s_client, "Content-Type",
      // "application/json");
      esp_http_client_set_header(s_client, "Content-Type",
                                 "application/x-www-form-urlencoded");
      esp_http_client_set_post_field(s_client, req.post_body,
                                     strlen(req.post_body));
    } else {
      esp_http_client_set_post_field(s_client, NULL, 0);
    }

    s_resp.err = esp_http_client_perform(s_client);
    if (s_resp.err == ESP_OK) {
      s_resp.status = esp_http_client_get_status_code(s_client);
      ESP_LOGI(TAG, "%s -> HTTP %d (%u Bytes)", req.path, s_resp.status,
               (unsigned)s_resp.body_len);
    } else {
      ESP_LOGE(TAG, "%s fehlgeschlagen: %s", req.path,
               esp_err_to_name(s_resp.err));
    }

    // Läuft im http_task: kurz halten, nichts Langsames (z.B. E-Paper) direkt
    // hier machen, sondern ein Event/eine Queue an den UI-Task schicken.
    if (req.cb != NULL) {
      req.cb(&s_resp, req.cb_ctx);
    }
  }
}

esp_err_t http_manager_start(void) {
  if (s_queue != NULL) {
    return ESP_ERR_INVALID_STATE;
  }

  s_queue = xQueueCreate(HTTP_QUEUE_LEN, sizeof(http_request_t));
  if (s_queue == NULL) {
    return ESP_ERR_NO_MEM;
  }

  esp_http_client_config_t config = {
      .host = HTTP_SERVER_HOST,
      .port = HTTP_SERVER_PORT,
      .path = "/",
      .transport_type = HTTP_TRANSPORT_OVER_SSL,
      .event_handler = http_event_handler,
      .user_data = &s_resp,
      .cert_pem = dev_ca_pem_start,
      .timeout_ms = 5000,
  };
  s_client = esp_http_client_init(&config);
  if (s_client == NULL) {
    vQueueDelete(s_queue);
    s_queue = NULL;
    return ESP_FAIL;
  }

  if (xTaskCreate(http_task, "http", HTTP_TASK_STACK, NULL, 3, NULL) !=
      pdPASS) {
    esp_http_client_cleanup(s_client);
    vQueueDelete(s_queue);
    s_client = NULL;
    s_queue = NULL;
    return ESP_ERR_NO_MEM;
  }
  return ESP_OK;
}

esp_err_t http_manager_enqueue(const http_request_t *req, TickType_t timeout) {
  if (s_queue == NULL || req == NULL) {
    return ESP_ERR_INVALID_STATE;
  }
  // xQueueSend kopiert sizeof(http_request_t) Bytes in die Queue.
  return xQueueSend(s_queue, req, timeout) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}
