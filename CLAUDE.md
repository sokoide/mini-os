# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an educational x86 32-bit multithreaded operating system with timer-based preemptive scheduling and keyboard input support. It demonstrates core OS concepts including boot process, interrupt handling, context switching, thread management, and hardware I/O. The OS runs in protected mode and uses round-robin scheduling with timer interrupts. The keyboard system provides scanf("%c") and scanf("%s") equivalent functionality via PS/2 controller integration.

## Development Commands

### Essential Build Commands

```bash
# Build the complete OS image
make all

# Run the OS in QEMU emulator
make run

# Clean all generated files
make clean

# Check if required development tools are installed
make check-env

# Show help with available targets
make help
```

### Development Requirements

- **i686-elf-gcc**: Cross-compiler for x86 target
- **nasm**: Assembler for x86 assembly code
- **qemu-system-i386**: x86 emulator for testing

## Architecture Overview

### Boot Process

1. **Boot Sector (boot.s)**: 16-bit real mode → A20 line enable → GDT setup → protected mode transition
2. **Kernel Entry (kernel_entry.s)**: BSS clearing, stack setup, jump to C kernel
3. **Kernel Main (kernel.c)**: Interrupt system, thread creation, scheduler initialization

### Key Components

#### Memory Layout

- **0x7C00**: Boot sector loading point
- **0x100000** (1MB): Kernel execution location
- **0x200000** (2MB): Kernel stack
- **0xB8000**: VGA text mode buffer

#### Project Structure

```
mini-os/
├── src/
│   ├── kernel/             # Core kernel functionality
│   │   ├── kernel.c        # Main kernel with threading and scheduling
│   │   ├── kernel.h        # Kernel function declarations
│   │   └── error_types.h   # Error code definitions
│   ├── drivers/            # Device drivers
│   │   ├── keyboard.c      # PS/2 keyboard driver implementation
│   │   └── keyboard.h      # Keyboard driver interface
│   ├── debug/              # Debug and diagnostic utilities
│   │   ├── debug_utils.c   # Debug functionality implementation
│   │   └── debug_utils.h   # Debug utility interfaces
│   ├── boot/               # Boot sequence and low-level code
│   │   ├── boot.s          # MBR bootloader with GDT and protected mode setup
│   │   ├── kernel_entry.s  # Assembly entry point with BSS initialization
│   │   ├── interrupt.s     # Timer interrupt handler and context switch functions
│   │   └── boot_constants.inc # Assembly constants and definitions
│   └── linker/             # Linker scripts
│       └── kernel.ld       # Memory layout definition
├── tests/                  # Test framework and test cases
├── docs/                   # Documentation
└── Makefile               # Build system configuration
```

### Context Switching Mechanism

The OS implements sophisticated context switching with proper register and flag preservation:

#### Thread Stack Layout (for initial_context_switch)

Stack is initialized in this order (bottom to top):

1. **8 Registers**: EAX, EBX, ECX, EDX, ESI, EDI, EBP (popped first)
2. **EFLAGS** (0x202 - interrupts enabled)
3. **Function Address** (popped last via `pop eax; jmp eax`)

#### Critical Functions

- **switch_context()**: Save current thread state, restore new thread state
- **initial_context_switch()**: First-time thread execution without saving previous state
- **timer_interrupt_handler**: Hardware timer interrupt entry point calling scheduler

### Thread Control Block (TCB)

Each thread_t contains:

- **4KB stack** (stack[1024])
- **State management** (READY/RUNNING/BLOCKED)
- **Timing controls** (counter, delay_ticks, last_tick)
- **Display positioning** (display_row)
- **ESP pointer** for context switching

### Interrupt System

- **Timer**: PIT configured for 100Hz (10ms intervals)
- **Keyboard**: PS/2 controller for real-time input processing
- **PIC**: Master PIC remapped to interrupts 32-39
- **IDT**: Timer (IRQ0→32), Keyboard (IRQ1→33)
- **Handler Flow**: Hardware → Assembly wrapper → C handler → Buffer/Scheduler → Context switch

### Keyboard Input System

The keyboard implementation provides scanf-equivalent functionality:

#### Hardware Interface

- **PS/2 Controller**: Communication via ports 0x60 (data) and 0x64 (status)
- **IRQ1 Interrupt**: Real-time scan code processing
- **Scan Code Tables**: US keyboard layout with Shift support

#### Software Architecture

- **Circular Buffer**: Thread-safe keystroke storage (256 bytes)
- **ASCII Conversion**: Hardware scan codes → printable characters
- **Input Functions**:
  - `getchar()` / `scanf_char()` - Single character input
  - `read_line()` / `scanf_string()` - String input with backspace support

#### Usage Examples

```c
// Single character input (scanf("%c") equivalent)
char ch = getchar();

// String input (scanf("%s") equivalent)
char buffer[64];
read_line(buffer, sizeof(buffer));
```

## Common Development Patterns

### Adding New Thread Functions

```c
void new_thread_function(void) {
    while (1) {
        asm volatile ("hlt");  // Wait for timer interrupt
    }
}

// In kernel_main():
create_thread(new_thread_function, delay_ticks, display_row);
```

### Debug Output

The system provides dual debug output:

- **VGA Text Mode**: Direct screen output at 0xB8000
- **Serial Port**: COM1 for external capture
- **debug_print()**: Outputs to both VGA and serial

### Memory Management Notes

- Uses **flat memory model** with segmentation effectively disabled
- **GDT** has NULL, Code (0x08), and Data (0x10) segments
- All segments map 0x00000000 to 4GB with kernel privilege (DPL=0)
- No paging implemented - uses 1:1 linear to physical mapping

## Critical Implementation Details

### Stack Layout Consistency

The create_thread() function pushes values in reverse order of how initial_context_switch() pops them. Comments in both kernel.c and interrupt.s explain this critical relationship.

### Interrupt Re-entry Protection

Timer interrupt handler automatically disables interrupts on entry. Context switching uses `popfd` to restore EFLAGS including interrupt enable flag - no manual `sti` needed.

### Thread Lifecycle

1. Thread created with create_thread() - stack initialized, state = READY
2. First execution via initial_context_switch() from scheduler
3. Subsequent switches use switch_context() for save/restore
4. Round-robin scheduling every 10ms via timer interrupt

### EFLAGS Handling

Critical for proper interrupt behavior:

- Thread stacks initialized with EFLAGS = 0x202 (IF=1, reserved bit=1)
- Context switching preserves interrupt enable state via pushfd/popfd
- Initial thread execution enables interrupts via popfd in initial_context_switch

## Architecture Documentation

See `ARCHITECTURE_jp.md` for comprehensive Japanese documentation including:

- Detailed system architecture diagrams
- x86 register explanations
- Boot process flow
- Memory layout details
- Educational explanations of OS concepts
