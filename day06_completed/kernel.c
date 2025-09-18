// Day 06 完成版 - kernel.c（CでPIC再マッピング + PIT初期化 + IDT/IRQ0 +
// VGA表示）
#include <stdint.h>

#include "io.h"
#include "vga.h"

// ----- VGA 簡易実装 -----
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

// ----- IDT 構築 -----
struct idt_entry {
    uint16_t base_low;   // ハンドラ下位16ビット
    uint16_t sel;        // セグメントセレクタ（通常0x08）
    uint8_t always0;     // 常に0
    uint8_t flags;       // 0x8E = present/特権0/32bit割り込みゲート
    uint16_t base_high;  // ハンドラ上位16ビット
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;  // IDTサイズ-1
    uint32_t base;   // IDT先頭アドレス
} __attribute__((packed));

#define IDT_SIZE 256
#define IDT_FLAG_PRESENT_DPL0_32INT 0x8E

static struct idt_entry idt[IDT_SIZE];
static struct idt_ptr idtr;

static inline void lidt(void* idtr_ptr) {
    __asm__ volatile("lidt (%0)" ::"r"(idtr_ptr));
}

static void set_idt_gate(int n, uint32_t handler) {
    idt[n].base_low = handler & 0xFFFF;
    idt[n].sel = 0x08;
    idt[n].always0 = 0;
    idt[n].flags = IDT_FLAG_PRESENT_DPL0_32INT;
    idt[n].base_high = (handler >> 16) & 0xFFFF;
}
static void load_idt(void) {
    idtr.limit = sizeof(idt) - 1;
    idtr.base = (uint32_t)&idt[0];
    lidt(&idtr);
}

// ----- 例外/IRQ スタブ宣言（interrupt.s） -----
extern void isr0(void);
extern void isr3(void);
extern void isr6(void);
extern void isr13(void);
extern void isr14(void);
extern void irq0(void);

// ----- PIC（8259A） -----
#define PIC1_CMD 0x20
#define PIC1_DATA 0x21
#define PIC2_CMD 0xA0
#define PIC2_DATA 0xA1
#define PIC_EOI 0x20

static void remap_pic(void) {
    uint8_t a1 = inb(PIC1_DATA);
    uint8_t a2 = inb(PIC2_DATA);
    outb(PIC1_CMD, 0x11);  // ICW1
    outb(PIC2_CMD, 0x11);
    outb(PIC1_DATA, 0x20);  // ICW2 マスタ 0x20
    outb(PIC2_DATA, 0x28);  // ICW2 スレーブ 0x28
    outb(PIC1_DATA, 0x04);  // ICW3 マスタ: IRQ2にスレーブ
    outb(PIC2_DATA, 0x02);  // ICW3 スレーブID=2
    outb(PIC1_DATA, 0x01);  // ICW4 8086
    outb(PIC2_DATA, 0x01);
    outb(PIC1_DATA, a1);  // マスク復元
    outb(PIC2_DATA, a2);
}
static void set_irq_masks(uint8_t m, uint8_t s) {
    outb(PIC1_DATA, m);
    outb(PIC2_DATA, s);
}
static inline void send_eoi_master(void) {
    outb(PIC1_CMD, PIC_EOI);
}

// ----- PIT（8254） -----
#define PIT_CH0 0x40
#define PIT_CMD 0x43
static void init_pit_100hz(void) {
    uint16_t div = 11932;  // 100Hz 近似
    outb(PIT_CMD, 0x36);   // ch0, lo/hi, mode3
    outb(PIT_CH0, div & 0xFF);
    outb(PIT_CH0, (div >> 8) & 0xFF);
}

// ----- 共通ハンドラ -----
struct isr_stack {
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;
    uint32_t int_no;
    uint32_t err_code;
};
static volatile uint32_t tick = 0;

static void irq_handler_timer(void) {
    tick++;
    if ((tick % 100) == 0) {
        vga_puts(".");
    }
    send_eoi_master();
}

void isr_handler_c(struct isr_stack* f) {
    if (f->int_no == 32) {
        irq_handler_timer();
        return;
    }
    // 例外（参考表示）
    vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
    vga_puts("[EXCEPTION] vec=");
    int n = (int)f->int_no;
    if (n >= 10) {
        vga_putc('0' + (n / 10));
    }
    vga_putc('0' + (n % 10));
    vga_puts(" err=");
    int e = (int)f->err_code;
    if (e >= 10) {
        vga_putc('0' + (e / 10));
    }
    vga_putc('0' + (e % 10));
    vga_puts("\n");
}

static void idt_init_with_timer(void) {
    for (int i = 0; i < IDT_SIZE; i++) set_idt_gate(i, 0);
    set_idt_gate(0, (uint32_t)isr0);
    set_idt_gate(3, (uint32_t)isr3);
    set_idt_gate(6, (uint32_t)isr6);
    set_idt_gate(13, (uint32_t)isr13);
    set_idt_gate(14, (uint32_t)isr14);
    set_idt_gate(32, (uint32_t)irq0);  // IRQ0: タイマ
    load_idt();
}

// ----- エントリ -----
void kmain(void) {
    vga_init();
    vga_puts("Day 06: Timer IRQ (100Hz)\n");

    remap_pic();
    set_irq_masks(0xFE, 0xFF);  // IRQ0のみ許可
    idt_init_with_timer();
    init_pit_100hz();

    __asm__ volatile("sti");  // CPU割り込み許可

    for (;;) {
        __asm__ volatile("hlt");
    }
}
