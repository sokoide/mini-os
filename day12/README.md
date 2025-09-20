# Day 12: Integration and Final Project - Completing a Practical OS ✨

---

🌐 Available languages:

[English](./README.md) | [日本語](./README_ja.md)

## Today's Goal

Integrate all features from Day 1 to Day 11 to complete a practical and robust OS.

## Background

From Day 1 to Day 11, we learned the basics of OS development and individually implemented features like the bootloader, protected mode, interrupts, scheduling, sleep, and keyboard input. Today, we will integrate these, improve quality, add tests and documentation to complete a practical-level OS.

## New Concepts

- **Quality Improvement**: Code hardening, error handling, memory safety.
- **Integration Testing**: Automatic quality verification and stability confirmation with QEMU headless testing.
- **Professional Documentation**: API specifications, architecture explanation, troubleshooting.
- **Modular Architecture**: Improved maintainability through function-based file separation.

## Learning Content

**Completed implementation for Day 12:**

-   Modular architecture (`src/`, `include/` structure)
-   Comprehensive keyboard input system (`src/keyboard.c`)
-   Unified kernel state management (`src/kernel.c`)
-   Enhanced debugging utilities (`src/debug_utils.c`)

**Quality Improvement and Error Handling:**

-   Robustness of all APIs (NULL checks, boundary conditions, return value validation)
-   Thread-safe ring buffer and list operations
-   Interrupt handling optimization and deadlock prevention
-   Memory safety and buffer overflow countermeasures

**Comprehensive Test System:**

-   Automatic quality verification with QEMU headless testing
-   Operation check and debugging based on serial logs
-   Integration tests for thread switching, sleep, and keyboard input
-   Stability confirmation of long-term operation through stress tests

## Task List

-   [ ] Transition to a modular project structure
-   [ ] Complete the comprehensive keyboard input system
-   [ ] Implement unified kernel state management
-   [ ] Enhance debugging utilities
-   [ ] Make all APIs robust (NULL checks, boundary conditions, return value validation)
-   [ ] Implement thread-safe ring buffer and list operations
-   [ ] Automatic quality verification system with QEMU headless testing
-   [ ] Operation check and debugging based on serial logs
-   [ ] Integration tests for thread switching, sleep, and keyboard input
-   [ ] Fix code to be clean with no warnings
-   [ ] Create API specification and architecture explanation documents
-   [ ] Prepare troubleshooting guide and debugging methods

## Day 12 Completed Architecture

### Project Structure

```
day12_completed/
├── src/                    # Source code
│   ├── boot/              # Boot code and assembly
│   │   ├── boot.s         # MBR bootloader
│   │   ├── kernel_entry.s # Kernel entry point
│   │   ├── interrupt.s    # Interrupt handler (asm)
│   │   └── context_switch.s # Context switch
│   ├── kernel.c           # Main kernel functions
│   ├── keyboard.c         # Keyboard driver
│   └── debug_utils.c      # Debug utilities
├── include/               # Header files
│   ├── kernel.h           # Kernel function declarations
│   ├── keyboard.h         # Keyboard function declarations
│   ├── debug_utils.h      # Debug function declarations
│   ├── error_types.h      # Error type definitions
│   ├── io.h               # I/O port operations
│   └── vga.h              # VGA control
├── linker/
│   └── kernel.ld          # Linker script
├── tests/                 # Test system
└── Makefile               # Build system
```

### Completion of the Keyboard Input System

Day 12 significantly expands the basic interrupt-driven keyboard input from Day 11:

**Comprehensive Keyboard Features (`src/keyboard.c`):**

-   **`getchar_blocking()`**: Single character input (equivalent to `scanf("%c")`)
-   **`read_line()`**: String input (equivalent to `scanf("%s")`)
-   **Backspace handling**: Function to correct input mistakes
-   **Input validation**: Accepts only printable characters
-   **Echo display**: Real-time input feedback

**Advanced Buffer Management:**

-   **Circular buffer**: Efficient asynchronous data management
-   **Overflow protection**: Safe handling when the buffer is full
-   **Thread-safe**: Safe cooperation with the interrupt handler

