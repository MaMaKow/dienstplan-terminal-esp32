/*
 * Dieses Modul ist nicht thread-safe; Aufrufe müssen aus einem Kontext
 * erfolgen.
 *
 * ledc_timer_config()	Legt die PWM-Frequenz (z. B. 1000 Hz für einen hörbaren
 * Ton) und die Auflösung (z. B. 8 Bit) fest. ledc_channel_config()
 * Verbindet den Timer mit einem GPIO-Pin und legt den Start-Duty-Cycle fest
 * (oft 0). ledc_set_duty() + ledc_update_duty()	Setzt den Duty-Cycle auf
 * 50 % (z. B. 0x7F bei 8 Bit) und aktiviert die Änderung. Der Buzzer ertönt.
 * ledc_stop() oder ledc_set_duty(..., 0) + ledc_update_duty()	Deaktiviert die
 * Ausgabe und setzt den Pegel auf einen definierten Ruhezustand.
 */
#include "buzzer.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
static gpio_num_t s_pin;
static size_t s_current_index;  // aktueller Schritt
static size_t s_current_length; // Anzahl Schritte der Sequenz
static int64_t s_next_step_us;  // Deadline in esp_timer-Mikrosekunden
static bool s_initialized = false;
static volatile int s_pending_sound = -1;
static const char *TAG = "DIENSTPLAN_TERMINAL_BUZZER";
#define BUZZER_CHANNEL LEDC_CHANNEL_0
#define BUZZER_TIMER LEDC_TIMER_0
#define BUZZER_DUTY_ON (0x05) // 0x7F = ~50 % bei LEDC_TIMER_8_BIT
typedef struct {
  uint16_t freq_hz;     // 0 = Pause (kein Ton)
  uint16_t duration_ms; // Dauer dieses Schritts
} buzzer_tone_t;
static const buzzer_tone_t *s_current_seq; // NULL = nichts läuft
static const buzzer_tone_t s_seq_card_read[] = {
    {1800, 100},
};

static const buzzer_tone_t s_seq_success[] = {
    {1400, 100}, {0, 150}, {1800, 100}, {0, 50}, {2400, 300},
};

static const buzzer_tone_t s_seq_failure[] = {
    {900, 180},
    {0, 50},
    {500, 360},
};
typedef struct {
  const buzzer_tone_t *tones;
  size_t length;
} buzzer_sequence_t;

static const buzzer_sequence_t s_sequences[] = {
    [BUZZER_SOUND_CARD_READ] = {s_seq_card_read,
                                sizeof(s_seq_card_read) /
                                    sizeof(s_seq_card_read[0])},
    [BUZZER_SOUND_SUCCESS] = {s_seq_success,
                              sizeof(s_seq_success) / sizeof(s_seq_success[0])},
    [BUZZER_SOUND_FAILURE] = {s_seq_failure,
                              sizeof(s_seq_failure) / sizeof(s_seq_failure[0])},
};
// LEDC-Setup, PWM vorbereiten, zunächst stumm
esp_err_t buzzer_init(gpio_num_t pin) {
  s_pin = pin;

  // 1. Timer konfigurieren: 1000 Hz, 8-Bit-Auflösung
  ledc_timer_config_t timer_conf = {.speed_mode = LEDC_LOW_SPEED_MODE,
                                    .timer_num = BUZZER_TIMER,
                                    .duty_resolution = LEDC_TIMER_8_BIT,
                                    .freq_hz = 1000,
                                    .clk_cfg = LEDC_AUTO_CLK};
  esp_err_t err_timer = ledc_timer_config(&timer_conf);
  if (ESP_OK != err_timer) {
    return err_timer;
  }
  // 2. Kanal konfigurieren: GPIO, Start-Duty = 0 (aus)
  ledc_channel_config_t channel_conf = {.gpio_num = s_pin,
                                        .speed_mode = LEDC_LOW_SPEED_MODE,
                                        .channel = BUZZER_CHANNEL,
                                        .timer_sel = BUZZER_TIMER,
                                        .duty = 0,
                                        .hpoint = 0};
  esp_err_t err_channel = ledc_channel_config(&channel_conf);
  if (ESP_OK != err_channel) {
    return err_channel;
  }
  ledc_fade_func_install(0); // 0 = keine besonderen Flags
  s_initialized = true;
  return ESP_OK;
}
static void s_output_step(const buzzer_tone_t *step) {
  esp_err_t freq_err = ESP_OK;
  if (0 == step->freq_hz) {
    // Pause: nur abschalten, Frequenz unverändert lassen
    ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, BUZZER_CHANNEL, 0, 0);
  } else {
    // Ton: erst Frequenz, dann Duty (vermeidet Tonreste)
    // Verzichte auf Rückgabewerte der Funktion. Frequenzen sind statisch
    // validiert, Fehler hier sind Programmierfehler keine Laufzeitfehler.
    freq_err = ledc_set_freq(LEDC_LOW_SPEED_MODE, BUZZER_TIMER, step->freq_hz);
    ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, BUZZER_CHANNEL,
                             BUZZER_DUTY_ON, 0);
  }
  s_next_step_us = esp_timer_get_time() + (int64_t)step->duration_ms * 1000;
}
static void s_start_sequence(buzzer_sound_t sound) {
  const buzzer_tone_t *tones = s_sequences[sound].tones;
  s_current_length = s_sequences[sound].length;
  s_current_index = 0;
  s_output_step(&tones[0]);
  s_current_seq = tones;
}

// Sequenz starten (überschreibt ggf. laufende Töne)
void buzzer_play(buzzer_sound_t sound) {
  if (true != s_initialized) {
    return;
  }
  if (sound < 0 ||
      sound >= (int)(sizeof(s_sequences) / sizeof(s_sequences[0]))) {
    return;
  }
  s_pending_sound = (int)sound;
}

// aus Hauptschleife oder eigenem Task periodisch aufrufen
void buzzer_update(void) {
  int pending = s_pending_sound;
  if (pending >= 0) {
    s_pending_sound = -1;
    s_start_sequence((buzzer_sound_t)pending);
    return;
  }
  if (!buzzer_is_playing()) {
    // ESP_LOGI(TAG, "Buzzer is not playing, return.");
    return;
  }
  if (esp_timer_get_time() < s_next_step_us) {
    return;
  }
  s_current_index++;
  if (s_current_index >= s_current_length) {
    ESP_LOGI(TAG, "End of Index, no more sound to play.");
    buzzer_stop();
    return;
  }
  s_output_step(&s_current_seq[s_current_index]);
}
// laufende Sequenz abbrechen (z. B. bei Timeout/Ruhezustand)
void buzzer_stop(void) {
  // Setze Duty = 0 zum Ausschalten
  // 4. Ton stoppen: Duty-Cycle auf 0 setzen und Ausgabe deaktivieren
  s_current_seq = NULL;
  ledc_set_duty(LEDC_LOW_SPEED_MODE, BUZZER_CHANNEL, 0);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, BUZZER_CHANNEL);
}

// nützlich: Zustandsautomaten-Entscheidungen (z. B. Karte gelesen → Auflösung
// läuft)
bool buzzer_is_playing(void) {
  if (NULL == s_current_seq) {
    return false;
  }
  return true;
}
