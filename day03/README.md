# Day 03: VGA Text Display (C Transition + Boot Split) 🎨

## Today's Learning Goals

**Development methodology changes significantly from Day 03.** We transition from the simple assembly file structure of Day 01-02 to full-scale OS development centered on C language.

### Why Transition to C Language?

**Limitations of Assembly-Centric Development in Day 01-02**

-   **Rapid complexity increase**: With VGA control, interrupt handling, memory management, etc., maintenance becomes difficult with assembly alone
-   **Lack of type safety**: Bugs in register and memory operations are hard to detect
-   **Reduced readability**: Code intent becomes unclear, unsuitable for team development

**Benefits of C Language Transition**

-   **Type system**: Clear design through structures and pointer types
-   **Function division**: Modularized functionality with testable structure
-   **Maintainability**: Clear expression of intent through high-level language
-   **Practicality**: Real OS like Linux and Windows are developed in C/C++

From Day 03, we implement primarily in C. To approach the final form (day99_completed) structure, boot-related files are split into `boot/boot.s` (16-bit) and `boot/kernel_entry.s` (32-bit entry). GDT is integrated into `boot.s`. Display is implemented in C with `kernel.c` and `vga.h`.

## Learning Content

-   VGA text buffer (0xB8000) structure and C access
-   Hardware cursor control (CRTC registers)
-   Color attributes (foreground/background) and text drawing API
-   Implementation of newline, scrolling, and cursor advance
-   Role separation in boot stages (16-bit loader vs 32-bit kernel entry)

【Note】VGA text 1 character is 2 bytes

-   Lower 8 bits are character code, upper 8 bits are attributes (lower 4 bits = foreground color / upper 4 bits = background color).
-   Example: `uint16_t cell = (uint8_t)c | ((attr & 0xFF) << 8);` written to `0xB8000 + 2*(row*80+col)`.

## Task List

-   [ ] Create vga.h header file and define VGA control API
-   [ ] Create kernel.c file and implement kmain() function
-   [ ] Integrate GDT in boot.s and add kernel loading to memory
-   [ ] Create kernel_entry.s for 32-bit environment initialization
-   [ ] Update Makefile for C code compilation and linking
-   [ ] Display strings by writing directly to VGA buffer in freestanding C environment
-   [ ] Verify operation in QEMU and confirm colorful text display

## Prerequisites Check

### Required Knowledge

-   Day 01-02 (boot, GDT, protected mode)
-   32-bit C (pointers, arrays, header and implementation separation)

### What's New Today

-   **freestanding C environment** (no libc, -ffreestanding/-nostdlib)
-   Direct C access to low-level I/O
-   Role separation between 16-bit loader and 32-bit kernel

## Understanding freestanding C Environment

**Normal C Environment vs OS Development Environment Differences:**

Normal C programs (e.g., Hello World) run on top of an OS:

-   OS provides memory management, file I/O, standard library
-   `main()` is called automatically
-   `printf()` and `malloc()` are available

In OS development, you become the OS itself:

-   Standard library doesn't exist (OS itself provides it)
-   Need to directly control memory and I/O
-   Must manually call `kmain()`

**Why freestanding C is necessary:**

-   To write code that runs in an environment without OS
-   To directly operate hardware (VGA, serial)
-   To design for operation with minimal resources

From Day 03 onwards, we develop in a "freestanding C" environment that differs greatly from normal C development environments.

### Normal C Environment vs freestanding C Environment

| Item                  | Normal C Environment                         | freestanding C Environment (OS Development)                          |
| --------------------- | -------------------------------------------- | -------------------------------------------------------------------- |
| **Standard Library**  | `printf`, `malloc`, `strlen`, etc. available | **Not available** - must implement yourself                          |
| **Memory Management** | Heap management with `malloc`/`free`         | **Custom implementation** - direct physical memory management        |
| **String Operations** | `strlen`, `strcpy`, etc. available           | **Custom implementation** - manual character-by-character processing |
| **I/O Operations**    | Console output with `printf`                 | **Direct hardware control** - VGA/serial                             |
| **Startup Process**   | Automatic start from `main()`                | **Bootloader manual call**                                           |
| **Linker**            | OS decides memory placement                  | **Explicitly specify memory placement**                              |

