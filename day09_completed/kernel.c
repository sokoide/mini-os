// Day 09 完成版 - プリエンプティブ・ラウンドロビン（簡易）
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
void vga_putnum(uint32_t n) {
    int i = 0;
    if (n == 0) {
        vga_putc('0');
        return;
    }
    uint32_t x = n;
    char r[10];
    while (x) {
        r[i++] = '0' + (x % 10);
        x /= 10;
    }
    while (i--) vga_putc(r[i]);
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

// --- スレッド/スケジューラ ---
typedef struct thread {
    uint32_t stack[1024];
    uint32_t esp;
    int row;
    uint32_t cnt;
    struct thread* next;
} thread_t;
extern void context_switch(uint32_t* old_esp, uint32_t new_esp);
extern void initial_context_switch(uint32_t new_esp);

static thread_t *current = 0, *ready = 0;
static volatile uint32_t tick = 0;
static uint32_t slice_ticks = 10;  // 100Hzで約100ms

static void ready_push(thread_t* t) {
    if (!t)
        return;
    if (!ready) {
        ready = t;
        t->next = t;
        return;
    }
    thread_t* last = ready;
    while (last->next != ready) last = last->next;
    t->next = ready;
    last->next = t;
}

static void init_stack(thread_t* t, void (*func)(void)) {
    uint32_t* sp = &t->stack[1024];

    // 正しいスレッド初期化 - switch_contextと互換性のあるスタックレイアウト
    // switch_context で復元される順序に合わせて積む
    *--sp =
        (uint32_t)func;  // 関数アドレス（最初のスレッド実行時にret先になる）
    *--sp = 0x00000202;  // EFLAGS（IF=1, reserved bit=1）
    *--sp = 0;           // EBP
    *--sp = 0;           // EDI
    *--sp = 0;           // ESI
    *--sp = 0;           // EDX
    *--sp = 0;           // ECX
    *--sp = 0;           // EBX
    *--sp = 0;           // EAX

    t->esp = (uint32_t)sp;
}

// PIC/PIT/IDT
#define PIC1_CMD 0x20
#define PIC1_DATA 0x21
#define PIC2_CMD 0xA0
#define PIC2_DATA 0xA1
#define PIC_EOI 0x20
static inline void eoi_master(void) {
    outb(PIC1_CMD, PIC_EOI);
}
static void remap_pic(void) {
    uint8_t a1 = inb(PIC1_DATA), a2 = inb(PIC2_DATA);
    outb(PIC1_CMD, 0x11);
    outb(PIC2_CMD, 0x11);
    outb(PIC1_DATA, 0x20);
    outb(PIC2_DATA, 0x28);
    outb(PIC1_DATA, 0x04);
    outb(PIC2_DATA, 0x02);
    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);
    outb(PIC1_DATA, a1);
    outb(PIC2_DATA, a2);
}
static void set_masks(uint8_t m, uint8_t s) {
    outb(PIC1_DATA, m);
    outb(PIC2_DATA, s);
}
#define PIT_CH0 0x40
#define PIT_CMD 0x43
static void init_pit_100hz(void) {
    uint16_t div = 11932;
    outb(PIT_CMD, 0x36);
    outb(PIT_CH0, div & 0xFF);
    outb(PIT_CH0, (div >> 8) & 0xFF);
}

// IDT
typedef struct {
    uint16_t lo, sel;
    uint8_t zero, flags;
    uint16_t hi;
} __attribute__((packed)) idt_entry_t;
typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_ptr_t;
static idt_entry_t idt[256];
static idt_ptr_t idtr;
static inline void lidt(void* p) {
    __asm__ volatile("lidt (%0)" ::"r"(p));
}
static void set_gate(int n, uint32_t h) {
    idt[n].lo = h & 0xFFFF;
    idt[n].sel = 0x08;
    idt[n].zero = 0;
    idt[n].flags = 0x8E;
    idt[n].hi = (h >> 16) & 0xFFFF;
}
extern void isr0(void);
extern void isr3(void);
extern void isr6(void);
extern void isr13(void);
extern void isr14(void);
extern void timer_interrupt_handler(void);
static void idt_init(void) {
    for (int i = 0; i < 256; i++) set_gate(i, 0);
    set_gate(0, (uint32_t)isr0);
    set_gate(3, (uint32_t)isr3);
    set_gate(6, (uint32_t)isr6);
    set_gate(13, (uint32_t)isr13);
    set_gate(14, (uint32_t)isr14);
    set_gate(32, (uint32_t)timer_interrupt_handler);
    idtr.limit = sizeof(idt) - 1;
    idtr.base = (uint32_t)&idt[0];
    lidt(&idtr);
}

