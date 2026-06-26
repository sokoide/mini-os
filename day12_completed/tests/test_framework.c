#include "test_framework.h"

#include <stdint.h>

// Global test state
static test_results_t test_results = {0, 0, 0};
static char current_suite_name[64];

// Test framework implementation
void test_init(const char* test_suite_name) {
    test_results.total_tests = 0;
    test_results.passed_tests = 0;
    test_results.failed_tests = 0;

    // Copy suite name
    int i = 0;
    while (test_suite_name[i] && i < 63) {
        current_suite_name[i] = test_suite_name[i];
        i++;
    }
    current_suite_name[i] = '\0';

    test_serial_init();
    test_serial_write_string("\r\n=== Starting Test Suite: ");
    test_serial_write_string(current_suite_name);
    test_serial_write_string(" ===\r\n");
}

void test_pass(const char* test_name) {
    test_results.total_tests++;
    test_results.passed_tests++;

    test_serial_write_string("[PASS] ");
    test_serial_write_string(test_name);
    test_serial_write_string("\r\n");
}

void test_fail(const char* file, int line, const char* test_name) {
    test_results.total_tests++;
    test_results.failed_tests++;

    test_serial_write_string("[FAIL] ");
    test_serial_write_string(test_name);
    test_serial_write_string(" at ");
    test_serial_write_string(file);
    test_serial_write_string(":");

    // Convert line number to string
    char line_str[16];
    uint32_to_string(line, line_str);
    test_serial_write_string(line_str);
    test_serial_write_string("\r\n");
}

void test_fail_with_values(const char* file, int line, const char* test_name,
                           uint32_t expected, uint32_t actual) {
    test_results.total_tests++;
    test_results.failed_tests++;

    test_serial_write_string("[FAIL] ");
    test_serial_write_string(test_name);
    test_serial_write_string(" at ");
    test_serial_write_string(file);
    test_serial_write_string(":");

    char num_str[16];
    uint32_to_string(line, num_str);
    test_serial_write_string(num_str);
    test_serial_write_string(" - Expected: ");
    uint32_to_string(expected, num_str);
    test_serial_write_string(num_str);
    test_serial_write_string(", Actual: ");
    uint32_to_string(actual, num_str);
    test_serial_write_string(num_str);
    test_serial_write_string("\r\n");
}

void test_summary(void) {
    test_serial_write_string("\r\n=== Test Summary for ");
    test_serial_write_string(current_suite_name);
    test_serial_write_string(" ===\r\n");

    char num_str[16];
    test_serial_write_string("Total tests: ");
    uint32_to_string(test_results.total_tests, num_str);
    test_serial_write_string(num_str);
    test_serial_write_string("\r\n");

    test_serial_write_string("Passed: ");
    uint32_to_string(test_results.passed_tests, num_str);
    test_serial_write_string(num_str);
    test_serial_write_string("\r\n");

    test_serial_write_string("Failed: ");
    uint32_to_string(test_results.failed_tests, num_str);
    test_serial_write_string(num_str);
    test_serial_write_string("\r\n");

    if (test_results.failed_tests == 0) {
        test_serial_write_string("*** ALL TESTS PASSED ***\r\n");
    } else {
        test_serial_write_string("*** SOME TESTS FAILED ***\r\n");
    }
    test_serial_write_string("\r\n");
}

test_results_t* get_test_results(void) {
    return &test_results;
}

// Serial output for tests (simplified version for testing)
void test_serial_init(void) {
    // Simple serial initialization for tests
    outb(0x3F8 + 1, 0x00);  // Disable all interrupts
    outb(0x3F8 + 3, 0x80);  // Enable DLAB (set baud rate divisor)
    outb(0x3F8 + 0, 0x03);  // Set divisor to 3 (lo byte) 38400 baud
    outb(0x3F8 + 1, 0x00);  //                  (hi byte)
    outb(0x3F8 + 3, 0x03);  // 8 bits, no parity, one stop bit
    outb(0x3F8 + 2, 0xC7);  // Enable FIFO, clear them, with 14-byte threshold
    outb(0x3F8 + 4, 0x0B);  // IRQs enabled, RTS/DSR set
}

void test_serial_write_char(char c) {
    // Wait for transmit buffer to be empty
    while ((inb(0x3F8 + 5) & 0x20) == 0);
    outb(0x3F8, c);
}

void test_serial_write_string(const char* str) {
    while (*str) {
        test_serial_write_char(*str);
        str++;
    }
}

// Utility function to convert uint32 to string
void uint32_to_string(uint32_t value, char* buffer) {
    if (value == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }

    char temp[16];
    int i = 0;

    while (value > 0) {
        temp[i++] = '0' + (value % 10);
        value /= 10;
    }

    // Reverse string
    int j = 0;
    while (i > 0) {
        buffer[j++] = temp[--i];
    }
    buffer[j] = '\0';
}