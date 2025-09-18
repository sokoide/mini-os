# Day 06: Timer Interrupts (PIC/PIT) ⏱️

## Today's Goal

Initialize hardware timer (PIT) and interrupt controller (PIC), generate 100Hz timer interrupts, and verify operation.

## Background

While we built an IDT in Day 5 to handle CPU exceptions, OS development also requires processing asynchronous interrupts from hardware. Today we'll remap the PIC and use PIT to generate periodic timer interrupts, establishing a hardware interrupt processing environment.

## New Concepts

-   **PIT (Programmable Interval Timer)**: 8254 chip that generates periodic interrupts as timer device. Divides reference clock 1.193182MHz to generate arbitrary periods. Channel 0 is used as system timer, forming foundation for scheduling.
-   **PIC (Programmable Interrupt Controller)**: 8259A chip that bundles IRQs from multiple devices and efficiently transmits to CPU. In BIOS settings, remaps IRQ0-15 to 32-47 for OS interrupt management.
-   **EOI (End Of Interrupt)**: Command to notify PIC that interrupt processing is complete. Necessary to allow next interrupt.

## Learning Content

-   PIC (Programmable Interrupt Controller) 8259A remapping (IRQ0→INT 32, etc.)
    -   What is PIC: Controller that transmits interrupt signals from external devices to CPU
    -   Remapping: Change BIOS initial interrupt vectors (0-15) to 32-47 for OS use
-   PIT (Programmable Interval Timer) 8254 initialization (Divisor setting)
    -   What is PIT: Timer chip that generates periodic interrupts
    -   Divisor: Set interrupt interval by dividing clock frequency (100Hz=10ms interval)
-   IRQ (external interrupt) handler registration and EOI (End Of Interrupt)
    -   IRQ: Interrupt Request, interrupt request from hardware
    -   EOI: Notify PIC that interrupt processing is complete, allow next interrupt
-   CPU interrupt enable (`sti`) and interrupt flow
    -   sti: Set Interrupt Flag, configure CPU to accept interrupts
    -   Interrupt flow: Device →PIC→CPU→ISR handler →EOI

## Task List

-   [ ] Remap PIC to assign IRQ0-15 to IDT 32-47
-   [ ] Initialize PIT to set 100Hz (10ms period) timer interrupts
-   [ ] Add IRQ0 stub to interrupt.s and perform register save/restore
-   [ ] Implement timer interrupt handler in kernel.c and update tick counter
-   [ ] Register IRQ0 (vector 32) to IDT and enable interrupt processing
-   [ ] Send EOI to PIC at appropriate timing
-   [ ] Enable CPU interrupts with sti instruction and confirm heartbeat display

## Prerequisites Check

### Required Knowledge

-   Day 01-05 (boot, GDT, protected mode, VGA, IDT/exceptions)
-   I/O port access (`inb/outb`)

### What's New Today

-   PIC initialization procedure with ICW1-ICW4 and mask settings
-   PIT command register (0x43) and channel 0 (0x40) divisor value settings
-   EOI issuing from hardware interrupt handlers

## Approach and Configuration

Like Day 05, keep assembly minimal (under boot/) and configure PIC/PIT/IDT in C.

```
├── boot
│   ├── boot.s            # 16-bit: A20, built-in GDT, load kernel with INT 13h, PE switch
│   ├── kernel_entry.s    # 32-bit: segment/stack setup → kmain()
│   └── interrupt.s       # Exception + IRQ stubs (IRQ0) and common entry
├── io.h                  # inb/outb/io_wait (C inline asm)
├── kernel.c              # PIC remapping + PIT setup + IDT registration + handlers + demo
├── vga.h                 # VGA API (C)
└── Makefile              # Link boot + kernel + interrupt to generate os.img
```

See `day99_completed` for reference (PIC/PIT/IDT role division and handler configuration).

## Implementation Guide (Example)

The following are implementation guidelines. Write source comments carefully in the actual code.

### 1. PIC Remapping (C)

Purpose: Relocate IRQ0-IRQ15 to IDT 32-47 (to avoid collision with CPU exceptions 0-31)

