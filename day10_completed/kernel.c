// Day 10 完成版 - スリープ/タイミングシステム
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
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

// --- Thread Management & Scheduling ---

#define THREAD_STACK_SIZE 1024
#define MAX_THREADS 4
#define MAX_COUNTER_VALUE 65535
#define EFLAGS_INTERRUPT_ENABLE 0x00000202

typedef enum {
    OS_SUCCESS = 0,
    OS_ERROR_NULL_POINTER = -1,
    OS_ERROR_INVALID_PARAMETER = -2,
    OS_ERROR_OUT_OF_MEMORY = -3,
    OS_ERROR_INVALID_STATE = -4,
} os_result_t;

#define OS_SUCCESS_CHECK(result) ((result) == OS_SUCCESS)
#define OS_FAILURE_CHECK(result) ((result) != OS_SUCCESS)

typedef enum {
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
} thread_state_t;

typedef enum {
    BLOCK_REASON_NONE,
    BLOCK_REASON_TIMER,
} block_reason_t;

typedef struct thread {
    uint32_t stack[THREAD_STACK_SIZE];
    uint32_t esp;

    thread_state_t state;
    block_reason_t block_reason;

    uint32_t counter;
    uint32_t delay_ticks;
    uint32_t last_tick;
    uint32_t wake_up_tick;

    int display_row;

    struct thread* next_ready;
    struct thread* next_blocked;
} thread_t;

typedef struct {
    thread_t* current_thread;
    thread_t* ready_thread_list;
    thread_t* blocked_thread_list;
    volatile uint32_t system_ticks;
    volatile int scheduler_lock_count;
} kernel_context_t;

static kernel_context_t k_context;

extern void context_switch(uint32_t* old_esp, uint32_t new_esp);
extern void initial_context_switch(uint32_t new_esp);

static uint32_t slice_ticks = 10;
static uint32_t last_slice_tick = 0;

// --- Kernel Context Accessors ---
kernel_context_t* get_kernel_context(void) {
    return &k_context;
}

thread_t* get_current_thread(void) {
    return get_kernel_context()->current_thread;
}

uint32_t get_system_ticks(void) {
    return get_kernel_context()->system_ticks;
}

void schedule(void);
static void sleep(uint32_t ticks);

// --- Thread Creation ---

static os_result_t validate_thread_params(void (*func)(void), int display_row,
                                          uint32_t* delay_ticks) {
    if (!func) {
        serial_puts("ERROR: create_thread called with NULL function pointer\n");
        return OS_ERROR_NULL_POINTER;
    }

    if (display_row < 0 || display_row >= 25) {
        serial_puts("ERROR: create_thread called with invalid display_row\n");
        return OS_ERROR_INVALID_PARAMETER;
    }

    if (*delay_ticks == 0) {
        serial_puts(
            "WARNING: create_thread called with delay_ticks=0, using 1\n");
        *delay_ticks = 1;
    }

    return OS_SUCCESS;
}

static void initialize_thread_stack(thread_t* thread, void (*func)(void)) {
    uint32_t* sp = &thread->stack[THREAD_STACK_SIZE];

    *--sp = (uint32_t)func;
    *--sp = EFLAGS_INTERRUPT_ENABLE;
    *--sp = 0;  // EBP
    *--sp = 0;  // EDI
    *--sp = 0;  // ESI
    *--sp = 0;  // EDX
    *--sp = 0;  // ECX
    *--sp = 0;  // EBX
    *--sp = 0;  // EAX
    thread->esp = (uint32_t)sp;
}

static void configure_thread_attributes(thread_t* thread, uint32_t delay_ticks,
                                        int display_row) {
    thread->state = THREAD_READY;
    thread->counter = 0;
    thread->delay_ticks = delay_ticks;
    thread->last_tick = 0;
    thread->display_row = display_row;
    thread->next_ready = NULL;
    thread->block_reason = BLOCK_REASON_NONE;
    thread->wake_up_tick = 0;
    thread->next_blocked = NULL;
}

static os_result_t add_thread_to_ready_list(thread_t* thread) {
    kernel_context_t* ctx = get_kernel_context();

    if (ctx->ready_thread_list == NULL) {
        ctx->ready_thread_list = thread;
        thread->next_ready = thread;
    } else {
        thread_t* last = ctx->ready_thread_list;
        int safety_counter = 0;

        while (last->next_ready != ctx->ready_thread_list &&
               safety_counter < MAX_THREADS) {
            last = last->next_ready;
            safety_counter++;
        }

        if (safety_counter >= MAX_THREADS) {
            serial_puts("ERROR: Thread list appears corrupted\n");
            return OS_ERROR_INVALID_STATE;
        }

        thread->next_ready = ctx->ready_thread_list;
        last->next_ready = thread;
    }

    return OS_SUCCESS;
}

