# Day 11: Keyboard Input System (PS/2 + Ring Buffer) ⌨️

## Today's Goal

Process asynchronous input from PS/2 keyboard and implement ring buffer and blocking I/O.

## Background

In Day 10, we added sleep functionality, but user input is also an important element in OS development. Today we'll build a system that processes input from PS/2 keyboard in interrupt-driven manner, manages data with ring buffer, and can efficiently wait for input with blocking API.

## New Concepts

-   **PS/2**: Serial interface connecting keyboard and PC. Sends scan codes and generates IRQ1 interrupts.
-   **Ring Buffer**: Buffer that stores data cyclically. Write side and read side can safely exchange data without collision.
-   **Blocking I/O**: I/O processing method that puts threads in waiting state until input is obtained.

## Learning Content

-   PS/2 keyboard hardware interface
-   Scan code → ASCII character conversion
-   Interrupt-driven asynchronous input processing
-   Thread-safe ring buffer implementation
-   Blocking mechanism for I/O waiting
-   Integration of keyboard interrupt (IRQ1) and timer interrupt (IRQ0)

## Task List

-   [ ] Research PS/2 keyboard hardware interface and understand scan codes
-   [ ] Implement conversion table from scan codes to ASCII characters
-   [ ] Implement ring buffer for safe storage of asynchronous data
-   [ ] Implement IRQ1 interrupt handler to process keyboard input
-   [ ] Remove PIC IRQ1 mask to enable keyboard interrupts
-   [ ] Implement blocking and wake-up for keyboard waiting threads
-   [ ] Fix to clean code without warnings
-   [ ] Test keyboard input in QEMU and verify asynchronous processing

## Configuration

```
boot/boot.s, boot/kernel_entry.s
boot/interrupt.s        # IRQ0(timer) + IRQ1(keyboard)
boot/context_switch.s   # Context switch + initial_context_switch
io.h, vga.h
kernel.c                # Scheduler with keyboard input
Makefile
```

## Keyboard Input Implementation Method

In Day 11, keyboard input is implemented using **interrupt-driven method**. The following components work together:

### Architecture Overview

```
⌨️  Key press
     ↓
🔌 PS/2 controller (scan code generation)
     ↓
⚡ IRQ1 interrupt generation
     ↓
🎯 keyboard_handler_c()
     ↓
🔄 Scan code → ASCII conversion
     ↓
📋 Store in ring buffer
     ↓
🔓 unblock_keyboard_threads()
     ↓
🏃 BLOCKED → READY thread return
```

### Implemented Components

1. **IRQ1 interrupt handler (`keyboard_handler_c`)**

    - Read scan codes from PS/2 controller
    - Convert to ASCII characters and store in ring buffer
    - Return keyboard waiting threads to READY state

2. **Ring buffer (`kbuf`)**

    - 128-byte circular buffer managing asynchronous input
    - `kbuf_push`: Character storage from interrupt handler
    - `kbuf_is_empty/full`: Buffer state checking

3. **Thread blocking mechanism**

    - `BLOCK_REASON_KEYBOARD`: Keyboard input waiting
    - `unblock_keyboard_threads`: Batch wake-up on input

4. **Demo threads**
    - `threadA/B`: Counter display and timer-based operation
    - `idle_thread`: CPU halt when all threads are blocked

## Integration with Scheduler

Day 11 implementation is based on the robust scheduling method introduced in Day 10:

-   **Role of `idle_thread`**: CPU halt when no executable threads exist
-   **Generic blocking mechanism**: Unified processing of timer waiting and keyboard waiting
-   **Efficient interrupt processing**: Cooperative operation of IRQ0 (timer) and IRQ1 (keyboard)

## Implementation Guide

### Complete Implementation Code

**Ring Buffer Implementation**

```c
// Keyboard buffer (ring buffer)
#define KBUF_SIZE 128
static volatile char kbuf[KBUF_SIZE];
static volatile uint32_t khead = 0, ktail = 0;

// Buffer state check
static inline int kbuf_is_empty(void) {
    return khead == ktail;
}

static inline int kbuf_is_full(void) {
    return ((khead + 1) % KBUF_SIZE) == ktail;
}

// Add character to buffer (called from interrupt handler)
static void kbuf_push(char c) {
    uint32_t nh = (khead + 1) % KBUF_SIZE;
    if (nh != ktail) {  // Only when buffer is not full
        kbuf[khead] = c;
        khead = nh;
    }
}
```