```c
#include "io.h"

#define PIC1_CMD  0x20  // Master PIC command
#define PIC1_DATA 0x21  // Master PIC data
#define PIC2_CMD  0xA0  // Slave PIC command
#define PIC2_DATA 0xA1  // Slave PIC data

#define PIC_EOI   0x20  // End Of Interrupt

static void remap_pic(void) {
    uint8_t a1 = inb(PIC1_DATA); // Save current mask
    uint8_t a2 = inb(PIC2_DATA);

    // ICW1: Initialize, edge trigger, ICW4 required
    outb(PIC1_CMD, 0x11);
    outb(PIC2_CMD, 0x11);

    // ICW2: Interrupt vector offset
    outb(PIC1_DATA, 0x20); // Master: 0x20 (32)
    outb(PIC2_DATA, 0x28); // Slave: 0x28 (40)

    // ICW3: Cascade wiring
    outb(PIC1_DATA, 0x04); // Master: Slave connected to IRQ2
    outb(PIC2_DATA, 0x02); // Slave: ID=2

    // ICW4: 8086/88 mode
    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);

    // Restore mask (write 0xFE/0xFF to enable only IRQ0 initially)
    outb(PIC1_DATA, a1);
    outb(PIC2_DATA, a2);
}

static void set_irq_masks(uint8_t master_mask, uint8_t slave_mask) {
    outb(PIC1_DATA, master_mask); // 0=enable, 1=disable
    outb(PIC2_DATA, slave_mask);
}

static inline void send_eoi_master(void) { outb(PIC1_CMD, PIC_EOI); }
static inline void send_eoi_slave(void)  { outb(PIC2_CMD, PIC_EOI); }
```

Example: To enable only IRQ0 (timer) initially → `set_irq_masks(0xFE, 0xFF);`

### 2. PIT Initialization (C)

Purpose: Generate interrupts at 100Hz frequency (10ms).
PIT clock is 1,193,182 Hz → divisor value `div = 1193182 / 100 = 11931(remainder)` (approximately 11932 is OK).

```c
#define PIT_CH0   0x40
#define PIT_CMD   0x43

static void init_pit_100hz(void) {
    uint16_t div = 11932; // 100Hz approximation
    outb(PIT_CMD, 0x36);  // Channel 0, low-high, mode 3 (square wave), binary
    outb(PIT_CH0, div & 0xFF);       // LSB
    outb(PIT_CH0, (div >> 8) & 0xFF); // MSB
}

【Note】0x36 command breakdown
- ch0 / LSB→MSB / mode 3 (square wave) / binary mode
```

### 3. IRQ0 (Timer) Stub (interrupt.s)

Add IRQ0 stub to Day 05's `interrupt.s` (use same common entry as exceptions).

```assembly
[bits 32]

global irq0
irq0:
  push dword 0          ; Dummy error code
  push dword 32         ; Vector number (remapped IRQ0)
  jmp isr_common        ; To same common entry as exceptions
```

When registering to IDT, call `set_idt_gate(32, (uint32_t)irq0);`.

### 4. IDT Registration and Handler (C)

Build IDT same as Day 05 and register IRQ0 (vector 32). In handler, increment `tick` counter and notify EOI to PIC (master EOI is OK since not slave side).

```c
extern void irq0(void);

static volatile uint32_t tick = 0;

void irq_handler_timer(void) {
    tick++;
    if ((tick % 100) == 0) { // About every second (100Hz)
        vga_puts(".");
    }
    send_eoi_master();
}

// Common handler coming from isr_common to C (example)
struct isr_stack { uint32_t regs[8]; uint32_t int_no; uint32_t err_code; };
void isr_handler_c(struct isr_stack* f) {
    if (f->int_no == 32) { irq_handler_timer(); return; }
    // Others (exceptions etc.) delegate to Day 05 processing
}

static void idt_init_with_timer(void) {
    // Omitted: Clear idt[] to 0 same as Day 05, register exceptions and necessary IRQs
    set_idt_gate(32, (uint32_t)irq0);
    load_idt();
}
```

### Supplement: What is EOI (End Of Interrupt) and How to Use?

EOI is a command to notify the PIC (8259A) that "interrupt processing is complete." The PIC maintains an "In-Service" bit for each IRQ line and won't pass the same IRQ to CPU again until it receives EOI. Therefore, forgetting to send EOI will prevent subsequent same IRQs from being delivered, causing symptoms like timer appearing to stop or keyboard becoming unresponsive.

- Basic forms
  - Master PIC (IRQ0–7): `outb(PIC1_CMD, 0x20)`
  - Slave PIC (IRQ8–15): First EOI to slave, then EOI to master (cascade release)

