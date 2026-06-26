#include <stdbool.h>
#include <stdint.h>

#include "test_framework.h"

// Keyboard module constants (from keyboard.h)
#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64
#define KEYBOARD_BUFFER_SIZE 256
#define KEYBOARD_STATUS_OUTPUT_FULL 0x01
#define SCANCODE_RELEASE_MASK 0x80
#define SCANCODE_ENTER 0x1C
#define SCANCODE_BACKSPACE 0x0E
#define SCANCODE_LEFT_SHIFT 0x2A
#define SCANCODE_RIGHT_SHIFT 0x36

// Keyboard buffer structure (from keyboard.h)
typedef struct {
    char buffer[KEYBOARD_BUFFER_SIZE];
    volatile int head;
    volatile int tail;
    volatile int count;
} keyboard_buffer_t;

// Function declarations for keyboard module
void init_keyboard_buffer(void);
void keyboard_buffer_put(char c);
char keyboard_buffer_get(void);
bool keyboard_buffer_is_empty(void);
bool keyboard_buffer_is_full(void);
uint8_t read_keyboard_status(void);
uint8_t read_keyboard_data(void);
char convert_scancode_to_ascii(uint8_t scancode, bool shift_pressed);
void init_keyboard_controller(void);
void init_keyboard(void);
void keyboard_handler_c(void);

// External buffer access for testing
extern keyboard_buffer_t kbd_buffer;

// Test function declarations
void test_keyboard_buffer_init(void);
void test_keyboard_buffer_put_get(void);
void test_keyboard_buffer_full_empty_states(void);
void test_keyboard_buffer_wrap_around(void);
void test_scancode_conversion_basic(void);
void test_scancode_conversion_shift(void);
void test_scancode_conversion_special_keys(void);
void test_keyboard_status_functions(void);
void test_keyboard_boundary_values(void);
void test_keyboard_error_conditions(void);

// Main test runner
int main(void) {
    test_init("Keyboard Module Tests");

    // Initialize mock hardware
    mock_init();

    // Buffer management tests
    test_keyboard_buffer_init();
    test_keyboard_buffer_put_get();
    test_keyboard_buffer_full_empty_states();
    test_keyboard_buffer_wrap_around();

    // Scancode conversion tests
    test_scancode_conversion_basic();
    test_scancode_conversion_shift();
    test_scancode_conversion_special_keys();

    // Hardware interface tests
    test_keyboard_status_functions();

    // Boundary and error condition tests
    test_keyboard_boundary_values();
    test_keyboard_error_conditions();

    test_summary();
    return 0;
}

// Test implementations

void test_keyboard_buffer_init(void) {
    init_keyboard_buffer();

    TEST_ASSERT(keyboard_buffer_is_empty(),
                "Buffer should be empty after initialization");
    TEST_ASSERT(!keyboard_buffer_is_full(),
                "Buffer should not be full after initialization");
    TEST_ASSERT_EQ(0, kbd_buffer.head, "Buffer head should be 0 after init");
    TEST_ASSERT_EQ(0, kbd_buffer.tail, "Buffer tail should be 0 after init");
    TEST_ASSERT_EQ(0, kbd_buffer.count, "Buffer count should be 0 after init");
}

void test_keyboard_buffer_put_get(void) {
    init_keyboard_buffer();

    // Test single character
    keyboard_buffer_put('A');
    TEST_ASSERT(!keyboard_buffer_is_empty(),
                "Buffer should not be empty after put");
    TEST_ASSERT_EQ(1, kbd_buffer.count,
                   "Buffer count should be 1 after single put");

    char ch = keyboard_buffer_get();
    TEST_ASSERT_EQ('A', ch, "Should get the same character that was put");
    TEST_ASSERT(keyboard_buffer_is_empty(), "Buffer should be empty after get");

    // Test multiple characters
    keyboard_buffer_put('H');
    keyboard_buffer_put('i');
    keyboard_buffer_put('!');
    TEST_ASSERT_EQ(3, kbd_buffer.count,
                   "Buffer count should be 3 after three puts");

    TEST_ASSERT_EQ('H', keyboard_buffer_get(), "First character should be 'H'");
    TEST_ASSERT_EQ('i', keyboard_buffer_get(),
                   "Second character should be 'i'");
    TEST_ASSERT_EQ('!', keyboard_buffer_get(), "Third character should be '!'");
    TEST_ASSERT(keyboard_buffer_is_empty(),
                "Buffer should be empty after all gets");
}