os_result_t create_thread(void (*func)(void), uint32_t delay_ticks,
                          int display_row, thread_t** out_thread) {
    static thread_t threads[MAX_THREADS];
    static int thread_count = 0;

    if (!out_thread) {
        serial_puts(
            "ERROR: create_thread called with NULL out_thread pointer\n");
        return OS_ERROR_NULL_POINTER;
    }
    *out_thread = NULL;

    os_result_t validation_result =
        validate_thread_params(func, display_row, &delay_ticks);
    if (OS_FAILURE_CHECK(validation_result)) {
        return validation_result;
    }

    if (thread_count >= MAX_THREADS) {
        serial_puts("ERROR: Maximum number of threads exceeded\n");
        return OS_ERROR_OUT_OF_MEMORY;
    }

    thread_t* thread = &threads[thread_count++];

    initialize_thread_stack(thread, func);
    configure_thread_attributes(thread, delay_ticks, display_row);

    os_result_t add_result = add_thread_to_ready_list(thread);
    if (OS_FAILURE_CHECK(add_result)) {
        thread_count--;
        return add_result;
    }

    serial_puts("SUCCESS: Thread created successfully\n");
    *out_thread = thread;
    return OS_SUCCESS;
}

static void remove_from_ready_list(thread_t* thread) {
    kernel_context_t* ctx = get_kernel_context();

    if (!ctx->ready_thread_list || !thread)
        return;

    if (ctx->ready_thread_list == thread && thread->next_ready == thread) {
        ctx->ready_thread_list = NULL;
    } else {
        thread_t* prev = ctx->ready_thread_list;
        while (prev->next_ready != thread) {
            prev = prev->next_ready;
        }
        prev->next_ready = thread->next_ready;

        if (ctx->ready_thread_list == thread) {
            ctx->ready_thread_list = thread->next_ready;
        }
    }
}

// PIC/PIT/IDT（必要最小限）
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

// 初期化ログ（PIC/IDTの設定確認用）
static void debug_log_interrupt_setup(void) {
    serial_puts("DEBUG: IDT gates set: 32=");
    serial_puthex((uint32_t)timer_interrupt_handler);
    serial_puts("\n");
    uint8_t m = inb(PIC1_DATA), s = inb(PIC2_DATA);
    serial_puts("DEBUG: PIC masks M=");
    serial_puthex(m);
    serial_puts(" S=");
    serial_puthex(s);
    serial_puts("\n");
}

// --- Thread Blocking and Sleeping ---

static void unblock_and_requeue_thread(thread_t* thread, thread_t* prev) {
    kernel_context_t* ctx = get_kernel_context();
    if (prev) {
        prev->next_blocked = thread->next_blocked;
    } else {
        ctx->blocked_thread_list = thread->next_blocked;
    }

    thread->state = THREAD_READY;
    thread->block_reason = BLOCK_REASON_NONE;
    thread->next_blocked = NULL;
    add_thread_to_ready_list(thread);
}

static void check_and_wake_timer_threads(void) {
    asm volatile("cli");
    kernel_context_t* ctx = get_kernel_context();
    thread_t* current = ctx->blocked_thread_list;
    thread_t* prev = NULL;
    int woke_count = 0;

    while (current) {
        thread_t* next = current->next_blocked;
        if (current->block_reason == BLOCK_REASON_TIMER &&
            current->wake_up_tick <= ctx->system_ticks) {
            serial_puts("WAKE_THREAD ticks=");
            serial_puthex(ctx->system_ticks);
            serial_puts(" wake_up=");
            serial_puthex(current->wake_up_tick);
            serial_puts("\n");
            unblock_and_requeue_thread(current, prev);
            woke_count++;
        } else {
            prev = current;
        }
        current = next;
    }

    if (woke_count > 0) {
        serial_puts("WOKE_UP_THREADS count=");
        serial_puthex(woke_count);
        serial_puts("\n");
    }

    asm volatile("sti");
}

void block_current_thread(block_reason_t reason, uint32_t data) {
    asm volatile("cli");

    thread_t* thread = get_current_thread();
    if (!thread) {
        asm volatile("sti");
        return;
    }

    remove_from_ready_list(thread);

    thread->state = THREAD_BLOCKED;
    thread->block_reason = reason;
    thread->next_blocked = NULL;

    kernel_context_t* ctx = get_kernel_context();

    if (reason == BLOCK_REASON_TIMER) {
        thread->wake_up_tick = data;
        if (!ctx->blocked_thread_list ||
            thread->wake_up_tick < ctx->blocked_thread_list->wake_up_tick) {
            thread->next_blocked = ctx->blocked_thread_list;
            ctx->blocked_thread_list = thread;
        } else {
            thread_t* current = ctx->blocked_thread_list;
            while (current->next_blocked &&
                   current->next_blocked->wake_up_tick <=
                       thread->wake_up_tick) {
                current = current->next_blocked;
            }
            thread->next_blocked = current->next_blocked;
            current->next_blocked = thread;
        }
    }

    asm volatile("sti");
}

