# Day 04: Serial Port Debug (freestanding C + Inline Assembly) 🔧

---

🌐 Available languages:

[English](./README.md) | [日本語](./README_ja.md)

## Today's Goal

Build a debug environment using UART serial port and implement a dual output system for VGA and serial.

## Background

While we achieved C language transition in Day 3, debug capabilities limited to VGA screen have limitations. Today we'll build debug infrastructure using serial ports, creating a more efficient OS development environment with log saving and continuous output capabilities.

## New Concepts

-   **I/O Ports (inb, outb)**: Mechanism for communicating with hardware using address space (I/O space) separate from memory space. inb instruction reads data from port, outb writes. Basic method for hardware control.
-   **Inline Assembly**: GCC feature for writing x86 assembly directly within C code. Essential for hardware access.

## Learning Content

-   **Inline Assembly Basics**: Writing x86 assembly within C code
-   **I/O Port Access**: Wrapping `in`/`out` instructions with C functions
-   **UART Programming**: Controlling 8250/16550 series COM1 (0x3F8)
-   **Hardware Initialization**: Baud rate, framing, FIFO settings
-   **Polling Method**: Waiting for transmission ready via LSR register
-   **Debug Infrastructure**: Parallel output system for VGA + serial

【Note】Wait for transmission register empty (LSR 0x20)

-   If bit 0x20 in UART's Line Status Register is 1, transmission register is empty and can write 1 character.
-   The wait `while(!(inb(COM1+5) & 0x20)) {}` is checking this confirmation.

## Task List

-   [ ] Create io.h header file and implement inb/outb functions with inline assembly
-   [ ] Understand UART registers and initialize COM1 port
-   [ ] Implement serial output functions in kernel.c and transmit data using polling method
-   [ ] Build parallel output system for VGA and serial
-   [ ] Use QEMU -serial option to verify serial output
-   [ ] Complete dual output environment as debug infrastructure

## Prerequisites Check

### Required Knowledge

-   **Day 01-02**: Boot, GDT, protected mode switching
-   **Day 03**: freestanding C, VGA control, C + assembly cooperation
-   **C Language**: Basic syntax, functions, header files
-   **Hardware Concepts**: I/O ports, registers, polling

### What's New

-   **Inline Assembly**: Writing x86 instructions within C code
-   **Constraint Description**: Writing GCC assembly constraints (`"=a"`, `"Nd"`, etc.)
-   **I/O Ports**: Addressing method different from memory mapping
-   **UART Protocol**: Serial communication basics (baud rate, framing)

## Approach and Configuration

```
├── boot
│   ├── boot.s           # 16-bit: A20, GDT (defined in same file), load kernel with INT 13h, PE switch
│   └── kernel_entry.s   # 32-bit: segment/stack setup → kmain()
├── boot.s               # (auxiliary/comparison standalone version if available)
├── io.h                 # inb/outb/io_wait (C inline asm)
├── kernel.c             # Initialize and output VGA and COM1 in C
├── Makefile             # Link boot and kernel to generate os.img
├── README.md            # This file
└── vga.h                # VGA API (C)
```

See `day04_completed/` for the completed version. In `day04_completed`, the kernel builds `kernel.c`.

## Implementation Guide

### 1. Understanding Inline Assembly

**Why inline assembly is necessary:**

In OS development, we need to directly control hardware, but there are operations that cannot be done with C language alone:

-   Reading/writing I/O ports (`in`/`out` instructions)
-   Direct CPU register manipulation
-   Special memory access

Using inline assembly allows us to achieve these low-level operations within C code.

**Inline assembly** is a GCC feature for writing x86 assembly directly within C code. It's essential for hardware access in freestanding C environments.

#### GCC Inline Assembly Syntax

```c
__asm__("instruction" : output constraints : input constraints : clobbered registers);
```

#### Constraint Description Meanings

-   `"=a"`: Store output in EAX register
-   `"a"`: Get input from EAX register
-   `"Nd"`: Use input as EDX register or immediate value
-   `%%al`: Escape register name (% → %%)

### 2. io.h (I/O Port Access Functions)

```c
// io.h — I/O port helpers for freestanding C
#pragma once
#include <stdint.h>

// Read 1 byte from I/O port
static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

// Write 1 byte to I/O port
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

// I/O delay (ensure compatibility with old PCs)
static inline void io_wait(void) {
    __asm__ volatile ("outb %%al, $0x80" : : "a"(0));
}
```

**Important points**:

-   `volatile`: Prevents compiler optimization (I/O has side effects)
-   `static inline`: Avoids duplicate definition of header file functions
-   Constraint `"Nd"`: Specify port number in EDX or as immediate value

### 2. kernel.c (COM1 Initialization and Transmission)

```c
#include <stdint.h>
#include "io.h"
#include "vga.h"

#define COM1 0x3F8

static void serial_init(void){
    outb(COM1+1, 0x00);      // IER=0 (disable interrupts)
    outb(COM1+3, 0x80);      // LCR: DLAB=1
    outb(COM1+0, 0x03);      // DLL=3 (115200/38400)
    outb(COM1+1, 0x00);      // DLM=0
    outb(COM1+3, 0x03);      // LCR: 8N1, DLAB=0
    outb(COM1+2, 0xC7);      // FCR: Enable FIFO/clear/14B threshold
    outb(COM1+4, 0x0B);      // MCR: RTS/DTR/OUT2
}

static void serial_putc(char c){
    while(!(inb(COM1+5) & 0x20)){} // LSR bit5 (THR empty)
    outb(COM1+0, (uint8_t)c);
}

static void serial_puts(const char* s){
    while(*s){
        if(*s=='\n') serial_putc('\r');
        serial_putc(*s++);
    }
}

void kmain(void){
    vga_init();
    vga_puts("Day 04: Serial debug (C)\n");
    serial_init();
    serial_puts("COM1: Hello from C!\\r\\n");
}
```

