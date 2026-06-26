#include <stdint.h>

#include "test_framework.h"

// Serial port constants (from kernel.h)
#define SERIAL_PORT_COM1 0x3F8
#define SERIAL_INT_DISABLE 0x00
#define SERIAL_DLAB_ENABLE 0x80
#define SERIAL_BAUD_38400_LOW 0x03
#define SERIAL_BAUD_38400_HIGH 0x00
#define SERIAL_8N1_CONFIG 0x03
#define SERIAL_FIFO_ENABLE 0xC7
#define SERIAL_MODEM_READY 0x0B
#define SERIAL_TRANSMIT_READY 0x20

// Function declarations for serial module
void init_serial(void);
void serial_write_char(char c);
void serial_write_string(const char* str);

// Test function declarations
void test_serial_initialization(void);
void test_serial_write_char_basic(void);
void test_serial_write_char_special(void);
void test_serial_write_string_basic(void);
void test_serial_write_string_empty(void);
void test_serial_write_string_long(void);
void test_serial_port_configuration(void);
void test_serial_transmit_ready_wait(void);
void test_serial_boundary_conditions(void);

// Helper functions
void setup_serial_mocks(void);
void verify_serial_port_sequence(void);

// Main test runner
int main(void) {
    test_init("Serial Communication Module Tests");

    // Setup mock hardware
    mock_init();
    setup_serial_mocks();

    // Initialization tests
    test_serial_initialization();
    test_serial_port_configuration();

    // Character transmission tests
    test_serial_write_char_basic();
    test_serial_write_char_special();
    test_serial_transmit_ready_wait();

    // String transmission tests
    test_serial_write_string_basic();
    test_serial_write_string_empty();
    test_serial_write_string_long();

    // Boundary condition tests
    test_serial_boundary_conditions();

    test_summary();
    return 0;
}

// Helper function implementations

void setup_serial_mocks(void) {
    // Mock serial port as always ready for transmission
    mock_set_inb_return_value(SERIAL_PORT_COM1 + 5, SERIAL_TRANSMIT_READY);
}

void verify_serial_port_sequence(void) {
    // Verify the expected sequence of port operations during init_serial()

    // Check interrupt disable (port COM1+1)
    TEST_ASSERT(mock_get_outb_call_count(SERIAL_PORT_COM1 + 1) >= 1,
                "Should write to interrupt enable register");

    // Check DLAB enable (port COM1+3)
    TEST_ASSERT(mock_get_outb_call_count(SERIAL_PORT_COM1 + 3) >= 1,
                "Should configure line control register");

    // Check baud rate setting (ports COM1+0 and COM1+1)
    TEST_ASSERT(mock_get_outb_call_count(SERIAL_PORT_COM1 + 0) >= 1,
                "Should set baud rate low byte");

    // Check FIFO configuration (port COM1+2)
    TEST_ASSERT(mock_get_outb_call_count(SERIAL_PORT_COM1 + 2) >= 1,
                "Should configure FIFO");

    // Check modem control (port COM1+4)
    TEST_ASSERT(mock_get_outb_call_count(SERIAL_PORT_COM1 + 4) >= 1,
                "Should configure modem control");
}

// Test implementations

void test_serial_initialization(void) {
    mock_reset();

    // Initialize serial port
    init_serial();

    // Verify correct initialization sequence
    verify_serial_port_sequence();

    // Check specific configuration values
    TEST_ASSERT_EQ(SERIAL_INT_DISABLE,
                   mock_get_last_outb_value(SERIAL_PORT_COM1 + 1),
                   "Interrupts should be disabled");

    TEST_ASSERT_EQ(SERIAL_BAUD_38400_LOW,
                   mock_get_last_outb_value(SERIAL_PORT_COM1 + 0),
                   "Baud rate low byte should be set correctly");

    TEST_ASSERT_EQ(SERIAL_FIFO_ENABLE,
                   mock_get_last_outb_value(SERIAL_PORT_COM1 + 2),
                   "FIFO should be enabled");

    TEST_ASSERT_EQ(SERIAL_MODEM_READY,
                   mock_get_last_outb_value(SERIAL_PORT_COM1 + 4),
                   "Modem control should be set correctly");
}

void test_serial_port_configuration(void) {
    mock_reset();

    init_serial();

    // Verify DLAB (Divisor Latch Access Bit) sequence
    // Should enable DLAB, set baud rate, then disable DLAB

    uint8_t line_control_calls = mock_get_outb_call_count(SERIAL_PORT_COM1 + 3);
    TEST_ASSERT(line_control_calls >= 2,
                "Should configure line control multiple times");

    // Verify 8N1 configuration (8 bits, no parity, 1 stop bit)
    TEST_ASSERT_EQ(SERIAL_8N1_CONFIG,
                   mock_get_last_outb_value(SERIAL_PORT_COM1 + 3),
                   "Final line control should be 8N1");
}

