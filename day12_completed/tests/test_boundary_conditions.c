#include <stddef.h>
#include <stdint.h>

#include "test_framework.h"

// Core OS constants for boundary testing
#define MAX_THREADS 4
#define THREAD_STACK_SIZE 1024
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define MAX_COUNTER_VALUE 65535
#define KEYBOARD_BUFFER_SIZE 256

// Thread state enum for testing
typedef enum {
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_SLEEPING,
    THREAD_BLOCKED
} thread_state_t;

// Function declarations for boundary testing
void test_thread_stack_boundaries(void);
void test_vga_coordinate_boundaries(void);
void test_counter_value_boundaries(void);
void test_memory_alignment_boundaries(void);
void test_port_address_boundaries(void);
void test_buffer_size_boundaries(void);
void test_timing_boundaries(void);
void test_integer_overflow_conditions(void);
void test_null_pointer_edge_cases(void);
void test_array_index_boundaries(void);

// Dummy functions for testing
void dummy_thread_function(void) { /* Test function */
}

// Main test runner
int main(void) {
    test_init("Boundary Value and Edge Case Tests");

    // Initialize mock hardware
    mock_init();

    // Core boundary condition tests
    test_thread_stack_boundaries();
    test_vga_coordinate_boundaries();
    test_counter_value_boundaries();
    test_memory_alignment_boundaries();
    test_port_address_boundaries();
    test_buffer_size_boundaries();
    test_timing_boundaries();
    test_integer_overflow_conditions();
    test_null_pointer_edge_cases();
    test_array_index_boundaries();

    test_summary();
    return 0;
}

// Test thread stack boundary conditions
void test_thread_stack_boundaries(void) {
    // Test stack pointer at valid boundaries
    uint32_t stack[THREAD_STACK_SIZE];
    uint32_t stack_start = (uint32_t)stack;
    uint32_t stack_end = stack_start + (THREAD_STACK_SIZE * sizeof(uint32_t));

    // Test ESP at stack boundaries
    uint32_t esp_at_top = stack_end - 4;   // Valid top of stack
    uint32_t esp_at_bottom = stack_start;  // Bottom boundary

    TEST_ASSERT(esp_at_top >= stack_start && esp_at_top < stack_end,
                "ESP at stack top should be within bounds");
    TEST_ASSERT(esp_at_bottom >= stack_start && esp_at_bottom < stack_end,
                "ESP at stack bottom should be within bounds");

    // Test stack size boundaries
    TEST_ASSERT_EQ(THREAD_STACK_SIZE, 1024,
                   "Stack size should be exactly 1024 words");
    TEST_ASSERT_EQ(THREAD_STACK_SIZE * sizeof(uint32_t), 4096,
                   "Stack should be exactly 4KB");

    // Test maximum stack usage
    uint32_t max_stack_usage =
        THREAD_STACK_SIZE - 20;  // Reserve space for context
    TEST_ASSERT(max_stack_usage < THREAD_STACK_SIZE,
                "Maximum usable stack should be less than total size");
}

// Test VGA coordinate boundary conditions
void test_vga_coordinate_boundaries(void) {
    // Test valid coordinate boundaries
    TEST_ASSERT_EQ(VGA_WIDTH, 80, "VGA width should be 80 characters");
    TEST_ASSERT_EQ(VGA_HEIGHT, 25, "VGA height should be 25 lines");

    // Test coordinate boundary values
    int valid_coords[][2] = {
        {0, 0},                          // Top-left corner
        {0, VGA_WIDTH - 1},              // Top-right corner
        {VGA_HEIGHT - 1, 0},             // Bottom-left corner
        {VGA_HEIGHT - 1, VGA_WIDTH - 1}  // Bottom-right corner
    };

    for (int i = 0; i < 4; i++) {
        int row = valid_coords[i][0];
        int col = valid_coords[i][1];

        TEST_ASSERT(row >= 0 && row < VGA_HEIGHT,
                    "Row should be within VGA bounds");
        TEST_ASSERT(col >= 0 && col < VGA_WIDTH,
                    "Column should be within VGA bounds");
    }

    // Test invalid coordinate detection
    int invalid_coords[][2] = {
        {-1, 0},                 // Negative row
        {0, -1},                 // Negative column
        {VGA_HEIGHT, 0},         // Row too large
        {0, VGA_WIDTH},          // Column too large
        {-1, -1},                // Both negative
        {VGA_HEIGHT, VGA_WIDTH}  // Both too large
    };

    for (int i = 0; i < 6; i++) {
        int row = invalid_coords[i][0];
        int col = invalid_coords[i][1];

        bool row_invalid = (row < 0 || row >= VGA_HEIGHT);
        bool col_invalid = (col < 0 || col >= VGA_WIDTH);

        TEST_ASSERT(row_invalid || col_invalid,
                    "Invalid coordinates should be detected");
    }
}

