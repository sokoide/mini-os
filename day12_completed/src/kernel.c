// ============================================================================
// Day 12 完成版 - Day99アーキテクチャ Phase1
// マルチスレッドOS（キーボード入力・コンテキスト切り替え・タイマー統合）
// ============================================================================

#include "kernel.h"

#include "debug_utils.h"
#include "keyboard.h"

// ============================================================================
// 基本型定義（nostdincのため手動定義）
// ============================================================================
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef int int32_t;
typedef unsigned long size_t;
#define NULL ((void*)0)
#define false 0
#define true 1

// ============================================================================
// グローバル変数と外部宣言
// ============================================================================

// アセンブリ関数の外部宣言（context_switch.sで実装）
extern void context_switch(uint32_t* old_esp, uint32_t new_esp);
extern void initial_context_switch(uint32_t new_esp);

// グローバルカーネルコンテキスト
static kernel_context_t k_context;

// ━━━ VGA テキストモード表示管理 ━━━
// VGAメモリバッファ（0xB8000 = テキストモード用メモリ領域）
static volatile uint16_t* const VGA_MEM = (uint16_t*)0xB8000;
// 現在のカーソル位置（X座標、Y座標）
static uint16_t cx, cy;
// 現在の文字色属性（前景色・背景色）
static uint8_t col = 0x0F;
// VGAエントリ作成：文字と属性を16bitに合成
static inline uint16_t ve(char c, uint8_t a) {
    return (uint16_t)c | ((uint16_t)a << 8);
}
// VGA文字色設定：前景色と背景色を指定
void vga_set_color(vga_color_t f, vga_color_t b) {
    col = (uint8_t)f | ((uint8_t)b << 4);
}
// VGAカーソル移動：画面上の指定位置にカーソルを移動
void vga_move_cursor(uint16_t x, uint16_t y) {
    cx = x;
    cy = y;
    // 線形位置計算（Y * 幅 + X）
    uint16_t p = y * VGA_WIDTH + x;
    // VGAコントローラーレジスタに位置設定
    outb(0x3D4, 14);  // カーソル位置上位バイト
    outb(0x3D5, (p >> 8) & 0xFF);
    outb(0x3D4, 15);  // カーソル位置下位バイト
    outb(0x3D5, p & 0xFF);
}
// VGA画面クリア：全画面を空白文字で埋めカーソルを左上に
void vga_clear(void) {
    for (uint16_t y = 0; y < VGA_HEIGHT; y++)
        for (uint16_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEM[y * VGA_WIDTH + x] = ve(' ', col);
    vga_move_cursor(0, 0);
}
// VGA文字出力：1文字を現在のカーソル位置に表示
void vga_putc(char c) {
    if (c == '\n') {
        // 改行文字の場合、次の行の先頭に移動
        cx = 0;
        cy++;
        vga_move_cursor(cx, cy);
        return;
    }
    // 指定位置に文字を書き込み
    VGA_MEM[cy * VGA_WIDTH + cx] = ve(c, col);
    // カーソルを次の位置に進める（行末で改行）
    if (++cx >= VGA_WIDTH) {
        cx = 0;
        cy++;
    }
    vga_move_cursor(cx, cy);
}
// VGA文字列出力：NULL終端文字列を順次表示
void vga_puts(const char* s) {
    while (*s) vga_putc(*s++);
}
// VGA数値出力：32bit整数を十進数文字列で表示
void vga_putnum(uint32_t n) {
    int i = 0;
    if (n == 0) {
        vga_putc('0');
        return;
    }
    uint32_t x = n;
    char r[10];  // 数字文字一時格納用配列
    // 下位桁か分離して配列に格納
    while (x) {
        r[i++] = '0' + (x % 10);
        x /= 10;
    }
    // 逆順で出力（上位桁から）
    while (i--) vga_putc(r[i]);
}
// VGA初期化：白文字・黒背景で画面クリア
void vga_init(void) {
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_clear();
}

// 画面クリア関数（day99_completed互換）
void clear_screen(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA_MEM[i] = ve(' ', col);
    }
}

