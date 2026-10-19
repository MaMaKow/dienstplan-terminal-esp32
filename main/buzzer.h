#pragma once
#include "driver/gpio.h"
#include "esp_err.h"
#include <stdbool.h>
typedef enum {
  BUZZER_SOUND_CARD_READ, // 1 heller Ton, ~50 ms, 1,8 kHz
  BUZZER_SOUND_SUCCESS, // 3 aufsteigende Töne, 1,4/1,8/2,2 kHz, je ~100 ms + 50
                        // ms Pause
  BUZZER_SOUND_FAILURE, // 2 tiefe Töne, 900/700 Hz, je ~180 ms + 100 ms Pause
  // BUZZER_SOUND_REJECTED,    // optionaler kurzer tiefer Ablehnungston
  // (später, Etappe 3)
} buzzer_sound_t;

esp_err_t
buzzer_init(gpio_num_t pin); // LEDC-Setup, PWM vorbereiten, zunächst stumm
void buzzer_play(
    buzzer_sound_t sound); // Sequenz starten (überschreibt ggf. laufende)
void buzzer_stop(
    void); // laufende Sequenz abbrechen (z. B. bei Timeout/Ruhezustand)
void buzzer_update(
    void); // aus Hauptschleife oder eigenem Task periodisch aufrufen
bool buzzer_is_playing(void); // nützlich: Zustandsautomaten-Entscheidungen (z.
                              // B. Karte gelesen → Auflösung läuft)