// IRQ共通ハンドラ
struct isr_stack {
    uint32_t regs[8];
    uint32_t int_no;
    uint32_t err;
};
/* context_switch is declared above. */
static uint32_t cur_esp = 0;
static uint32_t last_slice_tick = 0;

static void schedule(void) {
    serial_puts("schedule() called\n");

    if (!ready || !current) {
        serial_puts("schedule: no ready or current thread\n");
        return;
    }

    thread_t* next = ready->next;  // 現在の次候補
    serial_puts("schedule: next=");
    serial_puthex((uint32_t)next);
    serial_puts("\n");

    if (next == current) {
        serial_puts("schedule: next==current, no switch\n");
        return;
    }

    // readyポインタを次のスレッドに進める
    ready = ready->next;

    thread_t* prev = current;
    current = next;

    // デバッグ: 現在/次スレッドとESPを出力
    serial_puts("SCHED prev=");
    serial_puthex((uint32_t)prev);
    serial_puts(" next=");
    serial_puthex((uint32_t)current);
    serial_puts(" prevESP=");
    serial_puthex(prev->esp);
    serial_puts(" nextESP=");
    serial_puthex(current->esp);
    serial_puts("\n");
    context_switch(&prev->esp, current->esp);
}

// 専用のタイマー割り込みハンドラ
void timer_handler_c(void) {
    // EOI must be sent BEFORE schedule() - critical for continued timer
    // interrupts
    eoi_master();

    tick++;
    if ((tick & 0x3F) == 0) {
        serial_puts("TICK\n");
    }
    if ((tick - last_slice_tick) >= slice_ticks) {
        last_slice_tick = tick;
        serial_puts("Timer calling schedule\n");
        schedule();
    }
}

void isr_handler_c(struct isr_stack* f) {
    // 例外簡易表示（シリアルにも出力）
    serial_puts("EXC vec=");
    serial_puthex(f->int_no);
    serial_puts("\n");
    vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
    vga_puts("[EXC] vec=");
    vga_putnum(f->int_no);
    vga_putc('\n');
}

// デモスレッド
static thread_t th1, th2;
static void threadA(void) {
    serial_puts("threadA start\n");
    for (;;) {
        ++th1.cnt;
        if ((th1.cnt & 0xFFFF) == 0) {
            vga_move_cursor(0, 10);
            vga_set_color(VGA_YELLOW, VGA_BLACK);
            vga_puts("A:"), vga_putnum(th1.cnt), vga_puts("   ");
        }
        if ((th1.cnt & 0xFFFFFF) == 0) {
            serial_puts("threadA alive\n");
        }
    }
}

static void threadB(void) {
    serial_puts("threadB start\n");
    for (;;) {
        ++th2.cnt;
        if ((th2.cnt & 0xFFFF) == 0) {
            vga_move_cursor(0, 11);
            vga_set_color(VGA_CYAN, VGA_BLACK);
            vga_puts("B:");
            vga_putnum(th2.cnt);
            vga_puts("   ");
        }
        if ((th2.cnt & 0xFFFFFF) == 0) {
            serial_puts("threadB alive\n");
        }
    }
}

void kmain(void) {
    serial_init();
    serial_puts("D09 kmain begin\n");
    vga_init();
    vga_puts("Day 09: Preemptive RR\n");
    // スレッド準備
    th1.row = 10;
    th2.row = 11;
    th1.cnt = th2.cnt = 0;
    th1.next = th2.next = 0;
    init_stack(&th1, threadA);
    init_stack(&th2, threadB);
    ready_push(&th1);
    ready_push(&th2);
    current = &th1;  // 最初のcurrent

    remap_pic();
    set_masks(0xFE, 0xFF);
    idt_init();
    init_pit_100hz();
    serial_puts("PIC/IDT/PIT ready, sti\n");
    __asm__ volatile("sti");
    // 最初のスレッドへ（初期スタックから開始、start_first は使わない）
    serial_puts("START first thread esp=");
    serial_puthex(current->esp);
    serial_puts("\n");
    initial_context_switch(current->esp);
}
