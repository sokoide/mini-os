// Day 03 完成版 - kernel.c（CによるVGA出力）
#include <stdint.h>

#include "io.h"
#include "vga.h"

// ポートI/O（inb/outb/io_wait）は io.h によって提供される

static volatile uint16_t* const VGA_MEM =
    (uint16_t*)0xB8000;                      // VGAテキストバッファ先頭
static uint16_t cursor_x = 0, cursor_y = 0;  // カーソル位置
static uint8_t color = 0x0F;                 // 白（前景）/ 黒（背景）

static inline uint16_t vga_entry(char c, uint8_t color_attr) {
    // 文字(下位8ビット) + 属性(上位8ビット) を 1 ワードに合成
    return (uint16_t)c | ((uint16_t)color_attr << 8);
}

void vga_set_color(vga_color_t fg, vga_color_t bg) {
    color = (uint8_t)fg | ((uint8_t)bg << 4);
}

void vga_move_cursor(uint16_t x, uint16_t y) {
    // ハードウェアカーソル位置の更新（CRTC: インデックス 14/15）
    cursor_x = x;
    cursor_y = y;
    uint16_t pos = y * VGA_WIDTH + x;
    outb(0x3D4, 14);
    outb(0x3D5, (pos >> 8) & 0xFF);
    outb(0x3D4, 15);
    outb(0x3D5, pos & 0xFF);
}

void vga_clear(void) {
    // 画面全体を空白でクリアし、カーソルを(0,0)へ
    for (uint16_t y = 0; y < VGA_HEIGHT; ++y) {
        for (uint16_t x = 0; x < VGA_WIDTH; ++x) {
            VGA_MEM[y * VGA_WIDTH + x] = vga_entry(' ', color);
        }
    }
    vga_move_cursor(0, 0);
}

static void vga_scroll_if_needed(void) {
    if (cursor_y < VGA_HEIGHT)
        return;
    // 1行分スクロールアップし、最下行を空白で埋める
    for (uint16_t y = 1; y < VGA_HEIGHT; ++y) {
        for (uint16_t x = 0; x < VGA_WIDTH; ++x) {
            VGA_MEM[(y - 1) * VGA_WIDTH + x] = VGA_MEM[y * VGA_WIDTH + x];
        }
    }
    for (uint16_t x = 0; x < VGA_WIDTH; ++x) {
        VGA_MEM[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', color);
    }
    cursor_y = VGA_HEIGHT - 1;
}

void vga_putc(char c) {
    // 1文字表示。改行は次の行の先頭へ移動
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
    // ヌル終端文字列を順に表示
    while (*s) vga_putc(*s++);
}

void vga_init(void) {
    // 既定の色設定と画面クリア
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_clear();
}

void kmain(void) {
    vga_init();
    vga_puts("Day 03: C-based VGA driver\n");
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_puts("Hello from C!\n");
}
