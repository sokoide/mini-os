#include "test_framework.h"

// Constants from kernel.h that we need for testing
#define PIC_MASTER_COMMAND 0x20
#define PIC_MASTER_DATA 0x21

// Function declarations for the functions we're testing
void remap_pic(void);
void configure_interrupt_masks(void);
void enable_timer_interrupt(void);
void init_pic(void);

// Test function declarations
void test_remap_pic(void);
void test_configure_interrupt_masks(void);
void test_enable_timer_interrupt(void);
void test_init_pic_integration(void);

// Functions debug_print and print_at are provided by test_kernel_pic.c

// Test main function
void test_main(void) {
    // Initialize serial first for debugging
    test_serial_init();
    test_serial_write_string("TEST: Starting PIC test binary\r\n");

    test_init("PIC Functions");
    mock_init();

    test_remap_pic();
    test_configure_interrupt_masks();
    test_enable_timer_interrupt();
    test_init_pic_integration();

    test_summary();

    test_serial_write_string("TEST: Tests completed, halting system\r\n");

    // Halt the system after tests complete
    while (1) {
        asm volatile("hlt");
    }
}

// Test PIC remapping functionality
void test_remap_pic(void) {
    mock_reset();

    remap_pic();

    // Verify PIC master initialization sequence
    TEST_ASSERT_EQ(1, mock_get_outb_call_count(0x20),
                   "PIC master command port called once");
    TEST_ASSERT_EQ(0x11, mock_get_last_outb_value(0x20),
                   "PIC master command value correct");

    // Verify PIC master data configuration
    TEST_ASSERT_EQ(3, mock_get_outb_call_count(0x21),
                   "PIC master data port called 3 times");

    // Check the sequence of values written to master data port
    // Note: This is a simplified check - in a real implementation we'd track
    // the sequence
    TEST_ASSERT(mock_get_outb_call_count(0x21) == 3,
                "PIC master data configured with 3 writes");
}

// Test interrupt mask configuration
void test_configure_interrupt_masks(void) {
    mock_reset();

    configure_interrupt_masks();

    // Verify all interrupts are masked
    TEST_ASSERT_EQ(1, mock_get_outb_call_count(0x21),
                   "Interrupt mask register written once");
    TEST_ASSERT_EQ(0xFF, mock_get_last_outb_value(0x21),
                   "All interrupts masked (0xFF)");
}

// Test timer interrupt enablement
void test_enable_timer_interrupt(void) {
    mock_reset();

    enable_timer_interrupt();

    // Verify timer interrupt is enabled
    TEST_ASSERT_EQ(1, mock_get_outb_call_count(0x21),
                   "Timer interrupt mask updated");
    TEST_ASSERT_EQ(0xFE, mock_get_last_outb_value(0x21),
                   "Timer interrupt enabled (0xFE)");
}

// Test integrated PIC initialization
void test_init_pic_integration(void) {
    mock_reset();

    init_pic();

    // Verify the complete sequence was executed
    TEST_ASSERT(mock_get_outb_call_count(0x20) >= 1,
                "PIC command port accessed");
    TEST_ASSERT(mock_get_outb_call_count(0x21) >= 3,
                "PIC data port accessed multiple times");

    // Final state should have timer interrupt enabled
    TEST_ASSERT_EQ(0xFE, mock_get_last_outb_value(0x21),
                   "Final state: timer interrupt enabled");
}

// Entry point for the test binary (called from test_entry.s)
// Note: test_main is called by the assembly entry point