### Elements Prohibited/Unavailable in freestanding C

```c
// ❌ The following cannot be used at all

// Standard I/O functions
printf("Hello");           // → Replace with vga_puts("Hello")
scanf("%d", &num);         // → Replace with getchar()

// Memory management functions
int* ptr = malloc(100);    // → Use arrays or direct physical memory management
free(ptr);

// String functions
strlen(str);               // → Manually loop and count
strcpy(dest, src);         // → Manually copy character by character

// Math functions
sin(3.14);                 // → Custom implementation or avoid use
```

### Functions Requiring Custom Implementation in freestanding C

```c
// ✅ You implement functions like these yourself

// VGA output (printf replacement)
void vga_puts(const char* s);
void vga_putc(char c);

// String operations (strlen replacement)
int my_strlen(const char* s) {
    int len = 0;
    while (s[len] != '\0') len++;
    return len;
}

// I/O port operations (OS-specific)
void outb(uint16_t port, uint8_t value);
uint8_t inb(uint16_t port);
```

### Special Flags for Compilation

```bash
# Compiler flags for freestanding C
-ffreestanding   # Don't assume standard library
-nostdlib       # Don't link standard library
-fno-stack-protector  # Disable stack protection features
-m32            # Generate 32-bit code
```

### Why These Constraints Exist

**Why do these constraints exist?**

1. **OS doesn't exist yet**: `printf` and `malloc` depend on OS functionality
2. **Memory management not implemented**: No heap management system yet
3. **Direct hardware control**: Need to directly operate hardware like VGA, serial
4. **Minimal execution environment**: Minimal environment called from bootloader

## Approach and Overall Picture

```
boot/boot.s (16-bit):
  - A20 enable, GDT definition and registration, load kernel with BIOS INT 0x13
  - CR0.PE=1 → far jump (32-bit offset) to kernel_entry

boot/kernel_entry.s (32-bit):
  - Segment setup, stack initialization, call extern kmain

kernel.c: kmain() uses vga.h API for display
vga.h: C API for VGA control (clear/cursor/color/output/scroll)
```

## Hands-on: VGA Layer Implementation in C

### Step 1: Project Structure

**Actual day03 directory structure** (learning version with TODOs):

```
day03/
├── README.md            # This file (learning guide)
├── boot/
│   ├── boot.s           # 16-bit: A20, GDT integration, load kernel with INT 0x13, to PE
│   └── kernel_entry.s   # 32-bit: segment/stack setup → kmain()
├── kernel.c             # kmain and VGA demo (C implementation)
├── vga.h                # VGA text display API definition
└── Makefile             # Link boot and kernel to create os.img
```

**day03_completed structure** (completed version for reference):

```
day03_completed/
├── README.md            # Technical explanation of completed version
├── boot/
│   ├── boot.s           # Same as above (working implementation)
│   └── kernel_entry.s   # Same as above (working implementation)
├── kernel.c             # Complete implementation version
├── vga.h                # Complete API definition
├── io.h                 # I/O port operation helpers
└── Makefile             # Completed version build script
```

**Learning approach**:

1. Try implementing yourself in `day03/`
2. Refer to `day03_completed/` when stuck
3. Compare both to deepen understanding

### Step 2: vga.h (API Design)

First define the API in the header (implementation in kernel.c, or split to vga.c in future).

