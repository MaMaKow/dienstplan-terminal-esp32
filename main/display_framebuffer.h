#pragma once

#include <stddef.h>
#include <stdint.h>

#include "fonts.h"

// Sichtbare Display-Abmessungen in Pixeln.
#define EPD_WIDTH 296
#define EPD_HEIGHT 128

#define EPD_WHITE 0xFF
#define EPD_BLACK 0x00

// Der Puffer folgt dem RAM-Layout des um 90 Grad gedrehten SSD1680.
#define EPD_FRAMEBUFFER_SIZE (128 * 296 / 8)

void epd_fb_clear(uint8_t color);
void epd_fb_draw_pixel(int x, int y, uint8_t color);
uint8_t epd_fb_get_pixel(int x, int y);
void epd_fb_draw_rectangle(int x, int y, int width, int height, uint8_t color);
void epd_fb_fill_rectangle(int x, int y, int width, int height, uint8_t color);
void epd_fb_draw_string(int x, int y, const char *text,
                        const sFONT *font, uint8_t color);
void epd_fb_test_pattern(void);
void epd_fb_test_checkerboard(void);

// Zugriff für den Hardwaretreiber oder einen Host-seitigen Bildexport.
const uint8_t *epd_fb_data(void);
size_t epd_fb_size(void);