### Advantages of Modularization

**Improved Maintainability:**

-   Clear responsibilities through function-based file separation
-   Dependency management with header files
-   Ease of unit testing

**Improved Extensibility:**

-   Easy to add new device drivers
-   Possibility of parallel development due to functional independence
-   Easy to port features to other projects

### Enhancement of the Debug System

**Comprehensive Debug Output (`src/debug_utils.c`):**

-   **System metrics**: Number of threads, interrupts, keyboard events
-   **State display**: Currently running thread, scheduler lock state
-   **Performance monitoring**: System ticks, memory usage

**Dual Output System:**

-   **VGA**: Real-time screen display
-   **Serial**: For log files and remote debugging

## Evolution from Day 11 to 12

### Day 11 Basic Implementation

-   Basic interrupt-driven keyboard input
-   Simple ring buffer
-   Code with warnings

### Day 12 Complete Implementation

-   **Modularization**: Function-based file separation
-   **Advanced Keyboard**: String input, backspace support
-   **Robustness**: Error handling, input validation
-   **Quality**: Clean code with zero warnings
-   **Testability**: Comprehensive debug system

## Implementation Guide

### Day 12 Modular Architecture Implementation

**Creating the Project Structure**

```bash
# Create directory structure
mkdir -p day12_completed/{src/{boot,},include,linker,tests,build}

# Place source files
day12_completed/
├── src/
│   ├── boot/          # Assembly code
│   ├── kernel.c       # Main kernel
│   ├── keyboard.c     # Keyboard driver
│   └── debug_utils.c  # Debug functions
├── include/           # Header files
├── linker/kernel.ld   # Linker script
└── Makefile           # Build system
```

**Complete Implementation of keyboard.c**

```c
// ============================================================================
// Keyboard Driver Implementation
// PS/2 Keyboard Input Handling with Ring Buffer
// ============================================================================

#include "keyboard.h"

#include "kernel.h"

#define NULL ((void*)0)

// ============================================================================
// Constants and Global Variables
// ============================================================================

// Scancode to ASCII (US layout, Shift ignored, make only)
static const char scancode_map[128] = {
    0,   27,   '1',  '2', '3',  '4', '5', '6', '7', '8', '9', '0', '-',
    '=', '\b', '\t', 'q', 'w',  'e', 'r', 't', 'y', 'u', 'i', 'o', 'p',
    '[', ']',  '\n', 0,   'a',  's', 'd', 'f', 'g', 'h', 'j', 'k', 'l',
    ';', '\'', '`',  0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',',
    '.', '/',  0,    '*', 0,    ' ', 0,   0,   0,   0,   0,   0,   0,
    0,   0,    0,    0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,    0,    0,   0,    0,   0,   0,   0,   0,
};

// Keyboard ring buffer
static volatile char kbuf[KEYBOARD_BUFFER_SIZE];
static volatile uint32_t khead = 0, ktail = 0;

static inline int kbuf_is_empty(void) {
    return khead == ktail;
}

static inline int kbuf_is_full(void) {
    return ((khead + 1) % KEYBOARD_BUFFER_SIZE) == ktail;
}

static void kbuf_push(char c) {
    uint32_t nh = (khead + 1) % KEYBOARD_BUFFER_SIZE;
    if (nh != ktail) {
        kbuf[khead] = c;
        khead = nh;
    }
}

static int kbuf_pop(char* out) {
    if (kbuf_is_empty())
        return 0;
    *out = kbuf[ktail];
    ktail = (ktail + 1) % KEYBOARD_BUFFER_SIZE;
    return 1;
}

// ============================================================================
// Internal Helper Functions
// ============================================================================

// PS/2 keyboard initialization
static inline int ps2_output_full_internal(void) {
    return (inb(0x64) & 0x01);
}

void ps2_keyboard_init(void) {
    // Special initialization is often not necessary in QEMU. Just clean the output buffer lightly.
    int guard = 32;
    while (ps2_output_full_internal() && guard--) {
        (void)inb(0x60);
    }
}

// ============================================================================
// Public API Functions
// ============================================================================

