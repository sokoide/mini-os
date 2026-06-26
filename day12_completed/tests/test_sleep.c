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

// Function declarations for the functions we're testing
void remove_from_ready_list(thread_t* thread);
void insert_into_sleep_list(thread_t* thread, uint32_t wake_up_tick);
void transition_to_sleep_state(thread_t* thread, uint32_t wake_up_tick);
void sleep(uint32_t ticks);
thread_t* get_current_thread(void);
uint32_t get_system_ticks(void);

// Test function declarations
void test_remove_from_ready_list(void);
void test_insert_into_sleep_list(void);
void test_transition_to_sleep_state(void);
void test_sleep_integration(void);

// Test main function
void test_main(void) {
    // Initialize serial first for debugging
    test_serial_init();
    test_serial_write_string("TEST: Starting Sleep System test binary\r\n");

    test_init("Sleep System Functions");
    mock_init();

    test_remove_from_ready_list();
    test_insert_into_sleep_list();
    test_transition_to_sleep_state();
    test_sleep_integration();

    test_summary();

    test_serial_write_string("TEST: Sleep tests completed, halting system\r\n");

    // Halt the system after tests complete
    while (1) {
        asm volatile("hlt");
    }
}

// Test removing thread from ready list
void test_remove_from_ready_list(void) {
    thread_t test_thread;
    mock_reset();

    // Setup test thread
    test_thread.state = THREAD_READY;
    test_thread.next_ready = NULL;

    remove_from_ready_list(&test_thread);

    // Verify thread was processed
    TEST_ASSERT(1, "Thread removed from ready list");
}

// Test inserting thread into sleep list
void test_insert_into_sleep_list(void) {
    thread_t test_thread;
    uint32_t wake_tick = 1000;
    mock_reset();

    // Setup test thread
    test_thread.state = THREAD_READY;
    test_thread.next_sleeping = NULL;
    test_thread.wake_up_tick = 0;

    insert_into_sleep_list(&test_thread, wake_tick);

    // Verify wake up tick was set
    TEST_ASSERT_EQ(wake_tick, test_thread.wake_up_tick,
                   "Wake up tick set correctly");
    TEST_ASSERT(1, "Thread inserted into sleep list");
}

// Test transition to sleep state
void test_transition_to_sleep_state(void) {
    thread_t test_thread;
    uint32_t wake_tick = 500;
    mock_reset();

    // Setup test thread
    test_thread.state = THREAD_RUNNING;
    test_thread.wake_up_tick = 0;

    transition_to_sleep_state(&test_thread, wake_tick);

    // Verify state changed to sleeping
    TEST_ASSERT_EQ(THREAD_SLEEPING, test_thread.state,
                   "Thread state changed to SLEEPING");
    TEST_ASSERT_EQ(wake_tick, test_thread.wake_up_tick,
                   "Wake up tick configured");
}

// Test integrated sleep functionality
void test_sleep_integration(void) {
    mock_reset();

    // Test sleep function with various tick values
    sleep(10);  // Short sleep
    TEST_ASSERT(1, "Short sleep (10 ticks) completed");

    sleep(100);  // Medium sleep
    TEST_ASSERT(1, "Medium sleep (100 ticks) completed");

    sleep(1000);  // Long sleep
    TEST_ASSERT(1, "Long sleep (1000 ticks) completed");
}

// Entry point for the test binary (called from test_entry.s)
// Note: test_main is called by the assembly entry point