// --- Scheduler ---

static inline void acquire_scheduler_lock(void) {
    kernel_context_t* ctx = get_kernel_context();
    asm volatile("cli");
    ctx->scheduler_lock_count++;
    asm volatile("sti");
}

static inline void release_scheduler_lock(void) {
    kernel_context_t* ctx = get_kernel_context();
    asm volatile("cli");
    ctx->scheduler_lock_count--;
    asm volatile("sti");
}

static inline bool is_scheduler_locked(void) {
    return get_kernel_context()->scheduler_lock_count > 0;
}

static void handle_initial_thread_selection(void) {
    kernel_context_t* ctx = get_kernel_context();

    asm volatile("cli");
    ctx->current_thread = ctx->ready_thread_list;
    ctx->current_thread->state = THREAD_RUNNING;
    asm volatile("sti");

    release_scheduler_lock();
    initial_context_switch(ctx->current_thread->esp);
}

static void perform_thread_switch(void) {
    kernel_context_t* ctx = get_kernel_context();

    thread_t* old_thread = ctx->current_thread;
    thread_t* next_thread = ctx->current_thread->next_ready;

    thread_t* search_start = next_thread;
    while (next_thread && next_thread != old_thread) {
        if (next_thread->state == THREAD_READY) {
            asm volatile("cli");
            old_thread->state = THREAD_READY;
            next_thread->state = THREAD_RUNNING;
            ctx->current_thread = next_thread;
            asm volatile("sti");

            release_scheduler_lock();
            context_switch(&old_thread->esp, next_thread->esp);
            return;
        }

        next_thread = next_thread->next_ready;

        if (next_thread == search_start) {
            break;
        }
    }
    release_scheduler_lock();
}

static void handle_blocked_thread_scheduling(void) {
    kernel_context_t* ctx = get_kernel_context();
    thread_t* blocked_thread = ctx->current_thread;

    if (ctx->ready_thread_list) {
        asm volatile("cli");
        ctx->current_thread = ctx->ready_thread_list;
        ctx->current_thread->state = THREAD_RUNNING;
        asm volatile("sti");

        release_scheduler_lock();
        context_switch(&blocked_thread->esp, ctx->current_thread->esp);
    } else {
        release_scheduler_lock();
        while (!ctx->ready_thread_list) {
            asm volatile("hlt");
        }
        schedule();
    }
}

void schedule(void) {
    if (is_scheduler_locked()) {
        serial_puts("SCHED_LOCKED\n");
        return;
    }

    acquire_scheduler_lock();
    check_and_wake_timer_threads();
    kernel_context_t* ctx = get_kernel_context();

    if (!ctx->ready_thread_list) {
        release_scheduler_lock();
        schedule();
        return;
    }

    if (!ctx->current_thread) {
        serial_puts("INITIAL_THREAD_SEL\n");
        handle_initial_thread_selection();
        return;
    }

    if (ctx->current_thread->state == THREAD_BLOCKED) {
        serial_puts("BLOCKED_SCHEDULING\n");
        handle_blocked_thread_scheduling();
        return;
    }

    serial_puts("PERFORM_SWITCH\n");
    perform_thread_switch();
}

static void sleep(uint32_t ticks) {
    if (ticks == 0) {
        return;
    }

    if (ticks > MAX_COUNTER_VALUE) {
        ticks = MAX_COUNTER_VALUE;
    }

    if (!get_current_thread()) {
        return;
    }

    uint32_t wake_up_time = get_system_ticks() + ticks;
    block_current_thread(BLOCK_REASON_TIMER, wake_up_time);
    schedule();
}

// --- タイマー/例外ハンドラ ---
void timer_handler_c(void) {
    static uint32_t timer_count = 0;
    timer_count++;

    eoi_master();
    get_kernel_context()->system_ticks++;

    // More frequent debug output to see if timer is working
    if ((timer_count & 0x0F) == 0) {  // Every 16 timer interrupts
        serial_puts("TIMER_");
        serial_puthex(timer_count);
        serial_puts(" ticks=");
        serial_puthex(get_system_ticks());
        serial_puts("\n");
    }

    if ((get_system_ticks() - last_slice_tick) >= slice_ticks) {
        serial_puts("SCHEDULE_CALL\n");
        last_slice_tick = get_system_ticks();
        schedule();
    }
}