```c
// vga.h — VGA text mode control (C, freestanding)
#pragma once
#include <stdint.h>

#define VGA_WIDTH  80
#define VGA_HEIGHT 25

typedef enum {
    VGA_BLACK=0, VGA_BLUE, VGA_GREEN, VGA_CYAN, VGA_RED, VGA_MAGENTA, VGA_BROWN, VGA_LIGHT_GRAY,
    VGA_DARK_GRAY, VGA_LIGHT_BLUE, VGA_LIGHT_GREEN, VGA_LIGHT_CYAN, VGA_LIGHT_RED, VGA_PINK, VGA_YELLOW, VGA_WHITE
} vga_color_t;

void vga_init(void);
void vga_clear(void);
void vga_set_color(vga_color_t fg, vga_color_t bg);
void vga_move_cursor(uint16_t x, uint16_t y);
void vga_putc(char c);
void vga_puts(const char* s);
```

### Step 3: kernel.c (Implementation and Entry)

Implement and use vga.h API with `kmain()` as entry (no libc).

```c
// kernel.c — kmain and VGA implementation (minimal)
#include <stdint.h>
#include "vga.h"

static volatile uint16_t* const VGA_MEM = (uint16_t*)0xB8000;
static uint16_t cursor_x = 0, cursor_y = 0;
static uint8_t color = 0x0F; // white on black

static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

void vga_set_color(vga_color_t fg, vga_color_t bg) {
    color = (uint8_t)fg | ((uint8_t)bg << 4);
}

void vga_move_cursor(uint16_t x, uint16_t y) {
    cursor_x = x; cursor_y = y;
    uint16_t pos = y * VGA_WIDTH + x;
    outb(0x3D4, 14);
    outb(0x3D5, (pos >> 8) & 0xFF);
    outb(0x3D4, 15);
    outb(0x3D5, pos & 0xFF);
}

void vga_clear(void) {
    for (uint16_t y=0; y<VGA_HEIGHT; ++y) {
        for (uint16_t x=0; x<VGA_WIDTH; ++x) {
            VGA_MEM[y*VGA_WIDTH + x] = vga_entry(' ', color);
        }
    }
    vga_move_cursor(0,0);
}

static void vga_newline(void) {
    cursor_x = 0;
    if (++cursor_y >= VGA_HEIGHT) {
        // simple scroll up
        for (uint16_t y=1; y<VGA_HEIGHT; ++y)
            for (uint16_t x=0; x<VGA_WIDTH; ++x)
                VGA_MEM[(y-1)*VGA_WIDTH + x] = VGA_MEM[y*VGA_WIDTH + x];
        for (uint16_t x=0; x<VGA_WIDTH; ++x)
            VGA_MEM[(VGA_HEIGHT-1)*VGA_WIDTH + x] = vga_entry(' ', color);
        cursor_y = VGA_HEIGHT-1;
    }
}

void vga_putc(char c) {
    if (c=='\n') { vga_newline(); vga_move_cursor(cursor_x, cursor_y); return; }
    VGA_MEM[cursor_y*VGA_WIDTH + cursor_x] = vga_entry(c, color);
    if (++cursor_x >= VGA_WIDTH) vga_newline();
    vga_move_cursor(cursor_x, cursor_y);
}

void vga_puts(const char* s) { while (*s) vga_putc(*s++); }

void vga_init(void) { vga_set_color(VGA_WHITE, VGA_BLACK); vga_clear(); }

// I/O port helpers (planned for future as io.h / provided by boot side here)
extern void outb(uint16_t port, uint8_t val);

void kmain(void) {
    vga_init();
    vga_puts("Day 03: C-based VGA driver\n");
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_puts("Hello from C!\n");
}
```

Note: `outb` is provided by boot side (or implemented in C as io.h in later sessions).

### Step 4: boot/boot.s (16-bit, GDT integration, kernel load, PE switch)

`boot.s` performs the following:

-   Disable interrupts, enable A20 (0x92)
-   Define GDT "within file" and register with `lgdt`
-   Load sector 2 onwards from disk to `0x00100000` with BIOS `INT 0x13` (simple fixed sector count)
-   KERNEL_SECTORS sectors are loaded into memory. For 127 sectors, 127 x 512 bytes = 63.5 kilobytes are loaded
-   Enter protected mode with `CR0.PE=1` → `jmp dword 0x08:kernel_entry` to 32-bit

