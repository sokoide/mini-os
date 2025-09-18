# Day 07: Thread Data Structures (TCB) 🧵

## Today's Goal
Design and implement in C the Thread Control Block (TCB) and READY list that form the foundation of multithreaded OS.

## Background
While we can now generate timer interrupts in Day 6, OS development requires executing multiple tasks concurrently. Today we'll design TCB data structures for managing thread state and READY lists for managing threads, building the foundation for multithreading. Today focuses on design, with actual context switching implementation in the next session.

## New Concepts
- **TCB (Thread Control Block)**: Data structure for storing thread state. Contains information like register values, stack pointer, execution state, priority, etc. One is assigned per thread, manipulated by the scheduler.

## Learning Content

-   Thread states (READY/RUNNING/BLOCKED) and blocking reasons
-   Thread Control Block (TCB) design
-   Stack area allocation and initialization design (utilized in next session's context switching)
-   Basic functions for READY list (circular list) management
-   Thread creation API (template) design

【Note】Minimal TCB set
- First, align `esp` (stack pointer), `state` (READY/RUNNING/BLOCKED), `next`/`next_ready` (for READY circulation), `next_blocked` (for BLOCKED chain) as minimal configuration.

## Task List
- [ ] Define enum types for thread states (READY/RUNNING/BLOCKED) and blocking reasons
- [ ] Design TCB structure and define fields like stack, state, counters
- [ ] Define kernel context structure to manage current_thread, ready_thread_list, etc.
- [ ] Implement circular list operation functions (push_back, pop_front) for READY list
- [ ] Implement thread creation API (create_thread) for attribute setting and READY insertion
- [ ] Create demo threads in kmain and confirm READY list initialization
- [ ] Verify operation in QEMU and display list initialization messages

## Prerequisites Check

### Required Knowledge

-   Day 01-06 (boot, GDT, VGA, IDT/exceptions, timer IRQ)
-   C structures, enums, pointer basics

### What's New Today

-   Safe TCB layout (reasons for placing stack at beginning)
-   READY queue using circular singly-linked list
-   Thread attributes (display row, counter, delay, etc.)

## Approach and Configuration

Like Day 05/06, keep assembly minimal (under boot/) and create data structures mainly in C.

```
├── boot
│   ├── boot.s           # 16-bit: A20, built-in GDT, load kernel with INT 13h, PE switch
│   └── kernel_entry.s   # 32-bit: segment/stack setup → kmain()
├── io.h                 # inb/outb/io_wait (reuse existing)
├── kernel.c             # TCB definition, ready list, thread creation API template
├── vga.h                # VGA API (reuse existing)
└── Makefile             # Link boot + kernel
```

See `day99_completed/include/kernel.h` and `src/kernel.c` for overall picture and naming reference (similar thread structure and ready queue concepts).

## Implementation Guide (Example)

The following are implementation guidelines. Write source comments carefully in the actual code.

### 1. Thread States and Blocking Reasons (C)

```c
// Thread states
typedef enum {
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED
} thread_state_t;

// Blocking reasons
typedef enum {
    BLOCK_REASON_NONE,
    BLOCK_REASON_TIMER,    // sleep() etc.
    BLOCK_REASON_KEYBOARD  // input wait etc.
} block_reason_t;
```

### 2. Thread Control Block (TCB) (C)

Consider safe layout like "place stack array first to protect ESP from stack overflow".

```c
#define MAX_THREADS       4
#define THREAD_STACK_SIZE 1024  // 4KB (uint32_t × 1024)

typedef struct thread {
    uint32_t stack[THREAD_STACK_SIZE]; // Place at beginning (for protection)
    thread_state_t state;              // Thread state
    uint32_t counter;                  // Arbitrary counter (for demo)
    uint32_t delay_ticks;              // Counter update interval (tick)
    uint32_t last_tick;                // Last updated tick
    block_reason_t block_reason;       // Blocking reason
    uint32_t wake_up_tick;             // Scheduled wake up tick
    int display_row;                   // Display row
    struct thread* next_ready;         // For READY circular list
    struct thread* next_blocked;       // For BLOCKED list
    uint32_t esp;                      // Stack pointer (used in context switching)
} thread_t;
```

### 3. Kernel Context (C)

Consolidate "currently running", "READY list head", "BLOCKED list head", "tick" in one place.

```c
typedef struct {
    thread_t* current_thread;
    thread_t* ready_thread_list;   // Head of circular list
    thread_t* blocked_thread_list; // Optional: can be unused today
    uint32_t system_ticks;         // Reference Day 06 tick
} kernel_context_t;

static kernel_context_t g_ctx = {0};
```

### 4. READY List Operations (C)

Implement as circular singly-linked list and organize basics of insertion/deletion.

```c
// Add to end of list (if head is NULL, become 1-element circular pointing to self)
static void ready_list_push_back(thread_t* t) {
    if (!t) return;
    if (!g_ctx.ready_thread_list) {
        g_ctx.ready_thread_list = t;
        t->next_ready = t;
        return;
    }
    thread_t* head = g_ctx.ready_thread_list;
    thread_t* last = head;
    while (last->next_ready != head) last = last->next_ready;
    t->next_ready = head;
    last->next_ready = t;
}

// Pop from front (empty: NULL, 1 element: to NULL)
static thread_t* ready_list_pop_front(void) {
    thread_t* head = g_ctx.ready_thread_list;
    if (!head) return NULL;
    if (head->next_ready == head) {
        g_ctx.ready_thread_list = NULL;
        head->next_ready = NULL;
        return head;
    }
    thread_t* last = head;
    while (last->next_ready != head) last = last->next_ready;
    thread_t* ret = head;
    g_ctx.ready_thread_list = head->next_ready;
    last->next_ready = g_ctx.ready_thread_list;
    ret->next_ready = NULL;
    return ret;
}
```

### 5. Thread Creation API (Template) (C)

Full stack initialization and context switching will be implemented in Day 08 onwards. Here prepare up to attribute setting and READY insertion.

```c
static thread_t g_threads[MAX_THREADS];
static int g_thread_count = 0;

static thread_t* alloc_thread_slot(void) {
    if (g_thread_count >= MAX_THREADS) return NULL;
    return &g_threads[g_thread_count++];
}

// Stack initialization will be fully implemented next time (Day 08)
static void initialize_thread_stack_stub(thread_t* t, void (*func)(void)) {
    (void)func;
    t->esp = (uint32_t)&t->stack[THREAD_STACK_SIZE];
}

// Set representative attributes and insert into READY
int create_thread(void (*func)(void), uint32_t delay_ticks, int display_row, thread_t** out) {
    if (!func || !out) return -1;
    *out = NULL;
    thread_t* t = alloc_thread_slot();
    if (!t) return -2;

    // Attributes
    t->state = THREAD_READY;
    t->counter = 0;
    t->delay_ticks = delay_ticks ? delay_ticks : 1;
    t->last_tick = 0;
    t->block_reason = BLOCK_REASON_NONE;
    t->wake_up_tick = 0;
    t->display_row = display_row;
    t->next_ready = NULL;
    t->next_blocked = NULL;

    initialize_thread_stack_stub(t, func);
    ready_list_push_back(t);
    *out = t;
    return 0;
}
```

### 6. Demo (kmain) (C)

For now, confirm up to READY list creation and visualize situation with display. Context switching will be added in Day 08.

```c
void demo_thread_func_A(void) { for(;;) { /* Implementation in Day 08 onwards */ } }
void demo_thread_func_B(void) { for(;;) { /* Implementation in Day 08 onwards */ } }

void kmain(void) {
    vga_init();
    vga_puts("Day 07: Thread TCB design\n");

    thread_t* t1 = NULL; thread_t* t2 = NULL;
    create_thread(demo_thread_func_A, 10, 10, &t1);
    create_thread(demo_thread_func_B, 20, 11, &t2);

    // Simple display of READY list head element presence
    if (g_ctx.ready_thread_list) vga_puts("READY list initialized\n");

    for(;;) { __asm__ volatile ("hlt"); }
}
```

### 7. Makefile (Example)

Same as Day 05/06, build with boot split + C kernel configuration (minimal interrupts required).

```makefile
AS   = nasm
CC   = i686-elf-gcc
LD   = i686-elf-ld
OBJCOPY = i686-elf-objcopy
QEMU = qemu-system-i386

CFLAGS = -ffreestanding -m32 -nostdlib -fno-stack-protector -fno-pic -O2 -Wall -Wextra

BOOT    = boot/boot.s
ENTRY   = boot/kernel_entry.s
KERNELC = kernel.c

BOOTBIN = boot.bin
KELF    = kernel.elf
KBIN    = kernel.bin
OS_IMG  = os.img

all: $(OS_IMG)
	@echo "✅ Day 07: TCB/READY list build complete"
	@echo "🚀 make run to start"

$(BOOTBIN): $(BOOT)
	$(AS) -f bin $(BOOT) -o $(BOOTBIN)

kernel_entry.o: $(ENTRY)
	$(AS) -f elf32 $(ENTRY) -o $@

kernel.o: $(KERNELC) vga.h io.h
	$(CC) $(CFLAGS) -c $(KERNELC) -o $@

$(KELF): kernel_entry.o kernel.o
	$(LD) -m elf_i386 -Ttext 0x00010000 -e kernel_entry -o $(KELF) kernel_entry.o kernel.o

$(KBIN): $(KELF)
	$(OBJCOPY) -O binary $(KELF) $(KBIN)

$(OS_IMG): $(BOOTBIN) $(KBIN)
	cat $(BOOTBIN) $(KBIN) > $(OS_IMG)

run: $(OS_IMG)
	$(QEMU) -fda $(OS_IMG) -boot a -serial stdio

clean:
	rm -f $(BOOTBIN) kernel_entry.o kernel.o $(KELF) $(KBIN) $(OS_IMG)

.PHONY: all run clean
```

## Troubleshooting

-   READY list remains empty
    -   Check return value of `create_thread` and logic of `ready_list_push_back`
    -   Validation for exceeding `MAX_THREADS` or `func==NULL`
-   No display output
    -   Check initialization procedure from Day 03-06 (boot/, VGA initialization)

## Understanding Check

1. Why does placing `stack[]` at the beginning of TCB improve safety?
2. What are the merits and precautions of having READY queue as circular singly-linked list?
3. What are the roles of `delay_ticks` / `last_tick` / `wake_up_tick`?
4. Which TCB fields are referenced during context switching?

## Next Steps

-   ✅ TCB (Thread Control Block) design
-   ✅ Basic READY list operations
-   ✅ Thread creation API template

Day 08 will implement context switching (assembly) and actually switch threads using TCB's `esp` and initial stack.