# Day 09: Preemptive Multithreading (Round-Robin) 🔀

---

🌐 Available languages:

[English](./README.md) | [日本語](./README_ja.md)

## Today's Goal

Implement preemptive multithreading using timer interrupts and create a round-robin scheduler that automatically switches threads.

## Background

In Day 8, we implemented context switching but thread switching was manual. Today we'll implement preemptive scheduling using timer interrupts where the OS automatically switches threads, creating a true multitasking environment.

## New Concepts

-   **Preemptive Scheduling**: A method where the OS forcibly switches threads using timer interrupts. This contrasts with cooperative scheduling (where threads voluntarily yield the CPU) and guarantees real-time response and fairness.

## Learning Content

-   Time slice design (e.g., 10ms × N)
-   READY circular list rotation and next thread selection
-   Scheduling and EOI within IRQ0 handler
-   Saving current thread's `esp` and switching to next thread
-   PIC/PIT/IDT initialization and configuration

## Task List

-   [ ] Remap PIC and assign IRQ0 to IDT entry 32
-   [ ] Initialize PIT to 100Hz for periodic timer interrupts
-   [ ] Implement timer interrupt handler in interrupt.s
-   [ ] Implement round-robin scheduler in kernel.c and manage READY list
-   [ ] Call schedule() every time slice (10 ticks) to perform thread switching
-   [ ] Send EOI at appropriate timing to continue interrupts
-   [ ] Create demo threads and verify preemptive switching
-   [ ] Verify operation in QEMU, confirming multiple threads execute alternately

## Configuration

```
boot/boot.s, boot/kernel_entry.s
boot/interrupt.s        # IRQ0 handler and ISR stubs
boot/context_switch.s   # Context switch + initial_context_switch
io.h, vga.h
kernel.c                # Scheduler + IRQ0 integration
Makefile
```

## Implementation Guide

### Timer Interrupt Scheduling Mechanism

```
🕒 Timer Interrupt (IRQ0) 100Hz
     ↓
📥 timer_interrupt_handler (Assembly)
     ↓
🔧 timer_handler_c() (C language)
     ↓
✅ eoi_master()         # Send EOI first (important)
     ↓
📈 tick++               # Update timer count
     ↓
⏰ Time slice determination
     ↓
🔄 schedule()          # Thread switch when needed
     ↓
🏃 context_switch()    # Save registers → restore
```

**Important: EOI Sending Timing**

-   Send EOI **before** `schedule()`
-   Reason: Enable reception of next interrupt during thread switching

### Complete Implementation Code

**boot/interrupt.s**

```assembly
; Day 09 Complete Version - Exception ISR + IRQ0 stub with common handler (32-bit)
[bits 32]

%macro ISR_NOERR 1
  global isr%1
isr%1:
  push dword 0
  push dword %1
  jmp isr_common
%endmacro

%macro ISR_ERR 1
  global isr%1
isr%1:
  push dword %1
  jmp isr_common
%endmacro

extern isr_handler_c
extern timer_handler_c

; Common handler for exceptions (no context switch)
isr_common:
  pusha
  cld
  mov eax, esp
  push eax
  call isr_handler_c
  add esp, 4
  popa
  add esp, 8
  sti                   ; Explicitly enable interrupts
  iretd

; Timer interrupt dedicated handler (context switch compatible)
global timer_interrupt_handler
timer_interrupt_handler:
    ; Save all general-purpose registers to stack
    pusha                   ; Save EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI

    ; Save segment registers as well
    push ds
    push es
    push fs
    push gs

    ; Switch to kernel data segment
    mov ax, 0x10            ; Data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Call timer handler written in C
    ; [IMPORTANT] Here scheduler operates and current_thread may change
    call timer_handler_c

    ; Restore segment registers
    pop gs
    pop fs
    pop es
    pop ds

    ; Restore general-purpose registers
    ; [IMPORTANT] Registers restored here become those of another thread
    ; if thread was switched in timer_handler_c!
    popa

    ; End interrupt (return to original processing with IRET)
    iret

ISR_NOERR 0
ISR_NOERR 3
ISR_NOERR 6
ISR_ERR   13
ISR_ERR   14
```

**boot/context_switch.s (Day09 Version)**