Pseudocode (main parts):

```assembly
[org 0x7C00]
[bits 16]
start:
  cli
  ; A20 enable (0x92)
  in al, 0x92
  or al, 0x02
  out 0x92, al

  ; load GDT (defined below in this file)
  lgdt [gdt_descriptor]

  ; read kernel: INT 0x13 (CHS/LBA simple, fixed sector count for learning)
  ; destination ES:BX = 0x1000:0x0000 => 0x00100000
  ; ...(details expanded in future sessions)

  ; enter protected mode
  mov eax, cr0
  or eax, 1
  mov cr0, eax
  jmp dword 0x08:kernel_entry   ; 32-bit far jump

; --- GDT ---
align 8
gdt_start:
  dq 0x0000000000000000
  dq 0x00CF9A000000FFFF  ; code
  dq 0x00CF92000000FFFF  ; data
gdt_end:
gdt_descriptor:
  dw gdt_end - gdt_start - 1
  dd gdt_start

times 510 - ($ - $$) db 0
dw 0xAA55
```

### Step 5: boot/kernel_entry.s (32-bit, kmain call)

`kernel_entry.s` is 32-bit code that calls `extern kmain` after segment setup/stack initialization.

```assembly
[bits 32]
extern kmain
global kernel_entry

kernel_entry:
  mov ax, 0x10
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax
  mov ss, ax
  mov esp, 0x00300000
  call kmain
.halt:
  hlt
  jmp .halt
```

### Step 6: Makefile

-   Please refer to `day03_complete/Makefile`.

## Troubleshooting

### Day 03 Specific Issues

**🔴 kmain() is not called**

-   **Cause**: Mismatch between kernel link address and load destination address
-   **Check method**:
    ```bash
    objdump -h kernel.elf | grep text  # Check link address
    hexdump -C os.img | head -20       # Check loaded content
    ```
-   **Solution**: Match `ld -Ttext 0x00010000` with boot.s load destination

**🔴 C Compilation Error**

-   **Problem**: Using `printf` or `strlen`
-   **Solution**: Replace with custom functions for freestanding C
    ```c
    // ❌ printf("Hello");
    // ✅ vga_puts("Hello");
    ```

**🔴 Link Error**

-   **Problem**: `_start` symbol not found
-   **Solution**: Declare `global kernel_entry` in kernel_entry.s and link with `-e kernel_entry`

**🔴 No VGA Output**

-   **Cause 1**: Failed to write to VGA buffer address (0xB8000)
-   **Cause 2**: Writing 2-byte structure (character+attribute) as 1 byte
-   **Check method**: Execute `x/20x 0xB8000` in QEMU monitor
-   **Solution**:
    ```c
    // ❌ *((char*)0xB8000) = 'A';
    // ✅ *((uint16_t*)0xB8000) = 'A' | (0x0F << 8);
    ```

### Common Errors in freestanding C Environment

**Compilation errors**:

```bash
# ❌ undefined reference to `printf`
# → Use vga_puts()

# ❌ undefined reference to `strlen`
# → Manual implementation or use existing implementation

# ❌ implicit declaration of function 'outb'
# → Include io.h or extern declaration
```

**Runtime errors**:

-   **Hang**: Check if `hlt` instruction is at end of infinite loop
-   **Garbled characters**: Check null-terminated strings
-   **Cursor abnormal**: Check CRTC register write order

## Understanding Check

1. What are the differences between freestanding C and normal C execution environment?
2. How many bytes is 1 VGA character and what's the breakdown?
3. Which I/O ports control hardware cursor position?
4. Which memory areas are moved how during scrolling?

## Next Steps

-   ✅ C-based VGA drawing API
-   ✅ Initialization and output from kmain()
-   ✅ Minimal boot.s (GDT/mode switch/call)

In future sessions, we'll organize kernel loading and placement, provide I/O helpers (outb/inb), improve build/link (introduce linker scripts), and proceed to interrupts and timers.
