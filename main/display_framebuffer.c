#include "display_framebuffer.h"

#include <stdbool.h>
#include <string.h>

// SSD1680-RAM ist 128 x 296 Pixel, je acht x-Pixel pro Byte.
#define EPD_RAM_WIDTH 128
#define EPD_RAM_HEIGHT 296

static uint8_t framebuffer[EPD_FRAMEBUFFER_SIZE];

static bool get_fb_position(int x, int y, size_t *index, uint8_t *bit)
{
    if (x < 0 || x >= EPD_WIDTH || y < 0 || y >= EPD_HEIGHT) {
        return false;
    }

    // Physische Koordinaten (x,y) -> RAM-Koordinaten (127-y,x).
    const unsigned ram_x = (EPD_RAM_WIDTH - 1) - (unsigned)y;
    const unsigned ram_y = (unsigned)x;
    *index = ram_y * (EPD_RAM_WIDTH / 8) + ram_x / 8;
    *bit = (uint8_t)(7 - (ram_x % 8));
    return *index < sizeof(framebuffer);
}

void epd_fb_draw_pixel(int x, int y, uint8_t color)
{
    size_t index;
    uint8_t bit;
    if (!get_fb_position(x, y, &index, &bit)) {
        return;
    }

    if (color == EPD_BLACK) {
        framebuffer[index] &= (uint8_t)~(1U << bit);
    } else {
        framebuffer[index] |= (uint8_t)(1U << bit);
    }
}

uint8_t epd_fb_get_pixel(int x, int y)
{
    size_t index;
    uint8_t bit;
    if (!get_fb_position(x, y, &index, &bit)) {
        return EPD_WHITE;
    }
    return (framebuffer[index] & (1U << bit)) != 0 ? EPD_WHITE : EPD_BLACK;
}

void epd_fb_clear(uint8_t color)
{
    memset(framebuffer, color, sizeof(framebuffer));
}

void epd_fb_draw_rectangle(int x, int y, int width, int height, uint8_t color)
{
    if (width <= 0 || height <= 0) return;

    for (int col = 0; col < width; col++) {
        epd_fb_draw_pixel(x + col, y, color);
        epd_fb_draw_pixel(x + col, y + height - 1, color);
    }
    for (int row = 0; row < height; row++) {
        epd_fb_draw_pixel(x, y + row, color);
        epd_fb_draw_pixel(x + width - 1, y + row, color);
    }
}

void epd_fb_fill_rectangle(int x, int y, int width, int height, uint8_t color)
{
    if (width <= 0 || height <= 0) return;

    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            epd_fb_draw_pixel(x + col, y + row, color);
        }
    }
}

void epd_fb_draw_string(int x, int y, const char *text,
                        const sFONT *font, uint8_t color)
{
    if (font == NULL || text == NULL) return;

    const size_t text_length = strlen(text);
    const int text_width = (int)(text_length * font->Width);
    epd_fb_fill_rectangle(x, y, text_width, font->Height, EPD_WHITE);

    int x_pos = x;
    while (*text) {
        const unsigned char character = (unsigned char)*text++;
        if (character < ' ' || character >= (' ' + 95)) {
            x_pos += font->Width;
            continue;
        }

        const size_t bytes_per_row = (font->Width + 7) / 8;
        const uint8_t *glyph = font->table + (character - ' ') * font->Height * bytes_per_row;
        for (uint32_t row = 0; row < font->Height; row++) {
            for (uint32_t col = 0; col < font->Width; col++) {
                const size_t byte_index = row * bytes_per_row + col / 8;
                const uint8_t bit = (uint8_t)(7 - (col % 8));
                if ((glyph[byte_index] >> bit) & 1U) {
                    epd_fb_draw_pixel(x_pos + (int)col, y + (int)row, color);
                }
            }
        }
        x_pos += font->Width;
    }
}

void epd_fb_test_pattern(void)
{
    memset(framebuffer, EPD_WHITE, sizeof(framebuffer));
    for (int y = 0; y < EPD_RAM_HEIGHT; y += 16) {
        memset(&framebuffer[y * (EPD_RAM_WIDTH / 8)], EPD_BLACK, EPD_RAM_WIDTH / 8);
    }
}

void epd_fb_test_checkerboard(void)
{
    for (int y = 0; y < EPD_RAM_HEIGHT; y++) {
        for (int x = 0; x < EPD_RAM_WIDTH / 8; x++) {
            framebuffer[y * (EPD_RAM_WIDTH / 8) + x] =
                ((y / 8 + x) % 2 == 0) ? 0xAA : 0x55;
        }
    }
}

const uint8_t *epd_fb_data(void)
{
    return framebuffer;
}

size_t epd_fb_size(void)
{
    return sizeof(framebuffer);
}
