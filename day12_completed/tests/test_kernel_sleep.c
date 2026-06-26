// Test-specific kernel functions for Sleep System testing
// Only includes the sleep-related functions to avoid linking conflicts

#include <stddef.h>

#include "test_framework.h"

// Thread state enum
typedef enum {
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_SLEEPING,
    THREAD_BLOCKED
} thread_state_t;

// Simplified thread structure for testing
typedef struct thread {
    thread_state_t state;
    uint32_t wake_up_tick;
    struct thread* next_ready;
    struct thread* next_sleeping;
} thread_t;

// Mock global variables for testing
static thread_t* current_thread_mock = NULL;
static uint32_t system_ticks_mock = 1000;

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
 * Remove thread from ready list function
 * 【役割】指定されたスレッドをready状態のリストから削除
 */
void remove_from_ready_list(thread_t* thread) {
    debug_print("SLEEP: Removing thread from ready list");

    if (thread == NULL) {
        debug_print("SLEEP: ERROR - Cannot remove NULL thread");
        return;
    }

    // In real implementation, we would search and remove from circular ready
    // list For testing, we just log the operation
    thread->next_ready = NULL;

    debug_print("SLEEP: Thread removed from ready list");
}

/*
 * Insert thread into sleep list function
 * 【役割】指定されたスレッドをsleep状態のリストに挿入（時刻順）
 */
void insert_into_sleep_list(thread_t* thread, uint32_t wake_up_tick) {
    debug_print("SLEEP: Inserting thread into sleep list");

    if (thread == NULL) {
        debug_print("SLEEP: ERROR - Cannot insert NULL thread");
        return;
    }

    // Set wake up time
    thread->wake_up_tick = wake_up_tick;

    // In real implementation, we would insert in order by wake_up_tick
    // For testing, we just set the pointer
    thread->next_sleeping = NULL;

    debug_print(
        "SLEEP: Thread inserted into sleep list (ordered by wake time)");
}

/*
 * Transition thread to sleep state function
 * 【役割】スレッドの状態をSLEEPINGに変更し、起床時刻を設定
 */
void transition_to_sleep_state(thread_t* thread, uint32_t wake_up_tick) {
    debug_print("SLEEP: Transitioning thread to sleep state");

    if (thread == NULL) {
        debug_print("SLEEP: ERROR - Cannot transition NULL thread");
        return;
    }

    // Change state to sleeping
    thread->state = THREAD_SLEEPING;
    thread->wake_up_tick = wake_up_tick;

    debug_print("SLEEP: Thread transitioned to SLEEPING state");
}

/*
 * Get current thread function (mock for testing)
 * 【役割】現在実行中のスレッドを取得
 */
thread_t* get_current_thread(void) {
    static thread_t mock_thread = {THREAD_RUNNING, 0, NULL, NULL};
    if (current_thread_mock == NULL) {
        current_thread_mock = &mock_thread;
    }
    return current_thread_mock;
}

/*
 * Get system ticks function (mock for testing)
 * 【役割】システムティック数を取得
 */
uint32_t get_system_ticks(void) {
    return system_ticks_mock;
}

/*
 * Sleep function - puts current thread to sleep
 * 【役割】現在のスレッドを指定ティック数だけスリープさせる
 */
void sleep(uint32_t ticks) {
    debug_print("SLEEP: Starting sleep operation");

    if (ticks == 0) {
        debug_print("SLEEP: Zero ticks - no sleep needed");
        return;
    }

    thread_t* current = get_current_thread();
    if (current == NULL) {
        debug_print("SLEEP: ERROR - No current thread");
        return;
    }

    uint32_t current_ticks = get_system_ticks();
    uint32_t wake_up_tick = current_ticks + ticks;

    // Execute sleep sequence
    remove_from_ready_list(current);                // 1. Remove from ready list
    insert_into_sleep_list(current, wake_up_tick);  // 2. Insert into sleep list
    transition_to_sleep_state(current, wake_up_tick);  // 3. Change state

    debug_print("SLEEP: Thread put to sleep successfully");

    // In real implementation, we would call schedule() here
    // For testing, we just complete the operation
}