# Day 08: Context Switch 🔄

---

🌐 Available languages:

[English](./README.md) | [日本語](./README_ja.md)

## Today's Goal

Implement context_switch function in assembly and use TCB's esp to switch thread execution states.

## Background

While we designed TCB and READY list in Day 7, we can't actually switch threads yet. Today we'll implement the core assembly function for context switching, performing register save/restore and stack switching to complete the foundation for full multithreading.

## New Concepts

-   **Context Switch**: Core concept of multitasking. "Save execution state (registers etc.) of one thread and restore state of another thread".

## Learning Content

-   Register save/restore using pushfd/push and popfd/pop
-   ESP save/restore and stack switching
-   Initial stack design
-   Thread Control Block (TCB) structure

## Task List

-   [ ] Create context_switch.s file and implement assembly function
-   [ ] Correctly implement register save/restore (pushfd/push/pop/popfd) sequence
-   [ ] Perform ESP save/restore and stack switching
-   [ ] Implement function to build initial stack in kernel.c
-   [ ] Update TCB structure to utilize esp field
-   [ ] Create demo threads and perform thread switching with context_switch
-   [ ] Verify operation in QEMU and confirm Thread A execution

## Configuration

```
boot/boot.s, boot/kernel_entry.s
boot/context_switch.s   # Context switch implementation (32-bit)
io.h, vga.h
kernel.c                # TCB/thread management + switch invocation
Makefile
```

## Implementation Guide

### Detailed Context Switch Diagram

**Stack state change process:**

```
🔄 Running thread (ThreadA) stack:
Higher Address
+------------------+
|  ThreadA Data    |
|       ...        |
+------------------+ <- ESP (ThreadA running)
|                  |
Lower Address

📥 context_switch call (save process):
ThreadA → context_switch(&threadA->esp, threadB->esp);

Save order: pushfd → push ebp → push edi → ... → push eax

+------------------+
|  ThreadA Data    |
+------------------+
|      EFLAGS      | <- pushfd
|       EBP        | <- push ebp
|       EDI        | <- push edi
|       ESI        | <- push esi
|       EDX        | <- push edx
|       ECX        | <- push ecx
|       EBX        | <- push ebx
|       EAX        | <- push eax
+------------------+ <- ESP (save complete)
                     ↑ Save this position to threadA->esp

🔄 Stack switching:
mov [old_esp], esp   ; threadA->esp = current ESP
mov esp, new_esp     ; ESP = threadB->esp

📤 ThreadB restore process (restore order):
pop eax → pop ebx → ... → pop ebp → popfd → ret

ThreadB stack (before restore):
+------------------+
|  ThreadB Data    |
+------------------+
|      EFLAGS      | ← Restore with popfd
|       EBP        | ← Restore with pop ebp
|       EDI        | ← Restore with pop edi
|       ESI        | ← Restore with pop esi
|       EDX        | ← Restore with pop edx
|       ECX        | ← Restore with pop ecx
|       EBX        | ← Restore with pop ebx
|       EAX        | ← Restore with pop eax
+------------------+ <- ESP (ThreadB start)

After restore complete, ThreadB resumes execution
```

**Initial Stack Construction Details:**

```c
// Stack construction during thread initialization
static void init_stack(thread_t* t, void (*func)(void)) {
    uint32_t* sp = &t->stack[1024];

    // Stack values in order that context_switch will restore them
    *--sp = (uint32_t)func;    // Function address (ret will jump here)
    *--sp = 0x00000002;        // EFLAGS (IF=0, reserved bit=1)
    *--sp = 0;                 // EBP
    *--sp = 0;                 // EDI
    *--sp = 0;                 // ESI
    *--sp = 0;                 // EDX
    *--sp = 0;                 // ECX
    *--sp = 0;                 // EBX
    *--sp = 0;                 // EAX

    t->esp = (uint32_t)sp;     // Final ESP position
}
```

### Complete Implementation Code

**boot/context_switch.s**