```assembly
; Day 09 Complete Version - Context Switch (32-bit)
[bits 32]

; void context_switch(uint32_t* old_esp, uint32_t* new_esp)
; Standard context switch - save and restore register state
global context_switch
context_switch:
  ; Save current register state
  ; Save order: EFLAGS → EBP → EDI → ESI → EDX → ECX → EBX → EAX
  pushfd                    ; Save EFLAGS (including IF)
  push ebp
  push edi
  push esi
  push edx
  push ecx
  push ebx
  push eax

  ; Save current ESP to old_esp
  mov  eax, [esp+36]        ; old_esp pointer (8*4 + 4)
  mov  [eax], esp           ; Save current ESP

  ; Switch to new ESP
  mov  esp, [esp+40]        ; new_esp value (8*4 + 8)

  ; Restore new thread's register state
  ; Restore order: EAX → EBX → ECX → EDX → ESI → EDI → EBP → EFLAGS
  pop eax
  pop ebx
  pop ecx
  pop edx
  pop esi
  pop edi
  pop ebp
  popfd                     ; Restore EFLAGS with interrupt enable state (IF restored)

  ; Return to caller (new thread's continuation point / first time at function start with ret)
  ret

; Initial context switch (for first thread only)
; initial_context_switch(new_esp)
global initial_context_switch
initial_context_switch:
  ; Argument: [esp+4] = new_esp (ESP of destination thread)
  mov eax, [esp+4]
  mov esp, eax

  ; Restore registers/EFLAGS pushed to thread initial stack
  pop eax
  pop ebx
  pop ecx
  pop edx
  pop esi
  pop edi
  pop ebp
  popfd                  ; Restore including interrupt state

  ; Transition to function address at stack top
  pop eax                ; Get function address
  jmp eax                ; Direct jump (no return address needed)
```

**kernel.c (Complete Implementation)**

