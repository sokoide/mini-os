#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdbool.h>
#include <stdint.h>

// Test framework macros and utilities
#define TEST_ASSERT(condition, message)             \
    do {                                            \
        if (!(condition)) {                         \
            test_fail(__FILE__, __LINE__, message); \
            return;                                 \
        }                                           \
        test_pass(message);                         \
    } while (0)

#define TEST_ASSERT_EQ(expected, actual, message)                            \
    do {                                                                     \
        if ((expected) != (actual)) {                                        \
            test_fail_with_values(__FILE__, __LINE__, message,               \
                                  (uint32_t)(expected), (uint32_t)(actual)); \
            return;                                                          \
        }                                                                    \
        test_pass(message);                                                  \
    } while (0)

#define TEST_ASSERT_NOT_NULL(ptr, message) TEST_ASSERT((ptr) != NULL, message)

#define TEST_ASSERT_NULL(ptr, message) TEST_ASSERT((ptr) == NULL, message)

// Test result tracking
typedef struct {
    int total_tests;
    int passed_tests;
    int failed_tests;
} test_results_t;

// Test framework functions
void test_init(const char* test_suite_name);
void test_pass(const char* test_name);
void test_fail(const char* file, int line, const char* test_name);
void test_fail_with_values(const char* file, int line, const char* test_name,
                           uint32_t expected, uint32_t actual);
void test_summary(void);
test_results_t* get_test_results(void);

// Mock hardware functions
void mock_init(void);
void mock_reset(void);
uint8_t mock_get_outb_call_count(uint16_t port);
uint8_t mock_get_last_outb_value(uint16_t port);
void mock_set_inb_return_value(uint16_t port, uint8_t value);

// Serial output for tests
void test_serial_init(void);
void test_serial_write_char(char c);
void test_serial_write_string(const char* str);

// Utility functions
void uint32_to_string(uint32_t value, char* buffer);
void uint_to_hex_string(uint32_t value, char* buffer);

// Hardware interface functions (implemented by mock or real hardware)
void outb(uint16_t port, uint8_t val);
uint8_t inb(uint16_t port);

#endif  // TEST_FRAMEWORK_H