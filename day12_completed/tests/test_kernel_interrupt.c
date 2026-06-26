// Test-specific kernel functions for Interrupt System testing
// Only includes the interrupt-related functions to avoid linking conflicts

#include "test_framework.h"

// IDT structures from kernel.h
struct idt_entry {
    uint16_t base_low;
    uint16_t selector;
    uint8_t always0;
    uint8_t flags;
    uint16_t base_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

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
 * IDT structure setup function
 * 【役割】IDT（Interrupt Descriptor Table）の構造体初期化
 */
void setup_idt_structure(void) {
    debug_print("INTERRUPT: Setting up IDT structure");

    // In real implementation, we would:
    // 1. Initialize IDT array
    // 2. Set up IDT pointer
    // 3. Clear all entries initially

    debug_print("INTERRUPT: IDT structure initialized");
}

/*
 * Interrupt handlers registration function
 * 【役割】各割り込みハンドラをIDTに登録
 */
void register_interrupt_handlers(void) {
    debug_print("INTERRUPT: Registering interrupt handlers");

    // In real implementation, we would:
    // 1. Register timer interrupt handler (IRQ0 -> INT 32)
    // 2. Register keyboard handler if needed (IRQ1 -> INT 33)
    // 3. Register other system handlers

    debug_print("INTERRUPT: Timer interrupt handler registered");
    debug_print("INTERRUPT: Exception handlers registered");
}

/*
 * CPU interrupts enable function
 * 【役割】CPU割り込みの有効化（STI命令実行）
 */
void enable_cpu_interrupts(void) {
    debug_print("INTERRUPT: Enabling CPU interrupts");

    // In real implementation: asm volatile("sti");
    // For testing, we just log the action

    debug_print("INTERRUPT: CPU interrupts enabled (STI executed)");
}

/*
 * Complete interrupt system initialization
 * 【役割】割り込みシステム全体の初期化
 */
void init_interrupts(void) {
    debug_print("INTERRUPT: Starting complete interrupt initialization");

    // Initialize interrupt system in proper sequence
    setup_idt_structure();          // 1. Setup IDT structure
    register_interrupt_handlers();  // 2. Register handlers
    enable_cpu_interrupts();        // 3. Enable CPU interrupts

    print_at(20, 0, "Interrupt system initialized", 0x0A);
    debug_print("INTERRUPT: Complete interrupt system ready");
}