```c
// Day 09 Complete Version - Preemptive Round-Robin (Simple)
#include <stdint.h>

#include "io.h"
#include "vga.h"

// --- Simple VGA ---
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
    cx = x;
    cy = y;
    uint16_t p = y * VGA_WIDTH + x;
    outb(0x3D4, 14);
    outb(0x3D5, (p >> 8) & 0xFF);
    outb(0x3D4, 15);
    outb(0x3D5, p & 0xFF);
}
void vga_clear(void) {
    for (uint16_t y = 0; y < VGA_HEIGHT; y++)
        for (uint16_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEM[y * VGA_WIDTH + x] = ve(' ', col);
    vga_move_cursor(0, 0);
}
void vga_putc(char c) {
    if (c == '
') {
        cx = 0;
        cy++;
        vga_move_cursor(cx, cy);
        return;
    }
    VGA_MEM[cy * VGA_WIDTH + cx] = ve(c, col);
    if (++cx >= VGA_WIDTH) {
        cx = 0;
        cy++;
    }
    vga_move_cursor(cx, cy);
}
void vga_puts(const char* s) {
    while (*s) vga_putc(*s++);
}
void vga_putnum(uint32_t n) {
    int i = 0;
    if (n == 0) {
        vga_putc('0');
        return;
    }
    uint32_t x = n;
    char r[10];
    while (x) {
        r[i++] = '0' + (x % 10);
        x /= 10;
    }
    while (i--) vga_putc(r[i]);
}
void vga_init(void) {
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_clear();
}

// --- Serial (COM1) Debug ---
#define COM1 0x3F8
static inline void serial_init(void) {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}
static inline void serial_putc(char c) {
    while (!(inb(COM1 + 5) & 0x20)) {
    }
    outb(COM1 + 0, (uint8_t)c);
}
static inline void serial_puts(const char* s) {
    while (*s) {
        if (*s == '
')
            serial_putc('
');
        serial_putc(*s++);
    }
}
static inline void serial_puthex(uint32_t v) {
    const char* h = "0123456789ABCDEF";
    for (int i = 7; i >= 0; i--) serial_putc(h[(v >> (i * 4)) & 0xF]);
}

// --- Thread/Scheduler ---
typedef struct thread {
    uint32_t stack[1024];
    uint32_t esp;
    int row;
    uint32_t cnt;
    struct thread* next;
} thread_t;
extern void context_switch(uint32_t* old_esp, uint32_t new_esp);
extern void initial_context_switch(uint32_t new_esp);

static thread_t *current = 0, *ready = 0;
static volatile uint32_t tick = 0;
static uint32_t slice_ticks = 10;  // About 100ms at 100Hz

static void ready_push(thread_t* t) {
    if (!t)
        return;
    if (!ready) {
        ready = t;
        t->next = t;
        return;
    }
    thread_t* last = ready;
    while (last->next != ready) last = last->next;
    t->next = ready;
    last->next = t;
}

static void init_stack(thread_t* t, void (*func)(void)) {
    uint32_t* sp = &t->stack[1024];

    // Correct thread initialization - stack layout compatible with switch_context
    // Stack according to order restored by switch_context
    *--sp =
        (uint32_t)func;  // Function address (becomes ret destination on first thread execution)
    *--sp = 0x00000202;  // EFLAGS (IF=1, reserved bit=1)
    *--sp = 0;           // EBP
    *--sp = 0;           // EDI
    *--sp = 0;           // ESI
    *--sp = 0;           // EDX
    *--sp = 0;           // ECX
    *--sp = 0;           // EBX
    *--sp = 0;           // EAX

    t->esp = (uint32_t)sp;
}

// PIC/PIT/IDT
#define PIC1_CMD 0x20
#define PIC1_DATA 0x21
#define PIC2_CMD 0xA0
#define PIC2_DATA 0xA1
#define PIC_EOI 0x20
static inline void eoi_master(void) {
    outb(PIC1_CMD, PIC_EOI);
}
static void remap_pic(void) {
    uint8_t a1 = inb(PIC1_DATA), a2 = inb(PIC2_DATA);
    outb(PIC1_CMD, 0x11);
    outb(PIC2_CMD, 0x11);
    outb(PIC1_DATA, 0x20);
    outb(PIC2_DATA, 0x28);
    outb(PIC1_DATA, 0x04);
    outb(PIC2_DATA, 0x02);
    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);
    outb(PIC1_DATA, a1);
    outb(PIC2_DATA, a2);
}
static void set_masks(uint8_t m, uint8_t s) {
    outb(PIC1_DATA, m);
    outb(PIC2_DATA, s);
}
#define PIT_CH0 0x40
#define PIT_CMD 0x43
static void init_pit_100hz(void) {
    uint16_t div = 11932;
    outb(PIT_CMD, 0x36);
    outb(PIT_CH0, div & 0xFF);
    outb(PIT_CH0, (div >> 8) & 0xFF);
}

// IDT
typedef struct {
    uint16_t lo, sel;
    uint8_t zero, flags;
    uint16_t hi;
} __attribute__((packed)) idt_entry_t;
typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_ptr_t;
static idt_entry_t idt[256];
static idt_ptr_t idtr;
static inline void lidt(void* p) {
    __asm__ volatile("lidt (%0)" ::"r"(p));
}
static void set_gate(int n, uint32_t h) {
    idt[n].lo = h & 0xFFFF;
    idt[n].sel = 0x08;
    idt[n].zero = 0;
    idt[n].flags = 0x8E;
    idt[n].hi = (h >> 16) & 0xFFFF;
}
extern void isr0(void);
extern void isr3(void);
extern void isr6(void);
extern void isr13(void);
extern void isr14(void);
extern void timer_interrupt_handler(void);
static void idt_init(void) {
    for (int i = 0; i < 256; i++) set_gate(i, 0);
    set_gate(0, (uint32_t)isr0);
    set_gate(3, (uint32_t)isr3);
    set_gate(6, (uint32_t)isr6);
    set_gate(13, (uint32_t)isr13);
    set_gate(14, (uint32_t)isr14);
    set_gate(32, (uint32_t)timer_interrupt_handler);
    idtr.limit = sizeof(idt) - 1;
    idtr.base = (uint32_t)&idt[0];
    lidt(&idtr);
}

// Common IRQ handler
struct isr_stack {
    uint32_t regs[8];
    uint32_t int_no;
    uint32_t err;
};
/* context_switch is declared above. */
static uint32_t cur_esp = 0;
static uint32_t last_slice_tick = 0;

static void schedule(void) {
    serial_puts("schedule() called
");

    if (!ready || !current) {
        serial_puts("schedule: no ready or current thread
");
        return;
    }

    thread_t* next = ready->next;  // Current next candidate
    serial_puts("schedule: next=");
    serial_puthex((uint32_t)next);
    serial_puts("
");

    if (next == current) {
        serial_puts("schedule: next==current, no switch
");
        return;
    }

    // Advance ready pointer to next thread
    ready = ready->next;

    thread_t* prev = current;
    current = next;

    // Debug: output current/next threads and ESP
    serial_puts("SCHED prev=");
    serial_puthex((uint32_t)prev);
    serial_puts(" next=");
    serial_puthex((uint32_t)current);
    serial_puts(" prevESP=");
    serial_puthex(prev->esp);
    serial_puts(" nextESP=");
    serial_puthex(current->esp);
    serial_puts("
");
    context_switch(&prev->esp, current->esp);
}

// Dedicated timer interrupt handler
void timer_handler_c(void) {
    // EOI must be sent BEFORE schedule() - critical for continued timer
    // interrupts
    eoi_master();

    tick++;
    if ((tick & 0x3F) == 0) {
        serial_puts("TICK
");
    }
    if ((tick - last_slice_tick) >= slice_ticks) {
        last_slice_tick = tick;
        serial_puts("Timer calling schedule
");
        schedule();
    }
}

void isr_handler_c(struct isr_stack* f) {
    // Simple exception display (also output to serial)
    serial_puts("EXC vec=");
    serial_puthex(f->int_no);
    serial_puts("
");
    vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
    vga_puts("[EXC] vec=");
    vga_putnum(f->int_no);
    vga_putc('
');
}

// Demo threads
static thread_t th1, th2;
static void threadA(void) {
    serial_puts("threadA start
");
    for (;;) {
        ++th1.cnt;
        if ((th1.cnt & 0xFFFF) == 0) {
            vga_move_cursor(0, 10);
            vga_set_color(VGA_YELLOW, VGA_BLACK);
            vga_puts("A:"), vga_putnum(th1.cnt), vga_puts("   ");
        }
        if ((th1.cnt & 0xFFFFFF) == 0) {
            serial_puts("threadA alive
");
        }
    }
}

static void threadB(void) {
    serial_puts("threadB start
");
    for (;;) {
        ++th2.cnt;
        if ((th2.cnt & 0xFFFF) == 0) {
            vga_move_cursor(0, 11);
            vga_set_color(VGA_CYAN, VGA_BLACK);
            vga_puts("B:");
            vga_putnum(th2.cnt);
            vga_puts("   ");
        }
        if ((th2.cnt & 0xFFFFFF) == 0) {
            serial_puts("threadB alive
");
        }
    }
}

void kmain(void) {
    serial_init();
    serial_puts("D09 kmain begin
");
    vga_init();
    vga_puts("Day 09: Preemptive RR
");
    // Thread preparation
    th1.row = 10;
    th2.row = 11;
    th1.cnt = th2.cnt = 0;
    th1.next = th2.next = 0;
    init_stack(&th1, threadA);
    init_stack(&th2, threadB);
    ready_push(&th1);
    ready_push(&th2);
    current = &th1;  // First current

    remap_pic();
    set_masks(0xFE, 0xFF);
    idt_init();
    init_pit_100hz();
    serial_puts("PIC/IDT/PIT ready, sti
");
    __asm__ volatile("sti");
    // To first thread (start from initial stack, don't use start_first)
    serial_puts("START first thread esp=");
    serial_puthex(current->esp);
    serial_puts("
");
    initial_context_switch(current->esp);
}
```