**Implementation points**:

-   **Polling method**: Monitor LSR's THR Empty bit
-   **CRLF conversion**: Convert Unix `\n` to Windows-compatible `\r\n`
-   **Type safety**: Cast `char` to `uint8_t` for port output

### 3. UART Registers and Serial Communication Theory

#### COM1 Port Mapping

| Offset | Register    | Description                                    |
| ------ | ----------- | ---------------------------------------------- |
| COM1+0 | THR/RBR/DLL | Transmit/receive buffer, divisor low when DLAB |
| COM1+1 | IER/DLM     | Interrupt enable, divisor high when DLAB       |
| COM1+2 | FCR/IIR     | FIFO control/interrupt identification          |
| COM1+3 | LCR         | Line control (data length, parity, DLAB)       |
| COM1+4 | MCR         | Modem control (RTS, DTR, OUT)                  |
| COM1+5 | LSR         | Line status (transmit/receive ready, errors)   |

#### Initialization Procedure Theory

1. **Disable interrupts**: OS hasn't prepared interrupt handlers yet
2. **Set DLAB**: Switch to divisor register access mode
3. **Set baud rate**: 38400bps = 115200 ÷ 3
4. **Set framing**: 8-bit data, no parity, 1 stop bit
5. **Enable FIFO**: Improve efficiency with buffering
6. **Modem control**: Basic handshake signal settings

#### I/O Port vs Memory Map

| Method         | Access Method            | Example        | Characteristics               |
| -------------- | ------------------------ | -------------- | ----------------------------- |
| **I/O Port**   | `in`/`out` instructions  | UART, PIC, PIT | Dedicated address space, fast |
| **Memory Map** | Normal memory read/write | VGA (0xB8000)  | Uses part of memory space     |

### 5. boot/boot.s (Architecture Continuation)

Continue the architecture established in Day 03:

-   **GDT Integration**: Define directly in boot.s
-   **Kernel Loading**: Load from sector 2 to `0x00100000` with `INT 13h`
-   **Protected Mode**: `CR0.PE=1` → `jmp 0x08:kernel_entry` (without `dword`)

### 6. boot/kernel_entry.s (Bridge to C)

-   **Segment Setup**: Set DS/ES/FS/GS/SS to `0x10`
-   **Stack Initialization**: Set `ESP` to appropriate position
-   **C Function Call**: Execute `extern kmain` with `call`

### 7. Makefile (C Compilation Support)

Support C compilation same as Day 03:

```makefile
OBJECTS = boot/boot.o boot/kernel_entry.o kernel.o
TARGET = os.img

$(TARGET): $(OBJECTS)
	ld -T linker.ld -o kernel.elf $(OBJECTS)
	objcopy -O binary kernel.elf kernel.bin
	cat boot.bin kernel.bin > $(TARGET)

kernel.o: kernel.c io.h vga.h
	i686-elf-gcc -ffreestanding -c kernel.c -o kernel.o
```

## Build and Execute

```bash
cd day04
make clean
make all
make run        # -serial stdio outputs serial to terminal
```

Expected behavior:

-   Screen (VGA): "Day 04: Serial debug (C)"
-   Terminal (serial): "COM1: Hello from C!"

## Code Explanation: UART Registers

-   `COM1+0 (THR/DLL)` Transmit hold/divisor low when DLAB
-   `COM1+1 (IER/DLM)` Interrupt enable/divisor high when DLAB
-   `COM1+2 (FCR)` FIFO control (enable/clear/threshold)
-   `COM1+3 (LCR)` Data length/stop/parity/DLAB
-   `COM1+4 (MCR)` RTS/DTR/OUT/loopback
-   `COM1+5 (LSR)` Transmit/receive status (bit5: THR empty)

## Troubleshooting

1. Serial output not displayed
    - Starting with `-serial stdio`?
    - Waiting for LSR bit5 before transmission?
    - Is QEMU enabling COM1? (enabled by default)
2. Garbled characters
    - Is divisor ratio (DLL/DLM) correct? (38400→3)
    - Is 8N1 setting (LCR=0x03)?
3. Black screen
    - GDT selector (CS=0x08, DS=0x10) and far jump order

## Understanding Check

### Inline Assembly Understanding

1. What does each part of `__asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port))` mean?
2. Why is `volatile` necessary for I/O operations?
3. Can you explain the difference between GCC constraints `"=a"` and `"Nd"`?

### UART Programming Understanding

1. Why is it necessary to wait for LSR bit5 (THR empty)?
    - **Answer**: If next data is sent before transmission buffer is empty, data will be overwritten and lost
2. What's the meaning of timing for setting/clearing DLAB bit?
    - **Answer**: DLAB=1 switches to divisor setting mode, DLAB=0 switches to normal transmit/receive mode
3. Can you explain the difference between VGA and serial (memory map vs I/O port)?
    - **Answer**: VGA uses memory access to address 0xB8000, serial uses `in`/`out` instructions to port 0x3F8

## Next Steps

-   ✅ UART initialization and transmission
-   ✅ I/O port access basics
-   ✅ Dual debug (VGA+serial)

Next we'll learn to handle asynchronous events using interrupts (IDT) and timers (PIT).
