// Day 04 完成版 - kernel.c（CによるVGA + シリアル出力）
#include <stdint.h>

#include "io.h"
#include "vga.h"

// ----- VGA 実装（簡易版） -----
static volatile uint16_t* const VGA_MEM =
    (uint16_t*)0xB8000;                      // VGAテキストバッファ
static uint16_t cursor_x = 0, cursor_y = 0;  // カーソル位置
static uint8_t color = 0x0F;                 // 白/黒

static inline uint16_t vga_entry(char c, uint8_t color_attr) {
    return (uint16_t)c | ((uint16_t)color_attr << 8);
}

void vga_set_color(vga_color_t fg, vga_color_t bg) {
    color = (uint8_t)fg | ((uint8_t)bg << 4);
}

void vga_move_cursor(uint16_t x, uint16_t y) {
    cursor_x = x;
    cursor_y = y;
    uint16_t pos = y * VGA_WIDTH + x;
    outb(0x3D4, 14);
    outb(0x3D5, (pos >> 8) & 0xFF);
    outb(0x3D4, 15);
    outb(0x3D5, pos & 0xFF);
}

void vga_clear(void) {
    for (uint16_t y = 0; y < VGA_HEIGHT; ++y)
        for (uint16_t x = 0; x < VGA_WIDTH; ++x)
            VGA_MEM[y * VGA_WIDTH + x] = vga_entry(' ', color);
    vga_move_cursor(0, 0);
}

static void vga_scroll_if_needed(void) {
    if (cursor_y < VGA_HEIGHT)
        return;
    for (uint16_t y = 1; y < VGA_HEIGHT; ++y)
        for (uint16_t x = 0; x < VGA_WIDTH; ++x)
            VGA_MEM[(y - 1) * VGA_WIDTH + x] = VGA_MEM[y * VGA_WIDTH + x];
    for (uint16_t x = 0; x < VGA_WIDTH; ++x)
        VGA_MEM[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', color);
    cursor_y = VGA_HEIGHT - 1;
}

void vga_putc(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
        vga_scroll_if_needed();
        vga_move_cursor(cursor_x, cursor_y);
        return;
    }
    VGA_MEM[cursor_y * VGA_WIDTH + cursor_x] = vga_entry(c, color);
    if (++cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
        vga_scroll_if_needed();
    }
    vga_move_cursor(cursor_x, cursor_y);
}

void vga_puts(const char* s) {
    while (*s) vga_putc(*s++);
}

void vga_init(void) {
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_clear();
}

// ----- シリアル（COM1）実装 -----
#define COM1 0x3F8

static void serial_init(void) {
    outb(COM1 + 1, 0x00);  // IER=0（割り込み無効）
    outb(COM1 + 3, 0x80);  // LCR: DLAB=1
    outb(COM1 + 0, 0x03);  // DLL=3（115200/38400）
    outb(COM1 + 1, 0x00);  // DLM=0
    outb(COM1 + 3, 0x03);  // LCR: 8N1, DLAB=0
    outb(COM1 + 2, 0xC7);  // FCR: FIFO有効/クリア/14B閾値
    outb(COM1 + 4, 0x0B);  // MCR: RTS/DTR/OUT2
}

static void serial_putc(char c) {
    while (!(inb(COM1 + 5) & 0x20)) {
    }  // LSR bit5 (THR empty)
    outb(COM1 + 0, (uint8_t)c);
}

static void serial_puts(const char* s) {
    while (*s) {
        if (*s == '\n')
            serial_putc('\r');
        serial_putc(*s++);
    }
}

// ----- エントリ -----
void kmain(void) {
    vga_init();
    vga_puts("Day 04: Serial debug (C)\n");
    serial_init();
    serial_puts("COM1: Hello from C!\r\n");
}