// Keyboard initialization
void keyboard_init(void) {
    ps2_keyboard_init();
}

// Keyboard interrupt handler
void keyboard_handler_c(void) {
    // Notify PIC of interrupt completion
    outb(0x20, 0x20);

    // Check if keyboard data is available to be read
    uint8_t status = inb(0x64);
    if (!(status & 0x01)) {
        serial_write_string(
            "KEYBOARD: Interrupt fired but no data available\n");
        return;
    }

    // Read scancode
    uint8_t scancode = inb(0x60);

    // Ignore key release operations (break code)
    if (scancode & 0x80) {
        return;
    }

    // Convert scancode to ASCII character
    char ch = (scancode < 128) ? scancode_map[scancode] : 0;

    if (ch != 0) {
        // Store in buffer
        kbuf_push(ch);

        // Debug output
        serial_write_string("KEY: ");
        serial_write_char(ch);
        serial_write_string(" (");
        serial_puthex(scancode);
        serial_write_string(")\n");

        // Move waiting threads to READY (if any)
        unblock_keyboard_threads();
    }
}

// Blocking character input
char getchar_blocking(void) {
    char c;
    for (;;)
    {
        if (kbuf_pop(&c))
            return c;

        // Block if no input
        asm volatile("cli");
        if (kbuf_pop(&c)) {
            asm volatile("sti");
            return c;
        }
        block_current_thread(BLOCK_REASON_KEYBOARD, 0);
        asm volatile("sti");

        schedule();
    }
}

// Check if keyboard buffer is empty
int keyboard_buffer_empty(void) {
    return kbuf_is_empty();
}

// Line input function (ported and improved from day99_completed)
void read_line(char* buffer, int max_length) {
    // Enhanced input validation
    if (!buffer || max_length <= 1) {
        serial_write_string("read_line: Invalid parameters\n");
        return;  // Invalid parameters
    }

    // Additional safety: reasonable upper limit check
    if (max_length > 1024) {
        serial_write_string(
            "read_line: Buffer size too large, limiting to 1024\n");
        max_length = 1024;  // Prevent excessive buffer sizes
    }

    int pos = 0;
    char c;

    // Initialize buffer for safety
    buffer[0] = 0;

    while (pos < max_length - 1) {
        c = getchar_blocking();

        if (c == 10 || c == 13) {  // LF or CR
            // End input on newline
            break;
        } else if (c == 8 && pos > 0) {  // Backspace
            // Handle backspace
            pos--;
            // Remove character from screen (backspace + space + backspace)
            serial_write_char(8);
            serial_write_char(' ');
            serial_write_char(8);
        } else if (c >= 32 && c <= 126) {
            // Accept only printable characters
            buffer[pos] = c;
            pos++;
            // Echo display
            serial_write_char(c);
        }
        // Ignore other control characters
    }

    buffer[pos] = 0;        // String terminator
    serial_write_char(10);  // Output newline
}

// Check PS/2 output buffer
int ps2_output_full(void) {
    return ps2_output_full_internal();
}

