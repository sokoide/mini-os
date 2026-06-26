// Test-specific kernel functions for Thread Management testing
// Only includes the thread-related functions to avoid linking conflicts

#include <stddef.h>

#include "test_framework.h"

// Constants from kernel.h
#define MAX_THREADS 4
#define THREAD_STACK_SIZE 1024
#define MAX_COUNTER_VALUE 65535
#define DISPLAY_LINE_LENGTH 25
#define MAX_THREAD_NAME_LEN 15

// Thread state enum
typedef enum {
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_SLEEPING,
    THREAD_BLOCKED
} thread_state_t;

// Thread control block structure
typedef struct thread {
    uint32_t stack[THREAD_STACK_SIZE];
    thread_state_t state;
    uint32_t counter;
    uint32_t delay_ticks;
    uint32_t last_tick;
    uint32_t wake_up_tick;
    int display_row;
    struct thread* next_ready;
    struct thread* next_sleeping;
    uint32_t esp;
} thread_t;

// Simple debug function for tests
void debug_print(const char* message) {
    test_serial_write_string("[KERNEL_DEBUG] ");
    test_serial_write_string(message);
    test_serial_write_string("\r\n");
}

// Simple print function for tests
void print_at(int row, int col, const char* str, uint8_t color) {
    (void)row;
    (void)col;
    (void)color;
    test_serial_write_string("[KERNEL_PRINT] ");
    test_serial_write_string(str);
    test_serial_write_string("\r\n");
}

/*
 * Thread parameter validation function
 * 【役割】スレッド作成パラメータの妥当性を検証
 */
int validate_thread_params(void (*func)(void), int display_row,
                           uint32_t* delay_ticks) {
    debug_print("THREAD: Validating thread parameters");

    // Function pointer validation
    if (func == NULL) {
        debug_print("THREAD: ERROR - NULL function pointer");
        return -1;
    }

    // Display row validation (0-24 for 25 lines)
    if (display_row < 0 || display_row >= DISPLAY_LINE_LENGTH) {
        debug_print("THREAD: ERROR - Invalid display row");
        return -1;
    }

    // Delay ticks pointer validation
    if (delay_ticks == NULL) {
        debug_print("THREAD: ERROR - NULL delay_ticks pointer");
        return -1;
    }

    // Delay ticks value validation and adjustment
    if (*delay_ticks == 0) {
        debug_print("THREAD: WARNING - Zero delay adjusted to 10");
        *delay_ticks = 10;
    }

    if (*delay_ticks > MAX_COUNTER_VALUE) {
        debug_print("THREAD: WARNING - Delay clamped to maximum");
        *delay_ticks = MAX_COUNTER_VALUE;
    }

    debug_print("THREAD: Parameters validated successfully");
    return 0;
}

/*
 * Thread stack initialization function
 * 【役割】スレッドスタックの初期化とコンテキスト設定
 */
void initialize_thread_stack(thread_t* thread, void (*func)(void)) {
    debug_print("THREAD: Initializing thread stack");

    if (thread == NULL || func == NULL) {
        debug_print("THREAD: ERROR - NULL pointer in stack init");
        return;
    }

    // Set ESP to top of stack (stacks grow downward)
    // Leave some space for stack frame setup
    thread->esp = (uint32_t)&thread->stack[THREAD_STACK_SIZE - 16];

    // For real implementation, we would setup initial stack frame here
    // with function pointer, return address, etc.
    // For testing, we just ensure ESP is properly positioned

    debug_print("THREAD: Stack initialized with ESP set");
}

/*
 * Thread attributes configuration function
 * 【役割】スレッド属性の設定（状態、カウンター、表示位置等）
 */
void configure_thread_attributes(thread_t* thread, uint32_t delay_ticks,
                                 int display_row) {
    debug_print("THREAD: Configuring thread attributes");

    if (thread == NULL) {
        debug_print("THREAD: ERROR - NULL thread pointer");
        return;
    }

    // Initialize thread state
    thread->state = THREAD_READY;
    thread->counter = 0;
    thread->delay_ticks = delay_ticks;
    thread->display_row = display_row;
    thread->last_tick = 0;
    thread->wake_up_tick = 0;

    // Initialize list pointers
    thread->next_ready = NULL;
    thread->next_sleeping = NULL;

    debug_print("THREAD: Thread attributes configured");
}

/*
 * Add thread to ready list function
 * 【役割】スレッドをready状態のリストに追加
 */
int add_thread_to_ready_list(thread_t* thread) {
    debug_print("THREAD: Adding thread to ready list");

    if (thread == NULL) {
        debug_print("THREAD: ERROR - Cannot add NULL thread");
        return -1;
    }

    if (thread->state != THREAD_READY) {
        debug_print("THREAD: ERROR - Thread not in READY state");
        return -1;
    }

    // In real implementation, we would add to circular ready list
    // For testing, we just mark as successfully added
    debug_print("THREAD: Thread added to ready list successfully");
    return 0;
}