- When to send
  - For short handlers (counter increment, light logging, etc.), sending at end of processing is fine.
  - However, if handler might perform heavy processing or thread switching (scheduling), sending EOI first is practical. Sending EOI first ensures next IRQ won't be lost after switching, allowing timer etc. to continue operating.
  - This curriculum introduces preemption in Day 09, so for IRQ0 (PIT), we recommend the order "first EOI → then `tick++` or `schedule()`".

- Common pitfalls
  - Not sending EOI (or forgetting master EOI for slave interrupts)
  - Infinite loop etc. in handler prevents reaching EOI
  - Omitting EOI without enabling Auto-EOI mode (not covered in this material)

- Code example (PIT/IRQ0: master only)
  ```c
  void timer_handler_c(void) {
      send_eoi_master();       // Send EOI first to guarantee next IRQ
      tick++;
      if (tick - last >= slice) { /* schedule(); */ }
  }
  ```

### 5. kmain Initialization Order

```c
void kmain(void) {
    vga_init();
    vga_puts("Day 06: Timer IRQ (100Hz)\n");

    remap_pic();
    set_irq_masks(0xFE, 0xFF);  // Allow only IRQ0
    idt_init_with_timer();
    init_pit_100hz();

    __asm__ volatile ("sti");   // Enable CPU interrupts

    // Main loop (do nothing)
    for(;;) { __asm__ volatile ("hlt"); }
}
```

### 6. Makefile (Example)

-   Same as Day 05, build with boot split + C kernel configuration (including `interrupt.s`).
-   See `day06_complete/Makefile`.

```makefile
AS   = nasm
CC   = i686-elf-gcc
LD   = i686-elf-ld
OBJCOPY = i686-elf-objcopy
QEMU = qemu-system-i386

CFLAGS = -ffreestanding -m32 -nostdlib -fno-stack-protector -fno-pic -O2 -Wall -Wextra

BOOT    = boot/boot.s
ENTRY   = boot/kernel_entry.s
INTRASM = boot/interrupt.s   # Add IRQ0 stub
KERNELC = kernel.c

BOOTBIN = boot.bin
KELF    = kernel.elf
KBIN    = kernel.bin
OS_IMG  = os.img

all: $(OS_IMG)
	@echo "✅ Day 06: Timer interrupt build complete"
	@echo "🚀 make run to start"

$(BOOTBIN): $(BOOT)
	$(AS) -f bin $(BOOT) -o $(BOOTBIN)

kernel_entry.o: $(ENTRY)
	$(AS) -f elf32 $(ENTRY) -o $@

interrupt.o: $(INTRASM)
	$(AS) -f elf32 $(INTRASM) -o $@

kernel.o: $(KERNELC) vga.h io.h
	$(CC) $(CFLAGS) -c $(KERNELC) -o $@

$(KELF): kernel_entry.o interrupt.o kernel.o
	$(LD) -m elf_i386 -Ttext 0x00010000 -e kernel_entry -o $(KELF) kernel_entry.o interrupt.o kernel.o

$(KBIN): $(KELF)
	$(OBJCOPY) -O binary $(KELF) $(KBIN)

$(OS_IMG): $(BOOTBIN) $(KBIN)
	cat $(BOOTBIN) $(KBIN) > $(OS_IMG)

run: $(OS_IMG)
	$(QEMU) -fda $(OS_IMG) -boot a -serial stdio

clean:
	rm -f $(BOOTBIN) kernel_entry.o interrupt.o kernel.o $(KELF) $(KBIN) $(OS_IMG)

.PHONY: all run clean
```

## Troubleshooting

-   No periods appearing (heartbeat not visible)
    -   Calling `sti` and idling CPU with `hlt`?
    -   Are PIC masks correct (`0xFE` / `0xFF`)?
    -   Is `irq0` registered to IDT vector 32?
-   Stopping with exceptions
    -   Do stack processing in `isr_common` and C-side analysis match?
    -   Are exception and IRQ vector numbers (0-31 / 32-47) not mixed up?

## Understanding Check

1. Why is PIC remapping necessary?
2. How do you calculate the divisor value for 100Hz?
3. Where and when do you send EOI?
4. How do you distinguish between exception and IRQ handlers?

## Next Steps

-   ✅ PIC remapping and mask settings
-   ✅ PIT initialization (100Hz)
-   ✅ Heartbeat display in IRQ0 (timer) handler

Day 07 will apply time management using timer ticks (tick counter) as a foundation for threads and tasks.