**Scan Code Conversion Table**

```c
// Scan code → ASCII conversion table for US keyboard layout
static const char scancode_map[128] = {
    0,   27,   '1',  '2', '3',  '4', '5', '6', '7', '8', '9', '0', '-',
    '=', '', '	', 'q', 'w',  'e', 'r', 't', 'y', 'u', 'i', 'o', 'p',
    '[', ']',  '
', 0,   'a',  's', 'd', 'f', 'g', 'h', 'j', 'k', 'l',
    ';', '\'', '`',  0,   '\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',',
    '.', '/',  0,    '*', 0,    ' ', 0,   0,   0,   0,   0,   0,   0,
    0,   0,    0,    0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,    0,    0,   0,    0,   0,   0,   0,   0,   0,   0,
    // Remaining initialized to 0 (F1-F10, numeric keypad, etc.)
    [60 ... 127] = 0
};
```

**Keyboard Interrupt Handler**

```c
// IRQ1: Keyboard
void keyboard_handler_c(void) {
    // Notify PIC of interrupt processing completion
    outb(PIC1_CMD, PIC_EOI);

    // Check keyboard data read availability
    uint8_t status = inb(PS2_STAT);
    if (!(status & 0x01)) {
        serial_puts("KEYBOARD: Interrupt fired but no data available
");
        return;
    }

    // Read scan code
    uint8_t scancode = inb(PS2_DATA);

    // Ignore key release operations (break code)
    if (scancode & 0x80) {
        return;
    }

    // Convert scan code to ASCII character
    char ch = (scancode < 128) ? scancode_map[scancode] : 0;

    if (ch != 0) {
        // Store in buffer
        kbuf_push(ch);

        // Immediately output to both VGA and serial
        __asm__ volatile("cli");

        // VGA output - display to the right of "Keyboard Input:"
        vga_move_cursor(16, 6);
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        vga_putc(ch);
        vga_putc(' ');  // Clear old character

        // Serial output
        serial_putc(ch);

        __asm__ volatile("sti");

        // Debug output (day12_completed format)
        serial_puts("KEY: ");
        serial_putc(ch);
        serial_puts(" (");
        serial_puthex(scancode);
        serial_puts(")
");

        // Return input waiting threads to READY (if any)
        unblock_keyboard_threads();
    }
}
```

**Keyboard Thread Unblocking**

```c
// Return all keyboard waiting threads to READY state
static void unblock_keyboard_threads(void) {
    asm volatile("cli");
    kernel_context_t* ctx = get_kernel_context();
    thread_t* current = ctx->blocked_thread_list;
    thread_t* prev = NULL;

    while (current) {
        thread_t* next = current->next_blocked;
        if (current->block_reason == BLOCK_REASON_KEYBOARD) {
            // Remove from BLOCKED list
            if (prev) {
                prev->next_blocked = current->next_blocked;
            } else {
                ctx->blocked_thread_list = current->next_blocked;
            }

            // Return to READY list
            current->state = THREAD_READY;
            current->block_reason = BLOCK_REASON_NONE;
            current->next_blocked = NULL;
            add_thread_to_ready_list(current);
        } else {
            prev = current;
        }
        current = next;
    }
    asm volatile("sti");
}
```

**Demo Thread Implementation**

```c
// Idle thread
static void idle_thread(void) {
    for (;;) {
        asm volatile("hlt");  // Halt CPU
    }
}

// Thread A (fast counter)
static void threadA(void) {
    serial_puts("threadA start
");
    for (;;) {
        thread_t* self = get_current_thread();
        self->counter++;
        __asm__ volatile("cli");
        vga_move_cursor(0, 2);
        vga_set_color(VGA_YELLOW, VGA_BLACK);
        vga_puts("A: ");
        vga_putnum(self->counter);
        vga_puts("        ");  // Clear previous number
        __asm__ volatile("sti");

        if ((self->counter & 0x1FF) == 0)
            serial_puts("threadA alive
");

        sleep(50);  // Wait 500ms
    }
}

// Thread B (medium speed counter)
static void threadB(void) {
    serial_puts("threadB start
");
    for (;;) {
        thread_t* self = get_current_thread();
        self->counter++;
        __asm__ volatile("cli");
        vga_move_cursor(0, 3);
        vga_set_color(VGA_CYAN, VGA_BLACK);
        vga_puts("B: ");
        vga_putnum(self->counter);
        vga_puts("        ");  // Clear previous number
        __asm__ volatile("sti");

        if ((self->counter & 0x1FF) == 0)
            serial_puts("threadB alive
");

        sleep(75);  // Wait 750ms
    }
}
```

**PIC Configuration and IDT Initialization**

```c
// PIC remapping (IRQ0-15 → 32-47)
static void remap_pic(void) {
    uint8_t a1 = inb(PIC1_DATA), a2 = inb(PIC2_DATA);

    outb(PIC1_CMD, 0x11);   // ICW1: 8086 mode, cascade
    outb(PIC2_CMD, 0x11);
    outb(PIC1_DATA, 0x20);  // ICW2: master PIC → 32-39
    outb(PIC2_DATA, 0x28);  // ICW2: slave PIC → 40-47
    outb(PIC1_DATA, 0x04);  // ICW3: slave on IRQ2
    outb(PIC2_DATA, 0x02);  // ICW3: cascade ID=2
    outb(PIC1_DATA, 0x01);  // ICW4: 8086 mode
    outb(PIC2_DATA, 0x01);

    outb(PIC1_DATA, a1);    // Restore original mask
    outb(PIC2_DATA, a2);
}

// IDT configuration
static void idt_init(void) {
    // Initialize all entries to 0
    for (int i = 0; i < 256; i++)
        set_gate(i, 0);

    // Exception handlers
    set_gate(0, (uint32_t)isr0);   // Divide by zero
    set_gate(3, (uint32_t)isr3);   // Breakpoint
    set_gate(6, (uint32_t)isr6);   // Invalid opcode
    set_gate(13, (uint32_t)isr13); // General protection fault
    set_gate(14, (uint32_t)isr14); // Page fault

    // Interrupt handlers
    set_gate(32, (uint32_t)timer_interrupt_handler);    // IRQ0
    set_gate(33, (uint32_t)keyboard_interrupt_handler); // IRQ1

    // Set IDTR register
    idtr.limit = sizeof(idt) - 1;
    idtr.base = (uint32_t)&idt[0];
    lidt(&idtr);
}

// Main initialization
void kmain(void) {
    // Basic initialization
    serial_init();
    vga_init();
    init_kernel_context();

    // Interrupt system initialization
    remap_pic();
    set_masks(0xFC, 0xFF);  // Enable only IRQ0,1 (0xFC = 11111100)
    idt_init();
    init_pit_100hz();

    // PS/2 keyboard initialization
    ps2_keyboard_init();

    serial_puts("PIC/IDT/PIT/PS2 ready, enabling interrupts
");
    __asm__ volatile("sti");

    // Thread creation
    thread_t *th1, *th2, *tidle;
    create_thread(threadA, 50, 2, &th1);
    create_thread(threadB, 75, 3, &th2);
    create_thread(idle_thread, 1, 0, &tidle);

    serial_puts("Starting scheduler...");
    schedule();
}
```

## Implementation Key Points

**Ring buffer design:**

-   2-pointer management of `head` (write) and `tail` (read)
-   Safe asynchronous transfer between interrupt handler and application

**IRQ1 interrupt handler processing order:**

1. Send EOI to PIC (most important: enable next interrupt reception)
2. Check PS/2 status (data ready check)
3. Read scan code (port 0x60)
4. ASCII character conversion (using scan code table)
5. Store in ring buffer
6. Debug output (VGA + serial)
7. Wake-up blocked threads

## Troubleshooting

-   **No input at all**

    -   Verify PIC IRQ1 mask (bit 1) removal
    -   Verify IDT entry 33 (IRQ1) configuration

-   **System hang**
    -   Verify EOI send timing (execute at beginning of handler)
    -   Prevent scheduler re-entry during interrupts

## Understanding Check

1. Why was `threadKpoll` removed?
2. What's the difference between interrupt-driven and polling methods?
3. Why is ring buffer necessary?
4. What's the role of `unblock_keyboard_threads`?

## Next Steps

In Day 12, we'll integrate all the features implemented so far and improve them into a modularized structure. We'll also implement more advanced keyboard features (string input, backspace processing).