// 指定行クリア関数
void clear_line(int row) {
    if (row < 0 || row >= VGA_HEIGHT)
        return;

    for (int col = 0; col < VGA_WIDTH; col++) {
        VGA_MEM[row * VGA_WIDTH + col] = ve(' ', col);
    }
}

// 指定位置への文字列表示関数（day99_completed互換）
void print_at(int row, int col, const char* str, uint8_t color) {
    if (row < 0 || row >= VGA_HEIGHT || col < 0 || col >= VGA_WIDTH) {
        return;
    }

    volatile uint16_t* pos = VGA_MEM + row * VGA_WIDTH + col;
    while (*str && col < VGA_WIDTH) {
        *pos++ = (*str++ | (color << 8));
        col++;
    }
}

// ============================================================================
// カーネルコンテキストアクセサ関数
// ============================================================================

kernel_context_t* get_kernel_context(void) {
    return &k_context;
}

thread_t* get_current_thread(void) {
    return get_kernel_context()->current_thread;
}

uint32_t get_system_ticks(void) {
    return get_kernel_context()->system_ticks;
}

// ============================================================================
// システム情報表示関数
// ============================================================================

void display_system_info(void) {
    print_at(0, 0, "Timer-based Multi-threaded OS with Context Switching",
             VGA_COLOR_WHITE);
    print_at(2, 0, "System Information:", VGA_COLOR_YELLOW);
    print_at(3, 2, "Timer Frequency: 100Hz (10ms intervals)", VGA_COLOR_GRAY);
    print_at(4, 2, "Scheduling: Preemptive Round-Robin", VGA_COLOR_GRAY);
    print_at(5, 2, "Context Switch: Hardware timer interrupt", VGA_COLOR_GRAY);

    print_at(7, 0, "Thread Information:", VGA_COLOR_YELLOW);
    print_at(8, 2,
             "Thread 1: Counter updates every 1.0 second, checking the counter "
             "every 10ms",
             VGA_COLOR_GRAY);
    print_at(9, 2,
             "Thread 2: Counter updates every 1.5 seconds, checking the "
             "counter every 10ms",
             VGA_COLOR_GRAY);
    print_at(10, 2,
             "Thread 3: Keyboard input thread blocked by BLOCK_REASON_KEYBOARD",
             VGA_COLOR_GRAY);

    print_at(12, 0, "Live Thread Status:", VGA_COLOR_RED);
}

// ============================================================================
// シリアルI/O関数群
// ============================================================================

void init_serial(void) {
    outb(SERIAL_PORT_COM1 + 1, SERIAL_INT_DISABLE);
    outb(SERIAL_PORT_COM1 + 3, SERIAL_DLAB_ENABLE);
    outb(SERIAL_PORT_COM1 + 0, SERIAL_BAUD_38400_LOW);
    outb(SERIAL_PORT_COM1 + 1, SERIAL_BAUD_38400_HIGH);
    outb(SERIAL_PORT_COM1 + 3, SERIAL_8N1_CONFIG);
    outb(SERIAL_PORT_COM1 + 2, SERIAL_FIFO_ENABLE);
    outb(SERIAL_PORT_COM1 + 4, SERIAL_MODEM_READY);
}

void serial_write_char(char c) {
    while ((inb(SERIAL_PORT_COM1 + 5) & SERIAL_TRANSMIT_READY) == 0);
    outb(SERIAL_PORT_COM1, c);
}

void serial_write_string(const char* str) {
    while (*str) {
        serial_write_char(*str);
        str++;
    }
}

void serial_puthex(uint32_t v) {
    const char* h = "0123456789ABCDEF";
    for (int i = 7; i >= 0; i--) serial_write_char(h[(v >> (i * 4)) & 0xF]);
}

// Duplicate declarations removed - moved to top of file

// ============================================================================
// 関数プロトタイプ宣言
// ============================================================================