### Implementation Points

**Scheduler Design**

-   **READY Circular List**: Create circular list with `ready_push()`
-   **Round-Robin**: Advance `ready` pointer by 1 in `schedule()`
-   **Time Slice**: Switch threads every 10 ticks (about 100ms)

**Timer Interrupt Processing**

-   **EOI Send Timing**: Always execute before `schedule()`
-   **Interrupt Continuation**: Receive next interrupt during thread switching
-   **Debug Output**: "TICK" message every 64 ticks to confirm operation

**Initialization Order**

1. `remap_pic()` - Relocate PIC to 32 and above
2. `set_masks()` - Enable only IRQ0
3. `idt_init()` - Set IDT table
4. `init_pit_100hz()` - Set timer to 100Hz
5. `sti` - Enable interrupts
6. `initial_context_switch()` - Start first thread

## Troubleshooting

-   **Screen display corrupted**: Set different `row` for each thread output
-   **System hang**: Check READY list update order
-   **Timer stops**: Check EOI send timing (send before schedule)
-   **No thread switching**: Check `slice_ticks` value and time slice determination logic

## Understanding Check

1.  Why do we call `schedule()` from IRQ handler (interrupt context)?
2.  What changes when you modify the time slice length?
3.  Why is it necessary to send EOI before `schedule()`?

## Next Steps

In Day 10, we'll introduce `sleep()` and implement blocking/wake-up using timer ticks.