struct isr_stack {
    uint32_t regs[8];
    uint32_t int_no;
    uint32_t err;
};
void isr_handler_c(struct isr_stack* f) {
    vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
    vga_puts("[EXC] vec=");
    vga_putnum(f->int_no);
    vga_putc('\n');
    serial_puts("EXC vec=");
    serial_puthex(f->int_no);
    serial_puts("\n");
}

// idleスレッド
static void idle_thread(void) {
    for (;;) {
        asm volatile("hlt");
    }
}

// Day 10 Demo Threads - Sleep/Timing System
static void threadA(void) {
    serial_puts("threadA start (50ms sleep)\n");
    for (;;) {
        thread_t* self = get_current_thread();
        self->counter++;
        __asm__ volatile("cli");
        vga_move_cursor(0, 2);
        vga_set_color(VGA_YELLOW, VGA_BLACK);
        vga_puts("Fast Thread A: ");
        vga_putnum(self->counter);
        vga_puts("        ");  // Clear previous digits
        __asm__ volatile("sti");
        if ((self->counter & 0x1FF) == 0)
            serial_puts("threadA alive\n");

        sleep(5);  // 50ms sleep
    }
}

static void threadB(void) {
    serial_puts("threadB start (100ms sleep)\n");
    for (;;) {
        thread_t* self = get_current_thread();
        self->counter++;
        __asm__ volatile("cli");
        vga_move_cursor(0, 3);
        vga_set_color(VGA_CYAN, VGA_BLACK);
        vga_puts("Medium Thread B: ");
        vga_putnum(self->counter);
        vga_puts("        ");  // Clear previous digits
        __asm__ volatile("sti");
        if ((self->counter & 0x1FF) == 0)
            serial_puts("threadB alive\n");

        sleep(10);  // 100ms sleep
    }
}

static void threadC(void) {
    serial_puts("threadC start (200ms sleep)\n");
    for (;;) {
        thread_t* self = get_current_thread();
        self->counter++;
        __asm__ volatile("cli");
        vga_move_cursor(0, 4);
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        vga_puts("Slow Thread C: ");
        vga_putnum(self->counter);
        vga_puts("        ");  // Clear previous digits
        __asm__ volatile("sti");
        if ((self->counter & 0x1FF) == 0)
            serial_puts("threadC alive\n");

        sleep(20);  // 200ms sleep
    }
}

static void threadD(void) {
    serial_puts("threadD start (500ms sleep)\n");
    for (;;) {
        thread_t* self = get_current_thread();
        self->counter++;
        __asm__ volatile("cli");
        vga_move_cursor(0, 5);
        vga_set_color(VGA_LIGHT_MAGENTA, VGA_BLACK);
        vga_puts("Very Slow Thread D: ");
        vga_putnum(self->counter);
        vga_puts("        ");  // Clear previous digits
        __asm__ volatile("sti");
        if ((self->counter & 0x1FF) == 0)
            serial_puts("threadD alive\n");

        sleep(50);  // 500ms sleep
    }
}

extern void timer_interrupt_handler(void);

static void init_kernel_context(void) {
    k_context.current_thread = NULL;
    k_context.ready_thread_list = NULL;
    k_context.blocked_thread_list = NULL;
    k_context.system_ticks = 0;
    k_context.scheduler_lock_count = 0;
}

void kmain(void) {
    serial_init();
    serial_puts("D10 kmain begin\n");
    vga_init();
    vga_puts("Day 10: Sleep/Timing System\n");

    init_kernel_context();

    thread_t *th1, *th2, *th3, *th4, *tidle;
    create_thread(threadA, 5, 2, &th1);   // Fast: 50ms
    create_thread(threadB, 10, 3, &th2);  // Medium: 100ms
    create_thread(threadC, 20, 4, &th3);  // Slow: 200ms
    create_thread(threadD, 50, 5, &th4);  // Very slow: 500ms
    // delay_ticks = 1 and row = 0 are not used
    create_thread(idle_thread, 1, 0, &tidle);

    remap_pic();
    set_masks(0xFE, 0xFF);  // Only enable timer (IRQ0), disable keyboard (IRQ1)
    idt_init();
    init_pit_100hz();
    serial_puts("PIC/IDT/PIT ready, sti\n");
    debug_log_interrupt_setup();
    __asm__ volatile("sti");

    // Use the proper scheduler initialization from day99_completed
    serial_puts("About to start multithreading\n");
    schedule();
}