void schedule(void);
void sleep(uint32_t ticks);
static void init_kernel_context(void);
static void init_basic_systems(void);
static void init_interrupt_and_io_systems(void);
static void init_thread_system(void);
static void kernel_main_loop(void);

// ============================================================================
// スレッド管理関数群
// ============================================================================

// --- Thread Creation ---

os_result_t validate_thread_params(void (*func)(void), int display_row,
                                   uint32_t* delay_ticks) {
    if (!func) {
        serial_write_string(
            "ERROR: create_thread called with NULL function pointer\n");
        return OS_ERROR_NULL_POINTER;
    }

    if (display_row < 0 || display_row >= 25) {
        serial_write_string(
            "ERROR: create_thread called with invalid display_row\n");
        return OS_ERROR_INVALID_PARAMETER;
    }

    if (*delay_ticks == 0) {
        serial_write_string(
            "WARNING: create_thread called with delay_ticks=0, using 1\n");
        *delay_ticks = 1;
    }

    return OS_SUCCESS;
}

void initialize_thread_stack(thread_t* thread, void (*func)(void)) {
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

void configure_thread_attributes(thread_t* thread, uint32_t delay_ticks,
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

os_result_t add_thread_to_ready_list(thread_t* thread) {
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
            serial_write_string("ERROR: Thread list appears corrupted\n");
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
        serial_write_string(
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
        serial_write_string("ERROR: Maximum number of threads exceeded\n");
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

    serial_write_string("SUCCESS: Thread created successfully\n");
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
void remap_pic(void) {
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

// Forward declaration for set_masks function
static void set_masks(uint8_t m, uint8_t s);
static void idt_init(void);

// 割り込みマスク設定関数
void configure_interrupt_masks(void) {
    serial_write_string("PIC: Configuring interrupt masks");
    outb(PIC1_DATA, PIC_MASK_ALL_DISABLED);
    serial_write_string("PIC: All interrupts masked");
}

// タイマーとキーボード割り込み有効化
void enable_timer_interrupt(void) {
    serial_write_string("PIC: Enabling timer and keyboard interrupts");
    set_masks(PIC_MASK_TIMER_KEYBOARD, PIC_MASK_ALL_DISABLED);
    serial_write_string(
        "PIC: Timer (IRQ0) and Keyboard (IRQ1) interrupts enabled");
}

// PIC初期化関数
void init_pic(void) {
    serial_write_string("PIC: Starting PIC initialization");
    remap_pic();
    configure_interrupt_masks();
    enable_timer_interrupt();
    serial_write_string("PIC: PIC configured: Timer interrupt enabled");
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

// PIT初期化（day99_completed互換）
void init_timer(uint32_t frequency) {
    uint32_t divisor = PIT_FREQUENCY / frequency;
    outb(PIT_CMD, PIT_MODE_SQUARE_WAVE);
    outb(PIT_CH0, divisor & MASK_LOW_BYTE);
    outb(PIT_CH0, (divisor >> SHIFT_HIGH_BYTE) & MASK_LOW_BYTE);
    print_at(20, 0, "Timer initialized: 100Hz (10ms intervals)",
             VGA_COLOR_GREEN);
}

// 割り込みシステム初期化
void init_interrupts(void) {
    serial_write_string("INTERRUPTS: Starting interrupt system initialization");
    idt_init();
    init_pit_100hz();
    init_pic();
    enable_cpu_interrupts();
    serial_write_string("INTERRUPTS: Interrupt system initialized");
}

// CPU割り込み有効化関数
void enable_cpu_interrupts(void) {
    asm volatile("sti");
    serial_write_string("CPU: Interrupts enabled");
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
void set_gate(int n, uint32_t h) {
    idt[n].lo = h & 0xFFFF;
    idt[n].sel = 0x08;
    idt[n].zero = 0;
    idt[n].flags = 0x8E;
    idt[n].hi = (h >> 16) & 0xFFFF;
}

// Forward declaration for idt_init function
static void idt_init(void);

void setup_idt_structure(void) {
    debug_print("IDT: IDT structure configured and loaded");
    idtr.limit = sizeof(idt) - 1;
    idtr.base = (uint32_t)&idt[0];
    lidt(&idtr);
}

void register_interrupt_handlers(void) {
    debug_print("IDT: Timer interrupt handler registered");
    debug_print("IDT: Keyboard interrupt handler registered");
    set_gate(32, (uint32_t)timer_interrupt_handler);
    set_gate(33, (uint32_t)keyboard_interrupt_handler);
}
extern void isr0(void);
extern void isr3(void);
extern void isr6(void);
extern void isr13(void);
extern void isr14(void);
static void idt_init(void) {
    for (int i = 0; i < 256; i++) set_gate(i, 0);
    set_gate(0, (uint32_t)isr0);
    set_gate(3, (uint32_t)isr3);
    set_gate(6, (uint32_t)isr6);
    set_gate(13, (uint32_t)isr13);
    set_gate(14, (uint32_t)isr14);
    set_gate(32, (uint32_t)timer_interrupt_handler);
    set_gate(33, (uint32_t)keyboard_interrupt_handler);
    idtr.limit = sizeof(idt) - 1;
    idtr.base = (uint32_t)&idt[0];
    lidt(&idtr);
}

// 初期化ログ（PIC/IDTの設定確認用）
static void debug_log_interrupt_setup(void) {
    serial_write_string("DEBUG: IDT gates set: 32=");
    serial_puthex((uint32_t)timer_interrupt_handler);
    serial_write_string(" 33=");
    serial_puthex((uint32_t)keyboard_interrupt_handler);
    serial_write_string("\n");
    uint8_t m = inb(PIC1_DATA), s = inb(PIC2_DATA);
    serial_write_string("DEBUG: PIC masks M=");
    serial_puthex(m);
    serial_write_string(" S=");
    serial_puthex(s);
    serial_write_string("\n");
}

// ============================================================================
// スレッドスケジューラー関数群
// ============================================================================

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
            serial_write_string("WAKE_THREAD ticks=");
            serial_puthex(ctx->system_ticks);
            serial_write_string(" wake_up=");
            serial_puthex(current->wake_up_tick);
            serial_write_string("\n");
            unblock_and_requeue_thread(current, prev);
            woke_count++;
        } else {
            prev = current;
        }
        current = next;
    }

    if (woke_count > 0) {
        serial_write_string("WOKE_UP_THREADS count=");
        serial_puthex(woke_count);
        serial_write_string("\n");
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
    } else {  // FIFO for keyboard and other reasons
        if (!ctx->blocked_thread_list) {
            ctx->blocked_thread_list = thread;
        } else {
            thread_t* current = ctx->blocked_thread_list;
            while (current->next_blocked) {
                current = current->next_blocked;
            }
            current->next_blocked = thread;
        }
    }

    asm volatile("sti");
}

// --- PS/2 keyboard initialization moved to keyboard.c ---

// ============================================================================
// スレッドスケジューラー関数群
// ============================================================================

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

static inline int is_scheduler_locked(void) {
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
        serial_write_string("SCHED_LOCKED\n");
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
        serial_write_string("INITIAL_THREAD_SEL\n");
        handle_initial_thread_selection();
        return;
    }

    // 現在のスレッドがブロック状態の場合、強制的に次のスレッドに切り替え
    if (ctx->current_thread->state == THREAD_BLOCKED) {
        serial_write_string("BLOCKED_SCHEDULING\n");
        handle_blocked_thread_scheduling();
        return;
    }
    serial_write_string("PERFORM_SWITCH\n");
    perform_thread_switch();
}

void sleep(uint32_t ticks) {
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

// ============================================================================
// 割り込みハンドラー関数群
// ============================================================================

void timer_handler_c(void) {
    get_kernel_context()->system_ticks++;

    // PIC EOI
    outb(0x20, 0x20);

    // Schedule on timer tick (every 100ms)
    schedule();
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
    serial_write_string("EXC vec=");
    serial_puthex(f->int_no);
    serial_write_string("\n");
}

// タイマー割り込みハンドラ（C部分）
void timer_interrupt_handler_c(void) {
    kernel_context_t* ctx = get_kernel_context();
    ctx->system_ticks++;

    // 16回に1回デバッグ出力
    if ((ctx->system_ticks & 0xF) == 0) {
        serial_write_string("TIMER_");
        serial_puthex(ctx->system_ticks);
        serial_write_string(" ticks=");
        serial_puthex(ctx->system_ticks);
        serial_write_string("\n");
    }

    // PIC EOI
    outb(0x20, 0x20);

    // スケジューラー呼び出し
    schedule();
}

// ============================================================================
// スレッド実装関数群
// ============================================================================

// idleスレッド
static void idle_thread(void) {
    for (;;) {
        asm volatile("hlt");
    }
}

// デモスレッド
static void threadA(void) {
    serial_write_string("threadA start\n");

    for (;;) {
        thread_t* self = get_current_thread();
        self->counter++;
        __asm__ volatile("cli");
        vga_move_cursor(2, 13);
        vga_set_color(VGA_BROWN, VGA_BLACK);
        vga_puts("Thread A: ");
        vga_putnum(self->counter);
        vga_puts("        ");  // Clear previous digits
        __asm__ volatile("sti");
        if ((self->counter & 0x1FF) == 0)
            serial_write_string("threadA alive\n");

        sleep(50);  // 500ms
    }
}
static void threadB(void) {
    serial_write_string("threadB start\n");

    for (;;) {
        thread_t* self = get_current_thread();
        self->counter++;
        __asm__ volatile("cli");
        vga_move_cursor(2, 14);
        vga_set_color(VGA_CYAN, VGA_BLACK);
        vga_puts("Thread B: ");
        vga_putnum(self->counter);
        vga_puts("        ");  // Clear previous digits
        __asm__ volatile("sti");
        if ((self->counter & 0x1FF) == 0)
            serial_write_string("threadB alive\n");

        sleep(75);  // 750ms
    }
}

// ============================================================================
// システム初期化関数群
// ============================================================================

static void init_kernel_context(void) {
    k_context.current_thread = NULL;
    k_context.ready_thread_list = NULL;
    k_context.blocked_thread_list = NULL;
    k_context.system_ticks = 0;
    k_context.scheduler_lock_count = 0;
}

// スレッド関数群（day99_completed互換）
void threadC(void) {
    char ch;

    serial_write_string("threadC start\n");

    // キーボード入力デモンストレーション
    print_at(15, 2,
             "Thread C: Keyboard Input Demo - Press keys:", VGA_COLOR_WHITE);
    print_at(16, 3, "Press 'q' to quit, Enter for string input",
             VGA_COLOR_GRAY);

    while (1) {
        print_at(17, 3, "Press a key (or 's' for string): ", VGA_COLOR_WHITE);
        ch = getchar_blocking();

        if (ch == 'q' || ch == 'Q') {
            print_at(16, 3, "Keyboard demo terminated.            ",
                     VGA_COLOR_RED);
            print_at(17, 3, "                                     ",
                     VGA_COLOR_RED);
            break;
        } else if (ch == 's' || ch == 'S') {
            print_at(18, 3, " Enter string: ", VGA_COLOR_YELLOW);
            char input_buffer[256];
            read_line(input_buffer, sizeof(input_buffer));
            clear_line(19);
            print_at(19, 3, "You entered: ", VGA_COLOR_GREEN);
            print_at(19, 16, input_buffer, VGA_COLOR_WHITE);
        } else {
            char msg[32];
            msg[0] = 'K';
            msg[1] = 'e';
            msg[2] = 'y';
            msg[3] = ':';
            msg[4] = ' ';
            msg[5] = ch;
            msg[6] = ' ';
            msg[7] = '(';
            itoa((uint32_t)ch, &msg[8], 10);
            int len = 8;
            while (msg[len] != 0) len++;
            msg[len] = ')';
            msg[len + 1] = 0;

            clear_line(18);
            print_at(18, 3, msg, VGA_COLOR_MAGENTA);
        }

        sleep(5);
    }

    // スレッドが終了する場合は無限ループで待機
    print_at(20, 3, "Thread C finished. Sleeping forever.", VGA_COLOR_RED);
    while (1) {
        asm volatile("hlt");
    }
}

// Duplicate serial functions removed - moved to Serial I/O section

// ============================================================================
// ユーティリティ関数群
// ============================================================================

// 整数を文字列に変換する関数
void itoa(uint32_t value, char* buffer, int base) {
    char* p = buffer;
    char *p1, *p2;
    uint32_t digits = 0;

    if (value == 0) {
        *p++ = '0';
        *p = '\0';
        return;
    }

    while (value) {
        uint32_t remainder = value % base;
        *p++ = (remainder < 10) ? remainder + '0' : remainder + 'A' - 10;
        value /= base;
        digits++;
    }

    *p = '\0';

    // 文字列を逆順にする
    p1 = buffer;
    p2 = buffer + digits - 1;
    while (p1 < p2) {
        char tmp = *p1;
        *p1 = *p2;
        *p2 = tmp;
        p1++;
        p2--;
    }
}

void kernel_main(void) {
    init_kernel_context();
    init_basic_systems();
    init_interrupt_and_io_systems();
    init_thread_system();
    kernel_main_loop();
}

static void init_basic_systems(void) {
    init_serial();
    debug_print("KERNEL: Serial port initialized");

    clear_screen();
    debug_print("KERNEL: Screen cleared");

    display_system_info();
    debug_print("KERNEL: System info displayed");
}

static void init_interrupt_and_io_systems(void) {
    debug_print("KERNEL: About to initialize interrupts");
    init_interrupts();
    debug_print("KERNEL: Interrupts initialized");

    // Log detailed interrupt setup information
    debug_log_interrupt_setup();

    debug_print("KERNEL: About to initialize keyboard");
    keyboard_init();
    debug_print("KERNEL: Keyboard initialized");
}

static void init_thread_system(void) {
    debug_print("KERNEL: About to create threads");

    thread_t* thread_a;
    os_result_t result = create_thread(threadA, 100, 14, &thread_a);
    if (OS_FAILURE_CHECK(result)) {
        debug_print("ERROR: Failed to create thread A");
    } else {
        debug_print("KERNEL: Thread A created");
    }

    thread_t* thread_b;
    result = create_thread(threadB, 150, 15, &thread_b);
    if (OS_FAILURE_CHECK(result)) {
        debug_print("ERROR: Failed to create thread B");
    } else {
        debug_print("KERNEL: Thread B created");
    }

    thread_t* thread_c;
    result = create_thread(threadC, 200, 16, &thread_c);
    if (OS_FAILURE_CHECK(result)) {
        debug_print("ERROR: Failed to create thread C");
    } else {
        debug_print("KERNEL: Thread C created");
    }

    debug_print("KERNEL: Thread system initialized");
    debug_print("KERNEL: Waiting for timer interrupt to start scheduling");

    thread_t* thread_idle;
    result = create_thread(idle_thread, 1, 0, &thread_idle);
    if (OS_FAILURE_CHECK(result)) {
        debug_print("ERROR: Failed to create idle thread");
    } else {
        debug_print("KERNEL: idle thread created");
    }

    debug_print("KERNEL: Thread system initialized");
    debug_print("KERNEL: Waiting for timer interrupt to start scheduling");
}

static void kernel_main_loop(void) {
    debug_print("KERNEL: Waiting for timer interrupt");
}

// Keep the old kmain for backward compatibility
void kmain(void) {
    kernel_main();
}
