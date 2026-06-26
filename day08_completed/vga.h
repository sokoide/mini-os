// vga.h — VGAテキストモード制御
#pragma once
#include <stdint.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

typedef enum {
    VGA_BLACK = 0,
    VGA_BLUE,
    VGA_GREEN,
    VGA_CYAN,
    VGA_RED,
    VGA_MAGENTA,
    VGA_BROWN,
    VGA_LIGHT_GRAY,
    VGA_DARK_GRAY,
    VGA_LIGHT_BLUE,
    VGA_LIGHT_GREEN,
    VGA_LIGHT_CYAN,
    VGA_LIGHT_RED,
    VGA_PINK,
    VGA_YELLOW,
    VGA_WHITE
} vga_color_t;

void vga_init(void);
void vga_clear(void);
void vga_set_color(vga_color_t fg, vga_color_t bg);
void vga_move_cursor(uint16_t x, uint16_t y);
void vga_putc(char c);
void vga_puts(const char* s);
