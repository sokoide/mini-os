// Day 07 完成版 - TCB/READYリストの設計デモ
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

// --- Day07: TCB と READY リスト ---
typedef enum { THREAD_READY, THREAD_RUNNING, THREAD_BLOCKED } thread_state_t;
typedef enum {
    BLOCK_REASON_NONE,
    BLOCK_REASON_TIMER,
    BLOCK_REASON_KEYBOARD
} block_reason_t;

#define MAX_THREADS 4
#define THREAD_STACK_SIZE 1024
typedef struct thread {
    uint32_t stack[THREAD_STACK_SIZE];
    thread_state_t state;
    uint32_t counter, delay_ticks, last_tick;
    block_reason_t block_reason;
    uint32_t wake_up_tick;
    int display_row;
    struct thread* next_ready;
    struct thread* next_blocked;
    uint32_t esp;
} thread_t;

typedef struct {
    thread_t* current_thread;
    thread_t* ready_thread_list;
    thread_t* blocked_thread_list;
    uint32_t system_ticks;
} kernel_context_t;
static kernel_context_t g_ctx = {0};

static void ready_push_back(thread_t* t) {
    if (!t)
        return;
    if (!g_ctx.ready_thread_list) {
        g_ctx.ready_thread_list = t;
        t->next_ready = t;
        return;
    }
    thread_t* head = g_ctx.ready_thread_list;
    thread_t* last = head;
    while (last->next_ready != head) last = last->next_ready;
    t->next_ready = head;
    last->next_ready = t;
}
static thread_t* ready_pop_front(void) {
    thread_t* head = g_ctx.ready_thread_list;
    if (!head)
        return 0;
    if (head->next_ready == head) {
        g_ctx.ready_thread_list = 0;
        head->next_ready = 0;
        return head;
    }
    thread_t* last = head;
    while (last->next_ready != head) last = last->next_ready;
    thread_t* ret = head;
    g_ctx.ready_thread_list = head->next_ready;
    last->next_ready = g_ctx.ready_thread_list;
    ret->next_ready = 0;
    return ret;
}

static thread_t g_threads[MAX_THREADS];
static int g_thread_count = 0;
static thread_t* alloc_thread_slot(void) {
    if (g_thread_count >= MAX_THREADS)
        return 0;
    return &g_threads[g_thread_count++];
}
static void init_thread_stack_stub(thread_t* t, void (*func)(void)) {
    (void)func;
    t->esp = (uint32_t)&t->stack[THREAD_STACK_SIZE];
}

int create_thread(void (*func)(void), uint32_t delay_ticks, int display_row,
                  thread_t** out) {
    if (!func || !out)
        return -1;
    *out = 0;
    thread_t* t = alloc_thread_slot();
    if (!t)
        return -2;
    t->state = THREAD_READY;
    t->counter = 0;
    t->delay_ticks = delay_ticks ? delay_ticks : 1;
    t->last_tick = 0;
    t->block_reason = BLOCK_REASON_NONE;
    t->wake_up_tick = 0;
    t->display_row = display_row;
    t->next_ready = 0;
    t->next_blocked = 0;
    init_thread_stack_stub(t, func);
    ready_push_back(t);
    *out = t;
    return 0;
}

void demo_thread_func_A(void) {
    for (;;) { /* Day08以降で実装 */
    }
}
void demo_thread_func_B(void) {
    for (;;) { /* Day08以降で実装 */
    }
}

void kmain(void) {
    vga_init();
    vga_puts("Day 07: Thread TCB design\n");
    thread_t *t1 = 0, *t2 = 0;
    create_thread(demo_thread_func_A, 10, 10, &t1);
    create_thread(demo_thread_func_B, 20, 11, &t2);
    if (g_ctx.ready_thread_list)
        vga_puts("READY list initialized\n");
    for (;;) {
        __asm__ volatile("hlt");
    }
}
