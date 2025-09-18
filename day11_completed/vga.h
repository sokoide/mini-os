#pragma once
#include <stdint.h>

typedef enum {
    VGA_BLACK = 0,
    VGA_BLUE = 1,
    VGA_GREEN = 2,
    VGA_CYAN = 3,
    VGA_RED = 4,
    VGA_MAGENTA = 5,
    VGA_BROWN = 6,
    VGA_LIGHT_GREY = 7,
    VGA_DARK_GREY = 8,
    VGA_LIGHT_BLUE = 9,
    VGA_LIGHT_GREEN = 10,
    VGA_LIGHT_CYAN = 11,
    VGA_LIGHT_RED = 12,
    VGA_LIGHT_MAGENTA = 13,
    VGA_YELLOW = 14,
    VGA_WHITE = 15
} vga_color_t;

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

void vga_set_color(vga_color_t f, vga_color_t b);
void vga_move_cursor(uint16_t x, uint16_t y);
void vga_clear(void);
void vga_putc(char c);
void vga_puts(const char* s);
void vga_putnum(uint32_t n);
void vga_init(void);