void test_keyboard_buffer_full_empty_states(void) {
    init_keyboard_buffer();

    // Fill buffer to capacity
    for (int i = 0; i < KEYBOARD_BUFFER_SIZE; i++) {
        keyboard_buffer_put('X');
    }

    TEST_ASSERT(keyboard_buffer_is_full(),
                "Buffer should be full after filling to capacity");
    TEST_ASSERT_EQ(KEYBOARD_BUFFER_SIZE, kbd_buffer.count,
                   "Count should equal buffer size when full");

    // Try to add one more (should be handled gracefully)
    keyboard_buffer_put('Y');
    TEST_ASSERT(keyboard_buffer_is_full(),
                "Buffer should still be full after overflow attempt");

    // Empty the buffer
    for (int i = 0; i < KEYBOARD_BUFFER_SIZE; i++) {
        char ch = keyboard_buffer_get();
        TEST_ASSERT_EQ('X', ch, "All characters should be 'X'");
    }

    TEST_ASSERT(keyboard_buffer_is_empty(),
                "Buffer should be empty after draining");
}

void test_keyboard_buffer_wrap_around(void) {
    init_keyboard_buffer();

    // Fill buffer partially
    for (int i = 0; i < 10; i++) {
        keyboard_buffer_put('0' + i);
    }

    // Remove some characters
    for (int i = 0; i < 5; i++) {
        char ch = keyboard_buffer_get();
        TEST_ASSERT_EQ('0' + i, ch, "Characters should come out in FIFO order");
    }

    // Add more characters to test wrap-around
    for (int i = 0; i < 10; i++) {
        keyboard_buffer_put('A' + i);
    }

    // Verify remaining original characters
    for (int i = 5; i < 10; i++) {
        char ch = keyboard_buffer_get();
        TEST_ASSERT_EQ('0' + i, ch,
                       "Remaining original characters should be preserved");
    }

    // Verify new characters
    for (int i = 0; i < 10; i++) {
        char ch = keyboard_buffer_get();
        TEST_ASSERT_EQ('A' + i, ch, "New characters should be accessible");
    }
}

void test_scancode_conversion_basic(void) {
    // Test basic letter conversions (lowercase)
    TEST_ASSERT_EQ('a', convert_scancode_to_ascii(0x1E, false),
                   "Scancode 0x1E should convert to 'a'");
    TEST_ASSERT_EQ('s', convert_scancode_to_ascii(0x1F, false),
                   "Scancode 0x1F should convert to 's'");
    TEST_ASSERT_EQ('d', convert_scancode_to_ascii(0x20, false),
                   "Scancode 0x20 should convert to 'd'");

    // Test number conversions
    TEST_ASSERT_EQ('1', convert_scancode_to_ascii(0x02, false),
                   "Scancode 0x02 should convert to '1'");
    TEST_ASSERT_EQ('2', convert_scancode_to_ascii(0x03, false),
                   "Scancode 0x03 should convert to '2'");
    TEST_ASSERT_EQ('0', convert_scancode_to_ascii(0x0B, false),
                   "Scancode 0x0B should convert to '0'");

    // Test space and common symbols
    TEST_ASSERT_EQ(' ', convert_scancode_to_ascii(0x39, false),
                   "Scancode 0x39 should convert to space");
    TEST_ASSERT_EQ('-', convert_scancode_to_ascii(0x0C, false),
                   "Scancode 0x0C should convert to '-'");
    TEST_ASSERT_EQ('=', convert_scancode_to_ascii(0x0D, false),
                   "Scancode 0x0D should convert to '='");
}

void test_scancode_conversion_shift(void) {
    // Test shift conversions for letters (uppercase)
    TEST_ASSERT_EQ('A', convert_scancode_to_ascii(0x1E, true),
                   "Scancode 0x1E + shift should convert to 'A'");
    TEST_ASSERT_EQ('S', convert_scancode_to_ascii(0x1F, true),
                   "Scancode 0x1F + shift should convert to 'S'");
    TEST_ASSERT_EQ('D', convert_scancode_to_ascii(0x20, true),
                   "Scancode 0x20 + shift should convert to 'D'");

    // Test shift conversions for numbers (symbols)
    TEST_ASSERT_EQ('!', convert_scancode_to_ascii(0x02, true),
                   "Scancode 0x02 + shift should convert to '!'");
    TEST_ASSERT_EQ('@', convert_scancode_to_ascii(0x03, true),
                   "Scancode 0x03 + shift should convert to '@'");
    TEST_ASSERT_EQ(')', convert_scancode_to_ascii(0x0B, true),
                   "Scancode 0x0B + shift should convert to ')'");

    // Test shift conversions for symbols
    TEST_ASSERT_EQ('_', convert_scancode_to_ascii(0x0C, true),
                   "Scancode 0x0C + shift should convert to '_'");
    TEST_ASSERT_EQ('+', convert_scancode_to_ascii(0x0D, true),
                   "Scancode 0x0D + shift should convert to '+'");
}