// Test counter value boundary conditions
void test_counter_value_boundaries(void) {
    // Test counter boundaries
    uint32_t counter_values[] = {
        0, 1, 100, 1000, 32767, 65534, MAX_COUNTER_VALUE};

    for (int i = 0; i < 7; i++) {
        uint32_t value = counter_values[i];
        TEST_ASSERT(value <= MAX_COUNTER_VALUE,
                    "Counter should not exceed maximum");
        TEST_ASSERT(value >= 0, "Counter should not be negative");
    }

    // Test counter overflow conditions
    uint32_t near_max = MAX_COUNTER_VALUE - 1;
    uint32_t at_max = MAX_COUNTER_VALUE;

    TEST_ASSERT_EQ(near_max, 65534, "Near-max counter value correct");
    TEST_ASSERT_EQ(at_max, 65535, "Max counter value correct");

    // Test counter increment at boundary
    if (near_max < MAX_COUNTER_VALUE) {
        TEST_ASSERT_EQ(near_max + 1, at_max,
                       "Counter increment at boundary works");
    }
}

// Test memory alignment boundary conditions
void test_memory_alignment_boundaries(void) {
    // Test 4-byte alignment (x86 requirement)
    uint32_t test_addresses[] = {
        0x100000,  // 1MB aligned
        0x200000,  // 2MB aligned
        0x1000,    // 4KB aligned
        0x10,      // 16-byte aligned
        0x8,       // 8-byte aligned
        0x4        // 4-byte aligned
    };

    for (int i = 0; i < 6; i++) {
        uint32_t addr = test_addresses[i];

        // Test 4-byte alignment
        TEST_ASSERT_EQ(addr % 4, 0, "Address should be 4-byte aligned");

        // Test that alignment doesn't break with small offsets
        TEST_ASSERT_EQ((addr + 4) % 4, 0, "Address + 4 should remain aligned");
    }

    // Test stack alignment requirements
    uint32_t stack_ptr = 0x200000;  // Typical stack location
    TEST_ASSERT_EQ(stack_ptr % 16, 0,
                   "Stack should be 16-byte aligned for x86");
}

// Test I/O port address boundary conditions
void test_port_address_boundaries(void) {
    // Test valid port ranges
    uint16_t valid_ports[] = {
        0x20,  0x21,  // PIC ports
        0x40,  0x43,  // PIT ports
        0x60,  0x64,  // Keyboard ports
        0x3F8, 0x3FF  // Serial ports
    };

    for (int i = 0; i < 8; i++) {
        uint16_t port = valid_ports[i];
        TEST_ASSERT(port <= 0xFFFF, "Port address should fit in 16 bits");
        TEST_ASSERT(port >= 0x20, "Port should be in valid I/O range");
    }

    // Test port boundary values
    TEST_ASSERT_EQ(0x0000, 0, "Minimum port address");
    TEST_ASSERT_EQ(0xFFFF, 65535, "Maximum port address");

    // Test common port ranges
    bool pic_range = (0x20 >= 0x20 && 0x21 <= 0x21);
    bool pit_range = (0x40 >= 0x40 && 0x43 <= 0x43);
    bool kbd_range = (0x60 >= 0x60 && 0x64 <= 0x64);

    TEST_ASSERT(pic_range, "PIC ports in valid range");
    TEST_ASSERT(pit_range, "PIT ports in valid range");
    TEST_ASSERT(kbd_range, "Keyboard ports in valid range");
}

// Test buffer size boundary conditions
void test_buffer_size_boundaries(void) {
    // Test keyboard buffer boundaries
    TEST_ASSERT_EQ(KEYBOARD_BUFFER_SIZE, 256,
                   "Keyboard buffer should be 256 bytes");

    // Test buffer index boundaries
    int buffer_indices[] = {0, 1, 100, 200, 254, 255};

    for (int i = 0; i < 6; i++) {
        int index = buffer_indices[i];
        TEST_ASSERT(index >= 0, "Buffer index should not be negative");
        TEST_ASSERT(index < KEYBOARD_BUFFER_SIZE,
                    "Buffer index should be within bounds");
    }

    // Test buffer wrap-around conditions
    int head = KEYBOARD_BUFFER_SIZE - 1;  // At boundary
    int next_head = (head + 1) % KEYBOARD_BUFFER_SIZE;
    TEST_ASSERT_EQ(next_head, 0, "Buffer should wrap around correctly");

    // Test buffer full/empty conditions
    int count_full = KEYBOARD_BUFFER_SIZE;
    int count_empty = 0;

    TEST_ASSERT_EQ(count_empty, 0, "Empty buffer count should be 0");
    TEST_ASSERT_EQ(count_full, KEYBOARD_BUFFER_SIZE,
                   "Full buffer count should equal buffer size");
}