```assembly
; Day 08 Complete Version - Context Switch (32-bit)
[bits 32]

; void context_switch(uint32_t* old_esp, uint32_t* new_esp)
; Standard context switch - save and restore register state
global context_switch
context_switch:
  ; Save current register state
  ; Note: sti not needed here. EFLAGS restored by popfd, so
  ;       strictly follow save→switch→restore order to prevent interrupt re-entry
  pushfd                  ; Save EFLAGS (including IF)
  push ebp
  push edi
  push esi
  push edx
  push ecx
  push ebx
  push eax

  ; Save current ESP to old_esp
  mov  eax, [esp+36]      ; old_esp pointer (8*4 + 4)
  mov  [eax], esp         ; Save current ESP

  ; Switch to new ESP
  mov  esp, [esp+40]      ; new_esp value (8*4 + 8)

  ; Restore new thread's register state
  ; Restore in order consistent with initial stack (EAX..EBP, EFLAGS, function address)
  pop eax
  pop ebx
  pop ecx
  pop edx
  pop esi
  pop edi
  pop ebp
  popfd                   ; Restore EFLAGS (IF also restored)
  ret                     ; First time: jump to function address, later: return to caller
```

**kernel.c (Complete Implementation)**

```c
// Day 08 Complete Version - Minimal Context Switch Demo
#include <stdint.h>
#include "io.h"
#include "vga.h"

// --- Simple VGA Implementation ---
static volatile uint16_t* const VGA_MEM = (uint16_t*)0xB8000;
static uint16_t cx, cy;
static uint8_t col = 0x0F;
static inline uint16_t ve(char c, uint8_t a) {
    return (uint16_t)c | ((uint16_t)a << 8);
}
void vga_set_color(vga_color_t f, vga_color_t b) {
    col = (uint8_t)f | ((uint8_t)b << 4);
}
void vga_move_cursor(uint16_t x, uint16_t y) {
    cx = x; cy = y;
    uint16_t p = y * VGA_WIDTH + x;
    outb(0x3D4, 14); outb(0x3D5, (p >> 8) & 0xFF);
    outb(0x3D4, 15); outb(0x3D5, p & 0xFF);
}
void vga_clear(void) {
    for (uint16_t y = 0; y < VGA_HEIGHT; y++)
        for (uint16_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEM[y * VGA_WIDTH + x] = ve(' ', col);
    vga_move_cursor(0, 0);
}
void vga_putc(char c) {
    if (c == '
') { cx = 0; cy++; vga_move_cursor(cx, cy); return; }
    VGA_MEM[cy * VGA_WIDTH + cx] = ve(c, col);
    if (++cx >= VGA_WIDTH) { cx = 0; cy++; }
    vga_move_cursor(cx, cy);
}
void vga_puts(const char* s) { while (*s) vga_putc(*s++); }
void vga_init(void) { vga_set_color(VGA_WHITE, VGA_BLACK); vga_clear(); }

// --- Serial (COM1) Debug ---
#define COM1 0x3F8
static inline void serial_init(void) {
    outb(COM1 + 1, 0x00); outb(COM1 + 3, 0x80); outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00); outb(COM1 + 3, 0x03); outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}
static inline void serial_putc(char c) {
    while (!(inb(COM1 + 5) & 0x20)) {}
    outb(COM1 + 0, (uint8_t)c);
}
static inline void serial_puts(const char* s) {
    while (*s) { if (*s == '
') serial_putc('
'); serial_putc(*s++); }
}
static inline void serial_puthex(uint32_t v) {
    const char* h = "0123456789ABCDEF";
    for (int i = 7; i >= 0; i--) serial_putc(h[(v >> (i * 4)) & 0xF]);
}

// --- Day08: Context Switch ---
typedef struct thread {
    uint32_t stack[1024];
    uint32_t esp;
    int row;
} thread_t;
extern void context_switch(uint32_t** old_esp, uint32_t* new_esp);

static void init_stack(thread_t* t, void (*func)(void)) {
    uint32_t* sp = &t->stack[1024];

    // Correct thread initialization - stack layout compatible with context_switch
    // Stack values in order that context_switch will restore them
    *--sp = (uint32_t)func;  // Function address (ret destination for first execution)
    // Day08: IF=0 since interrupt infrastructure not yet set up (reserved bit=1)
    // EFLAGS = 0x00000002 (IF=0, required reserved bit=1)
    *--sp = 0x00000002;  // EFLAGS (IF=0, reserved bit=1)
    *--sp = 0;           // EBP
    *--sp = 0;           // EDI
    *--sp = 0;           // ESI
    *--sp = 0;           // EDX
    *--sp = 0;           // ECX
    *--sp = 0;           // EBX
    *--sp = 0;           // EAX

    t->esp = (uint32_t)sp;
}

static thread_t th1, th2;
static uint32_t* current_esp = 0;

static void threadA(void) {
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    // Display once and idle (Day08 doesn't use interrupts, continuous drawing causes flicker)
    vga_move_cursor(0, 10);
    vga_puts("Thread A running...");
    serial_puts("Thread A running
");
    for(;;) { __asm__ volatile("nop"); }
}
static void threadB(void) {
    // Day08 won't reach here
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_move_cursor(0, 11);
    vga_puts("Thread B running...");
    serial_puts("[This is NOT printed] Thread B running
");
    for(;;) { __asm__ volatile("nop"); }
}

void kmain(void) {
    serial_init();
    serial_puts("KMAIN begin
");
    vga_init();
    vga_puts("Day 08: Context Switch demo
");
    th1.row = 10; th2.row = 11;
    init_stack(&th1, threadA);
    init_stack(&th2, threadB);
    serial_puts("SWITCH to th1 esp="); serial_puthex(th1.esp); serial_puts("
");
    // Switch to first thread
    context_switch(&current_esp, (uint32_t*)th1.esp);
    serial_puts("RETURNED unexpectedly
");
    for(;;) { __asm__ volatile("hlt"); }
}
```

