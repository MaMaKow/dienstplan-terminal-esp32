#pragma once
#include <stdint.h>
#include "fonts.h"

#define EPD_WHITE 0xFF
#define EPD_BLACK 0x00

void epd_init(void);
void epd_clear(uint8_t color);
void epd_display(void);

void epd_draw_string(int x, int y, const char *text,
                     const sFONT *font, uint8_t color);

// Testfunktionen
void epd_test_pattern(void);      // Horizontale Linien
void epd_test_checkerboard(void); // Schachbrettmuster