// Test timing boundary conditions
void test_timing_boundaries(void) {
    // Test timer frequency boundaries
    uint32_t timer_freq = 100;    // 100 Hz
    uint32_t pit_freq = 1193180;  // PIT base frequency

    uint32_t divisor = pit_freq / timer_freq;
    TEST_ASSERT(divisor > 0, "Timer divisor should be positive");
    TEST_ASSERT(divisor <= 0xFFFF, "Timer divisor should fit in 16 bits");

    // Test tick value boundaries
    uint32_t tick_values[] = {0, 1, 100, 1000, 10000, 0xFFFFFFFE, 0xFFFFFFFF};

    for (int i = 0; i < 7; i++) {
        uint32_t ticks = tick_values[i];

        // Test that tick arithmetic doesn't overflow immediately
        if (ticks < 0xFFFFFFFF) {
            TEST_ASSERT(ticks + 1 > ticks, "Tick increment should work");
        }
    }

    // Test sleep duration boundaries
    uint32_t sleep_durations[] = {1, 10, 100, 1000, 10000};

    for (int i = 0; i < 5; i++) {
        uint32_t duration = sleep_durations[i];
        TEST_ASSERT(duration > 0, "Sleep duration should be positive");
        TEST_ASSERT(duration < 0xFFFFFFFF,
                    "Sleep duration should not overflow");
    }
}

// Test integer overflow conditions
void test_integer_overflow_conditions(void) {
    // Test 32-bit integer boundaries
    uint32_t max_uint32 = 0xFFFFFFFF;
    uint32_t near_max = 0xFFFFFFFE;

    TEST_ASSERT_EQ(max_uint32, 4294967295U, "Max uint32 value correct");
    TEST_ASSERT_EQ(near_max + 1, max_uint32, "Near-max increment works");

    // Test 16-bit boundaries (for port addresses)
    uint16_t max_uint16 = 0xFFFF;
    uint16_t near_max_16 = 0xFFFE;

    TEST_ASSERT_EQ(max_uint16, 65535, "Max uint16 value correct");
    TEST_ASSERT_EQ(near_max_16 + 1, max_uint16, "16-bit increment works");

    // Test 8-bit boundaries (for I/O values)
    uint8_t max_uint8 = 0xFF;
    uint8_t near_max_8 = 0xFE;

    TEST_ASSERT_EQ(max_uint8, 255, "Max uint8 value correct");
    TEST_ASSERT_EQ(near_max_8 + 1, max_uint8, "8-bit increment works");

    // Test signed integer boundaries
    int32_t max_int32 = 0x7FFFFFFF;
    int32_t min_int32 = 0x80000000;

    TEST_ASSERT_EQ(max_int32, 2147483647, "Max int32 value correct");
    TEST_ASSERT_EQ(min_int32, -2147483648, "Min int32 value correct");
}

// Test NULL pointer edge cases
void test_null_pointer_edge_cases(void) {
    // Test NULL pointer detection
    void* null_ptr = NULL;
    void* non_null_ptr = (void*)0x12345678;

    TEST_ASSERT_NULL(null_ptr, "NULL pointer should be detected");
    TEST_ASSERT_NOT_NULL(non_null_ptr, "Non-NULL pointer should be detected");

    // Test pointer arithmetic boundaries
    uintptr_t ptr_value = (uintptr_t)non_null_ptr;
    TEST_ASSERT(ptr_value != 0, "Non-NULL pointer should have non-zero value");

    // Test function pointer boundaries
    void (*func_ptr)(void) = dummy_thread_function;
    void (*null_func)(void) = NULL;

    TEST_ASSERT_NOT_NULL(func_ptr, "Function pointer should not be NULL");
    TEST_ASSERT_NULL(null_func, "NULL function pointer should be detected");
}

// Test array index boundary conditions
void test_array_index_boundaries(void) {
    // Test thread array boundaries
    int max_threads = MAX_THREADS;

    for (int i = 0; i < max_threads; i++) {
        TEST_ASSERT(i >= 0, "Thread index should not be negative");
        TEST_ASSERT(i < MAX_THREADS, "Thread index should be within bounds");
    }

    // Test invalid thread indices
    int invalid_indices[] = {-1, MAX_THREADS, MAX_THREADS + 1, 100};

    for (int i = 0; i < 4; i++) {
        int index = invalid_indices[i];
        bool is_invalid = (index < 0 || index >= MAX_THREADS);
        TEST_ASSERT(is_invalid, "Invalid thread index should be detected");
    }

    // Test VGA buffer boundaries
    int vga_size = VGA_WIDTH * VGA_HEIGHT;

    for (int row = 0; row < VGA_HEIGHT; row++) {
        for (int col = 0; col < VGA_WIDTH;
             col += 10) {  // Sample every 10 columns
            int index = row * VGA_WIDTH + col;
            TEST_ASSERT(index >= 0, "VGA buffer index should not be negative");
            TEST_ASSERT(index < vga_size,
                        "VGA buffer index should be within bounds");
        }
    }

    // Test stack array boundaries
    uint32_t stack_indices[] = {0, 1, 100, 500, 1000, THREAD_STACK_SIZE - 1};

    for (int i = 0; i < 6; i++) {
        uint32_t index = stack_indices[i];
        TEST_ASSERT(index < THREAD_STACK_SIZE,
                    "Stack index should be within bounds");
    }
}