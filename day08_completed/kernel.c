// Day 08 完成版 - コンテキストスイッチの最小デモ
#include <stdint.h>

#include "io.h"
#include "vga.h"

// --- VGA 簡易 ---
static volatile uint16_t* const VGA_MEM = (uint16_t*)0xB8000;
static uint16_t cx, cy;
static uint8_t col = 0x0F;
static inline uint16_t ve(char c, uint8_t a) {
    return (uint16_t)c | ((uint16_t)a << 8);
}
void vga_set_color(vga_color_t f, vga_color_t b) {
    col = (uint8_t)f | ((uint8_t)b << 4);
}
void vga_move_cursor(uint16_t x, uint16_t y) {
    cx = x;
    cy = y;
    uint16_t p = y * VGA_WIDTH + x;
    outb(0x3D4, 14);
    outb(0x3D5, (p >> 8) & 0xFF);
    outb(0x3D4, 15);
    outb(0x3D5, p & 0xFF);
}
void vga_clear(void) {
    for (uint16_t y = 0; y < VGA_HEIGHT; y++)
        for (uint16_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEM[y * VGA_WIDTH + x] = ve(' ', col);
    vga_move_cursor(0, 0);
}
void vga_putc(char c) {
    if (c == '\n') {
        cx = 0;
        cy++;
        vga_move_cursor(cx, cy);
        return;
    }
    VGA_MEM[cy * VGA_WIDTH + cx] = ve(c, col);
    if (++cx >= VGA_WIDTH) {
        cx = 0;
        cy++;
    }
    vga_move_cursor(cx, cy);
}
void vga_puts(const char* s) {
    while (*s) vga_putc(*s++);
}
void vga_init(void) {
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_clear();
}

// --- シリアル（COM1）デバッグ ---
#define COM1 0x3F8
static inline void serial_init(void) {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}
static inline void serial_putc(char c) {
    while (!(inb(COM1 + 5) & 0x20)) {
    }
    outb(COM1 + 0, (uint8_t)c);
}
static inline void serial_puts(const char* s) {
    while (*s) {
        if (*s == '\n')
            serial_putc('\r');
        serial_putc(*s++);
    }
}
static inline void serial_puthex(uint32_t v) {
    const char* h = "0123456789ABCDEF";
    for (int i = 7; i >= 0; i--) serial_putc(h[(v >> (i * 4)) & 0xF]);
}

// --- Day08: コンテキストスイッチ ---
typedef struct thread {
    uint32_t stack[1024];
    uint32_t esp;
    int row;
} thread_t;
extern void context_switch(uint32_t** old_esp, uint32_t* new_esp);

static void init_stack(thread_t* t, void (*func)(void)) {
    uint32_t* sp = &t->stack[1024];

    // 正しいスレッド初期化 - switch_contextと互換性のあるスタックレイアウト
    // switch_context で復元される順序に合わせて積む
    *--sp =
        (uint32_t)func;  // 関数アドレス（最初のスレッド実行時にret先になる）
    // Day08 では割り込みインフラ未整備のため IF=0 にする（予約ビットは1）
    // EFLAGS = 0x00000002（IF=0, 必須予約ビット=1）
    *--sp = 0x00000002;  // EFLAGS（IF=0, reserved bit=1）
    *--sp = 0;           // EBP
    *--sp = 0;           // EDI
    *--sp = 0;           // ESI
    *--sp = 0;           // EDX
    *--sp = 0;           // ECX
    *--sp = 0;           // EBX
    *--sp = 0;           // EAX

    t->esp = (uint32_t)sp;
}

static thread_t th1, th2;
static uint32_t* current_esp = 0;

static void threadA(void) {
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    // 一度だけ表示してアイドル（Day08は割り込み未使用のため、連続描画はチラつきやすい）
    vga_move_cursor(0, 10);
    vga_puts("Thread A running...");
    serial_puts("Thread A running\n");
    for (;;) {
        __asm__ volatile("nop");
    }
}
static void threadB(void) {
    // Day08ではここには来ない
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_move_cursor(0, 11);
    vga_puts("Thread B running...");
    serial_puts("[This is NOT printed] Thread B running\n");
    for (;;) {
        __asm__ volatile("nop");
    }
}

void kmain(void) {
    serial_init();
    serial_puts("KMAIN begin\n");
    vga_init();
    vga_puts("Day 08: Context Switch demo\n");
    th1.row = 10;
    th2.row = 11;
    init_stack(&th1, threadA);
    init_stack(&th2, threadB);
    serial_puts("SWITCH to th1 esp=");
    serial_puthex(th1.esp);
    serial_puts("\n");
    // 最初のスレッドへ切替
    context_switch(&current_esp, (uint32_t*)th1.esp);
    serial_puts("RETURNED unexpectedly\n");
    for (;;) {
        __asm__ volatile("hlt");
    }
}
