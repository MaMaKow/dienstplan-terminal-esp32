#pragma once
#include <stdint.h>
#include "display_framebuffer.h"

// Shared clock layout for rendering.
#define EPD_TIME_X 10
#define EPD_TIME_Y 20
#define EPD_TIME_FONT Font48

void epd_init(void);
void epd_clear(uint8_t color);
void epd_display(void);

void epd_draw_string(int x, int y, const char *text,
                     const sFONT *font, uint8_t color);

// Testfunktionen
void epd_test_pattern(void);      // Horizontale Linien
void epd_test_checkerboard(void); // Schachbrettmuster