### Implementation Key Points

**Assembly Implementation Points**

-   **C Symbol**: `void context_switch(uint32_t** old_esp, uint32_t* new_esp);`
-   **Save Order**: `pushfd` → `push ebp` → `push edi` → `push esi` → `push edx` → `push ecx` → `push ebx` → `push eax`
-   **ESP Switch**: `mov [old_esp], esp` → `mov esp, new_esp`
-   **Restore Order**: `pop eax` → `pop ebx` → `pop ecx` → `pop edx` → `pop esi` → `pop edi` → `pop ebp` → `popfd` → `ret`
-   **Important**: Save and restore orders are reversed (stack characteristic)

**Initial Stack Design**

-   **Initialization Function**: `init_stack(thread_t* t, void (*func)(void))`
-   **Stack Order**: Function address → EFLAGS → EBP → EDI → ESI → EDX → ECX → EBX → EAX
-   **EFLAGS Setting**: Day 08 uses `0x00000002` (IF=0) since interrupts not set up yet
-   **ESP Setting**: After initialization, save address of last stacked value to `t->esp`

**Calling Method**

-   **Function Call**: `context_switch(&prev->esp, (uint32_t*)next->esp);`
-   **Parameter Explanation**:
-   `old_esp`: Pointer to location to save current ESP
-   `new_esp`: ESP value of target thread to switch to
-   **Operation Verification**: After thread start, display + `nop` loop (no preemption)

## Troubleshooting

-   **Does not return after switching**: Review the order/values of the initial stack. Pay special attention to the positions of EFLAGS and the function address.
-   **System crashes**: Ensure the order of pushfd/push and popfd/pop matches.
-   **Interrupt-related issues**: In Day08, run with IF=0 (interrupts disabled).

## Understanding Check

1.  Why do we write back the ESP _after saving_ into `old_esp`?
2.  What should be pushed onto the initial stack, and in what order?
3.  Why is the save order reversed when restoring?

## Next Steps

In Day 09, integrate with timer interrupts and implement a round-robin preemptive scheduler.