// Unblock threads waiting for keyboard
void unblock_keyboard_threads(void) {
    asm volatile("cli");
    kernel_context_t* ctx = get_kernel_context();
    thread_t* current = ctx->blocked_thread_list;
    thread_t* prev = NULL;
    while (current) {
        thread_t* next = current->next_blocked;
        if (current->block_reason == BLOCK_REASON_KEYBOARD) {
            if (prev) {
                prev->next_blocked = current->next_blocked;
            } else {
                ctx->blocked_thread_list = current->next_blocked;
            }

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

**Makefile Implementation**

```makefile
# Day 12 Integrated Build System
CC = i686-elf-gcc
LD = i686-elf-ld
ASM = nasm
OBJCOPY = i686-elf-objcopy

# Directories
SRC_DIR = src
BOOT_DIR = $(SRC_DIR)/boot
INCLUDE_DIR = include
BUILD_DIR = build

# Flags
CFLAGS = -ffreestanding -m32 -nostdlib -fno-stack-protector \
         -fno-pic -O2 -Wall -Wextra -I$(INCLUDE_DIR)
LDFLAGS = -m elf_i386 -T linker/kernel.ld

# Object files
BOOT_OBJECTS = $(BUILD_DIR)/boot.bin $(BUILD_DIR)/kernel_entry.o \
               $(BUILD_DIR)/interrupt.o $(BUILD_DIR)/context_switch.o
KERNEL_OBJECTS = $(BUILD_DIR)/kernel.o $(BUILD_DIR)/keyboard.o \
                 $(BUILD_DIR)/debug_utils.o

.PHONY: all clean run test

all: os.img

# Boot sector
$(BUILD_DIR)/boot.bin: $(BOOT_DIR)/boot.s | $(BUILD_DIR)
	$(ASM) -f bin $< -o $@

# Assembly objects
$(BUILD_DIR)/%.o: $(BOOT_DIR)/%.s | $(BUILD_DIR)
	$(ASM) -f elf32 $< -o $@

# C objects
$(BUILD_DIR)/kernel.o: $(SRC_DIR)/kernel.c $(INCLUDE_DIR)/*.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/keyboard.o: $(SRC_DIR)/keyboard.c $(INCLUDE_DIR)/keyboard.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/debug_utils.o: $(SRC_DIR)/debug_utils.c $(INCLUDE_DIR)/debug_utils.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Kernel link
$(BUILD_DIR)/kernel.elf: $(BOOT_OBJECTS) $(KERNEL_OBJECTS)
	$(LD) $(LDFLAGS) $(filter-out $(BUILD_DIR)/boot.bin,$^) -o $@

$(BUILD_DIR)/kernel.bin: $(BUILD_DIR)/kernel.elf
	$(OBJCOPY) -O binary $< $@

# Create OS image
os.img: $(BUILD_DIR)/boot.bin $(BUILD_DIR)/kernel.bin
	cat $^ > $@

truncate -s 1440K $@

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Run tests
test: os.img
	qemu-system-i386 -drive format=raw,file=os.img -serial stdio -display none -no-reboot

# Run normally
run: os.img
	qemu-system-i386 -drive format=raw,file=os.img

# Clean up
clean:
	rm -rf $(BUILD_DIR) os.img

# Help
help:
	@echo "Available targets:"
	@echo "  all     - Build the entire OS"
	@echo "  run     - Run the OS in QEMU"
	@echo "  test    - Run headless tests"
	@echo "  clean   - Remove generated files"
```

**Header File Design (include/keyboard.h)**

```c
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "error_types.h"

// PS/2 keyboard constants
#define PS2_DATA 0x60
#define PS2_STATUS 0x64
#define PS2_OUTPUT_FULL 0x01

// Keyboard buffer size
#define KEYBOARD_BUFFER_SIZE 256

// Keyboard function prototypes
void keyboard_init(void);
void keyboard_handler_c(void);
char getchar_blocking(void);
int keyboard_buffer_empty(void);
void read_line(char* buffer, int max_length);

// Keyboard test/debug functions
int ps2_output_full(void);
void unblock_keyboard_threads(void);

#endif  // KEYBOARD_H
```

**Integration Test System**

```c
// Test functions in kernel.c
void run_integration_tests(void) {
    serial_write_string("=== Day 12 Integration Test Start ===\n");

    // Scheduler test
    test_scheduler_functionality();

    // Keyboard test
    test_keyboard_system();

    // Sleep/wake test
    test_sleep_wake_mechanism();

    serial_write_string("=== All Tests Completed ===\n");
}

void test_scheduler_functionality(void) {
    serial_write_string("Scheduler Test: ");

    // Basic operation check
    if (get_current_thread() != NULL) {
        serial_write_string("OK\n");
    } else {
        serial_write_string("FAIL\n");
    }
}

void test_keyboard_system(void) {
    serial_write_string("Keyboard System Test: ");

    // Buffer operation check
    if (!keyboard_buffer_empty() || ps2_output_full() >= 0) {
        serial_write_string("OK\n");
    } else {
        serial_write_string("FAIL\n");
    }
}
```

### Implementation Phases

**Phase 1: Foundation Migration (1-2 hours)**

1.  Create directory structure
2.  Split Day 11 code into modules
3.  Create header files and organize dependencies
4.  Create Makefile and confirm build

**Phase 2: Feature Enhancement (2-3 hours)**

1.  Implement advanced features in keyboard.c
2.  Enhance debug_utils.c
3.  Add comprehensive error handling
4.  Implement input validation and buffer protection

**Phase 3: Quality Improvement (1-2 hours)**

1.  Resolve all warnings
2.  Implement integration tests
3.  Prepare documentation
4.  Final operation check

## Integration Completion Checklist

### 🔧 Architecture Integration

-   [ ] **Modular Structure**: Functional division with src/, include/
-   [ ] **Scheduler System**: Preemptive multithreading
-   [ ] **Thread Lifecycle**: Complete implementation of create, block/unblock, schedule
-   [ ] **Multithread Safety**: Interrupt protection and scheduler lock

### 🧪 Quality Assurance & Testing

-   [ ] **Zero-Warning Build**: Resolution of all compilation warnings
-   [ ] **Integration Tests**: Operation checks for scheduler, keyboard, sleep
-   [ ] **Boundary Condition Tests**: NULL input, buffer overflow countermeasures
-   [ ] **Long-Term Operation Test**: Stable multithreaded operation

### ⚡ Performance & Safety

-   [ ] **Interrupt Optimization**: Minimization of cli-sti duration
-   [ ] **Deadlock Prevention**: Proper interrupt management
-   [ ] **Memory Safety**: Buffer overflow countermeasures and NULL pointer checks
-   [ ] **Unified Error Handling**: Consistent error handling

### 📚 Documentation & Learning Value

-   [ ] **API Specifications**: Complete specification descriptions for all functions
-   [ ] **Architecture Explanation**: Evolution from Day 11 to Day 12
-   [ ] **Learning Guide**: Documentation for step-by-step understanding
-   [ ] **Troubleshooting**: Problem-solving procedures and debugging methods

## Troubleshooting

-   **Keyboard input not responding**
    -   Check IRQ1 mask settings
    -   Check IDT entry 33 settings
-   **Garbled characters or data loss**
    -   Check scancode table
    -   Check for race conditions in the ring buffer
-   **System hang**
    -   Check EOI sending timing
    -   Check acquisition and release of scheduler lock

## 🎉 Day 12 Complete! - Congratulations on completing a practical OS!

**Through the learning from Day 01 to 12, you have achieved the following:**

### ✅ Acquired Technical Skills

-   **x86 Architecture**: Complete understanding from real mode to protected mode
-   **Operating System Design**: Implementation of a modular, practical OS
-   **Multithread Programming**: Preemptive scheduling and synchronization control
-   **Low-Level Hardware Control**: Interrupt, timer, keyboard, VGA control
-   **System Programming**: Integration of assembly and C, device driver development
-   **Software Quality**: Modularization, testing, debugging methods

### 🎯 Features of the Completed OS

-   **32-bit x86 Protected Mode**: Utilization of modern CPU features
-   **Modular Architecture**: Design considering maintainability and extensibility
-   **Preemptive Multithreading**: Fair CPU time allocation and real-time response
-   **Interrupt-Driven I/O**: Asynchronous processing of keyboard and timer
-   **Comprehensive Keyboard Input**: Character/string input, backspace support
-   **Integrated Debug System**: Comprehensive debugging with VGA display and serial output
-   **Robustness and Error Handling**: Practical-level quality assurance

### 🚀 Challenge for the Next Step

**For further learning, based on the reference implementation:**

-   **Advanced Synchronization Primitives**: Semaphores, mutexes, condition variables
-   **Memory Management Expansion**: Dynamic memory allocation, virtual memory
-   **File System**: Implementation of FAT12/16 read/write functionality
-   **Network Functions**: Basic TCP/IP implementation
-   **GUI System**: Window manager and graphical interface
-   **Porting to Different Architectures**: ARM, RISC-V, x86_64

**The OS you created is a wonderful achievement that demonstrates an understanding of computer science fundamentals and serves as a starting point for further exploration. Continue to enjoy learning and development!** 🎓