#include <stddef.h>

#include "test_framework.h"

// Constants from kernel.h that we need for testing
#define MAX_THREADS 4
#define THREAD_STACK_SIZE 1024
#define MAX_COUNTER_VALUE 65535

// Thread state enum
typedef enum {
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_SLEEPING,
    THREAD_BLOCKED
} thread_state_t;

// Thread control block structure (simplified for testing)
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

// Function declarations for the functions we're testing
int validate_thread_params(void (*func)(void), int display_row,
                           uint32_t* delay_ticks);
void initialize_thread_stack(thread_t* thread, void (*func)(void));
void configure_thread_attributes(thread_t* thread, uint32_t delay_ticks,
                                 int display_row);
int add_thread_to_ready_list(thread_t* thread);

// Test function declarations
void test_validate_thread_params(void);
void test_initialize_thread_stack(void);
void test_configure_thread_attributes(void);
void test_add_thread_to_ready_list(void);
void test_thread_integration(void);

// Dummy test function for stack initialization
void dummy_test_function(void) {
    // Test function - does nothing
}

// Test main function
void test_main(void) {
    // Initialize serial first for debugging
    test_serial_init();
    test_serial_write_string(
        "TEST: Starting Thread Management test binary\r\n");

    test_init("Thread Management Functions");
    mock_init();

    test_validate_thread_params();
    test_initialize_thread_stack();
    test_configure_thread_attributes();
    test_add_thread_to_ready_list();
    test_thread_integration();

    test_summary();

    test_serial_write_string(
        "TEST: Thread tests completed, halting system\r\n");

    // Halt the system after tests complete
    while (1) {
        asm volatile("hlt");
    }
}

// Test thread parameter validation
void test_validate_thread_params(void) {
    mock_reset();
    uint32_t delay_ticks = 100;

    // Test valid parameters
    TEST_ASSERT_EQ(0,
                   validate_thread_params(dummy_test_function, 5, &delay_ticks),
                   "Valid parameters accepted");

    // Test NULL function pointer
    TEST_ASSERT_EQ(-1, validate_thread_params(NULL, 5, &delay_ticks),
                   "NULL function rejected");

    // Test invalid display row (too high)
    TEST_ASSERT_EQ(
        -1, validate_thread_params(dummy_test_function, 25, &delay_ticks),
        "Invalid high display row rejected");

    // Test invalid display row (negative)
    TEST_ASSERT_EQ(
        -1, validate_thread_params(dummy_test_function, -1, &delay_ticks),
        "Negative display row rejected");

    // Test NULL delay_ticks pointer
    TEST_ASSERT_EQ(-1, validate_thread_params(dummy_test_function, 5, NULL),
                   "NULL delay_ticks rejected");
}

// Test thread stack initialization
void test_initialize_thread_stack(void) {
    thread_t test_thread;
    mock_reset();

    // Initialize with test function
    initialize_thread_stack(&test_thread, dummy_test_function);

    // Check that ESP points into the stack
    uint32_t stack_start = (uint32_t)test_thread.stack;
    uint32_t stack_end = stack_start + (THREAD_STACK_SIZE * sizeof(uint32_t));

    TEST_ASSERT(test_thread.esp >= stack_start && test_thread.esp < stack_end,
                "ESP within stack bounds");

    // ESP should point near the end of the stack (stack grows downward)
    TEST_ASSERT(test_thread.esp >
                    stack_start + (THREAD_STACK_SIZE * sizeof(uint32_t)) - 1024,
                "ESP near stack top");
}

// Test thread attribute configuration
void test_configure_thread_attributes(void) {
    thread_t test_thread;
    mock_reset();

    configure_thread_attributes(&test_thread, 50, 10);

    TEST_ASSERT_EQ(THREAD_READY, test_thread.state,
                   "Thread state set to READY");
    TEST_ASSERT_EQ(0, test_thread.counter, "Counter initialized to 0");
    TEST_ASSERT_EQ(50, test_thread.delay_ticks, "Delay ticks set correctly");
    TEST_ASSERT_EQ(10, test_thread.display_row, "Display row set correctly");
    TEST_ASSERT_EQ(0, test_thread.last_tick, "Last tick initialized to 0");
    TEST_ASSERT_EQ(0, test_thread.wake_up_tick,
                   "Wake up tick initialized to 0");
    TEST_ASSERT_NULL(test_thread.next_ready, "Next ready pointer NULL");
    TEST_ASSERT_NULL(test_thread.next_sleeping, "Next sleeping pointer NULL");
}

// Test adding thread to ready list
void test_add_thread_to_ready_list(void) {
    thread_t test_thread;
    mock_reset();

    // Configure thread first
    test_thread.state = THREAD_READY;

    // Test successful addition
    TEST_ASSERT_EQ(0, add_thread_to_ready_list(&test_thread),
                   "Thread added to ready list successfully");
}

// Test integrated thread creation process
void test_thread_integration(void) {
    thread_t test_thread;
    uint32_t delay_ticks = 75;
    mock_reset();

    // Test full thread creation sequence
    TEST_ASSERT_EQ(0,
                   validate_thread_params(dummy_test_function, 8, &delay_ticks),
                   "Integration: Parameters validated");

    initialize_thread_stack(&test_thread, dummy_test_function);
    TEST_ASSERT(test_thread.esp > (uint32_t)test_thread.stack,
                "Integration: Stack initialized");

    configure_thread_attributes(&test_thread, delay_ticks, 8);
    TEST_ASSERT_EQ(THREAD_READY, test_thread.state,
                   "Integration: Attributes configured");

    TEST_ASSERT_EQ(0, add_thread_to_ready_list(&test_thread),
                   "Integration: Added to ready list");
}

// Entry point for the test binary (called from test_entry.s)
// Note: test_main is called by the assembly entry point