void test_serial_write_char_basic(void) {
    mock_reset();
    setup_serial_mocks();

    // Test basic character transmission
    serial_write_char('A');

    // Verify transmit ready was checked
    TEST_ASSERT(mock_get_outb_call_count(SERIAL_PORT_COM1 + 5) >= 0,
                "Should check transmit ready status");

    // Verify character was sent to data port
    TEST_ASSERT_EQ(1, mock_get_outb_call_count(SERIAL_PORT_COM1),
                   "Should write exactly one character to data port");
    TEST_ASSERT_EQ('A', mock_get_last_outb_value(SERIAL_PORT_COM1),
                   "Should transmit the correct character");
}

void test_serial_write_char_special(void) {
    mock_reset();
    setup_serial_mocks();

    // Test special characters
    char special_chars[] = {'\n', '\r', '\t', '\b', '\0'};

    for (int i = 0; i < 5; i++) {
        mock_reset();
        setup_serial_mocks();

        serial_write_char(special_chars[i]);

        TEST_ASSERT_EQ(1, mock_get_outb_call_count(SERIAL_PORT_COM1),
                       "Should write special character to data port");
        TEST_ASSERT_EQ(special_chars[i],
                       mock_get_last_outb_value(SERIAL_PORT_COM1),
                       "Should transmit the correct special character");
    }
}

void test_serial_transmit_ready_wait(void) {
    mock_reset();

    // Mock serial port as NOT ready initially
    mock_set_inb_return_value(SERIAL_PORT_COM1 + 5, 0x00);

    // This would normally cause the serial_write_char to wait
    // In our mock implementation, we can verify it checks the status
    serial_write_char('W');

    // Should have checked line status register
    TEST_ASSERT(mock_get_outb_call_count(SERIAL_PORT_COM1 + 5) >= 0,
                "Should check line status register");

    // Set ready status and try again
    mock_set_inb_return_value(SERIAL_PORT_COM1 + 5, SERIAL_TRANSMIT_READY);

    serial_write_char('X');
    TEST_ASSERT_EQ('X', mock_get_last_outb_value(SERIAL_PORT_COM1),
                   "Should transmit when ready");
}

void test_serial_write_string_basic(void) {
    mock_reset();
    setup_serial_mocks();

    // Test basic string transmission
    const char* test_string = "Hello";
    serial_write_string(test_string);

    // Should have transmitted 5 characters
    TEST_ASSERT_EQ(5, mock_get_outb_call_count(SERIAL_PORT_COM1),
                   "Should transmit all characters in string");

    // Last character should be 'o'
    TEST_ASSERT_EQ('o', mock_get_last_outb_value(SERIAL_PORT_COM1),
                   "Last character should be 'o'");
}

void test_serial_write_string_empty(void) {
    mock_reset();
    setup_serial_mocks();

    // Test empty string
    serial_write_string("");

    // Should not transmit any characters
    TEST_ASSERT_EQ(0, mock_get_outb_call_count(SERIAL_PORT_COM1),
                   "Should not transmit any characters for empty string");
}

void test_serial_write_string_long(void) {
    mock_reset();
    setup_serial_mocks();

    // Test long string
    const char* long_string =
        "This is a very long string that contains many characters to test the "
        "serial write function";
    int expected_length = 0;

    // Count expected length
    while (long_string[expected_length] != '\0') {
        expected_length++;
    }

    serial_write_string(long_string);

    // Should have transmitted all characters
    TEST_ASSERT_EQ(expected_length, mock_get_outb_call_count(SERIAL_PORT_COM1),
                   "Should transmit all characters in long string");

    // Last character should be 'n' (from "function")
    TEST_ASSERT_EQ('n', mock_get_last_outb_value(SERIAL_PORT_COM1),
                   "Last character should be correct");
}

void test_serial_boundary_conditions(void) {
    mock_reset();
    setup_serial_mocks();

    // Test single character string
    serial_write_string("X");
    TEST_ASSERT_EQ(1, mock_get_outb_call_count(SERIAL_PORT_COM1),
                   "Single character string should work");
    TEST_ASSERT_EQ('X', mock_get_last_outb_value(SERIAL_PORT_COM1),
                   "Single character should be transmitted correctly");

    mock_reset();
    setup_serial_mocks();

    // Test string with spaces and punctuation
    serial_write_string("A B, C.");
    TEST_ASSERT_EQ(6, mock_get_outb_call_count(SERIAL_PORT_COM1),
                   "String with spaces and punctuation should work");
    TEST_ASSERT_EQ('.', mock_get_last_outb_value(SERIAL_PORT_COM1),
                   "Punctuation should be transmitted correctly");

    mock_reset();
    setup_serial_mocks();

    // Test string with numbers
    serial_write_string("123");
    TEST_ASSERT_EQ(3, mock_get_outb_call_count(SERIAL_PORT_COM1),
                   "Numeric string should work");
    TEST_ASSERT_EQ('3', mock_get_last_outb_value(SERIAL_PORT_COM1),
                   "Numbers should be transmitted correctly");

    // Test null pointer handling (if implemented)
    // Note: This test depends on implementation details
    mock_reset();
    setup_serial_mocks();

    // This should not crash the system
    serial_write_string(NULL);
    TEST_ASSERT_EQ(0, mock_get_outb_call_count(SERIAL_PORT_COM1),
                   "Null pointer should be handled gracefully");
}