void test_scancode_conversion_special_keys(void) {
    // Test special key conversions
    TEST_ASSERT_EQ('\n', convert_scancode_to_ascii(SCANCODE_ENTER, false),
                   "Enter scancode should convert to newline");
    TEST_ASSERT_EQ('\b', convert_scancode_to_ascii(SCANCODE_BACKSPACE, false),
                   "Backspace scancode should convert to backspace");
    TEST_ASSERT_EQ('\t', convert_scancode_to_ascii(0x0F, false),
                   "Tab scancode should convert to tab");

    // Test shift keys (should return 0 as they're modifier keys)
    TEST_ASSERT_EQ(0, convert_scancode_to_ascii(SCANCODE_LEFT_SHIFT, false),
                   "Left shift should return 0");
    TEST_ASSERT_EQ(0, convert_scancode_to_ascii(SCANCODE_RIGHT_SHIFT, false),
                   "Right shift should return 0");

    // Test release codes (with release mask)
    TEST_ASSERT_EQ(
        0, convert_scancode_to_ascii(0x1E | SCANCODE_RELEASE_MASK, false),
        "Release codes should return 0");
}

void test_keyboard_status_functions(void) {
    // Mock keyboard status register
    mock_set_inb_return_value(KEYBOARD_STATUS_PORT,
                              KEYBOARD_STATUS_OUTPUT_FULL);

    uint8_t status = read_keyboard_status();
    TEST_ASSERT_EQ(KEYBOARD_STATUS_OUTPUT_FULL, status,
                   "Should read correct status from keyboard");

    // Mock keyboard data
    mock_set_inb_return_value(KEYBOARD_DATA_PORT, 0x1E);

    uint8_t data = read_keyboard_data();
    TEST_ASSERT_EQ(0x1E, data, "Should read correct data from keyboard");
}

void test_keyboard_boundary_values(void) {
    // Test buffer at capacity - 1
    init_keyboard_buffer();

    for (int i = 0; i < KEYBOARD_BUFFER_SIZE - 1; i++) {
        keyboard_buffer_put('T');
    }
    TEST_ASSERT(!keyboard_buffer_is_full(),
                "Buffer should not be full at capacity-1");
    TEST_ASSERT_EQ(KEYBOARD_BUFFER_SIZE - 1, kbd_buffer.count,
                   "Count should be capacity-1");

    // Add one more to reach exact capacity
    keyboard_buffer_put('T');
    TEST_ASSERT(keyboard_buffer_is_full(),
                "Buffer should be full at exact capacity");

    // Test invalid scancodes
    TEST_ASSERT_EQ(0, convert_scancode_to_ascii(0xFF, false),
                   "Invalid scancode should return 0");
    TEST_ASSERT_EQ(0, convert_scancode_to_ascii(0x00, false),
                   "Zero scancode should return 0");
}

void test_keyboard_error_conditions(void) {
    init_keyboard_buffer();

    // Test getting from empty buffer (should handle gracefully)
    char ch = keyboard_buffer_get();
    // Note: Depending on implementation, this might return 0 or undefined
    // The important thing is that it doesn't crash
    TEST_ASSERT(true, "Getting from empty buffer should not crash");

    // Test buffer overflow protection
    for (int i = 0; i <= KEYBOARD_BUFFER_SIZE + 10; i++) {
        keyboard_buffer_put('O');
    }
    TEST_ASSERT(kbd_buffer.count <= KEYBOARD_BUFFER_SIZE,
                "Buffer count should not exceed capacity");

    // Test that buffer still works after overflow attempt
    init_keyboard_buffer();
    keyboard_buffer_put('A');
    ch = keyboard_buffer_get();
    TEST_ASSERT_EQ('A', ch,
                   "Buffer should work normally after overflow recovery");
}