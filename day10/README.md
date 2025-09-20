# Day 10: Sleep/Timing 💤

---

🌐 Available languages:

[English](./README.md) | [日本語](./README_ja.md)

## Today's Goal
Implement sleep() function using timer interrupts to realize thread blocking and wake-up mechanisms.

## Background
In Day 9, we implemented preemptive scheduling, but threads had no means to voluntarily yield the CPU. Today we'll implement timer-based sleep functionality, allowing threads to wait for specified time periods and use CPU efficiently.

## New Concepts
- **Blocking**: When threads stop execution for some reason (timer, I/O, etc.) and move from READY list to BLOCKED list.
- **Wake-up**: Returning threads that meet their waiting conditions from BLOCKED list back to READY list.

## Learning Content

- Thread state management (READY/RUNNING/BLOCKED)
- Timer-based waiting functionality (sleep)
- Classification of blocking reasons (BLOCK_REASON_TIMER, etc.)
- Time-ordered sorted blocked list
- CPU efficiency through idle thread
- Structured kernel context management

## Task List
- [ ] Extend thread states to READY/RUNNING/BLOCKED
- [ ] Implement data structure to manage BLOCKED list in time order
- [ ] Implement sleep() function to put threads in BLOCKED state
- [ ] Perform wake-up checks in timer interrupt handler
- [ ] Return threads with arrived wake-up time to READY list
- [ ] Implement idle thread to efficiently halt CPU
- [ ] Create multiple demo threads to verify operation with different sleep times
- [ ] Verify operation in QEMU, confirming thread blocking and wake-up

## Configuration

```
boot/boot.s, boot/kernel_entry.s
boot/interrupt.s        # Timer interrupt handler
boot/context_switch.s   # Context switch + initial_context_switch
io.h, vga.h
kernel.c                # Scheduler with sleep functionality
Makefile
```

## Scheduler Structure

In Day 10, we adopt a structured scheduler to support sleep functionality.

-   **State management with `kernel_context_t`**: Kernel state (currently executing thread, runnable list, waiting list, etc.) is centrally managed in the `kernel_context_t` structure. This improves overall system visibility.

-   **Two thread lists**:
    -   **READY list**: Circular list managing executable threads. The scheduler selects the next thread from here.
    -   **BLOCKED list**: List managing threads waiting due to `sleep` etc. Each timer interrupt checks if there are threads whose wake-up time has arrived.

-   **Role of `idle_thread`**: The system always has a special thread called `idle_thread`. This halts the CPU with `hlt` instruction when there are no other threads to execute. This allows the scheduler to operate safely even when no executable threads exist, preventing wasteful CPU consumption.

-   **Generic blocking mechanism**: The `block_current_thread` function is a generic mechanism for putting threads to wait for specific reasons (`block_reason_t`). In Day 10, we use this for timer waiting (`BLOCK_REASON_TIMER`).

## Implementation Guide

### Sleep Function Flow

```
🛌 sleep(ticks) called
     ↓
📤 remove_from_ready_list()     # Remove from READY list
     ↓
🔒 Change to THREAD_BLOCKED state
     ↓
⏰ wake_up_tick = current_ticks + ticks
     ↓
📋 Insert in time order to BLOCKED list
     ↓
🔄 schedule()                   # Switch to other thread
```

### Timer Interrupt Processing Flow

```
🕒 Timer Interrupt (IRQ0)
     ↓
✅ eoi_master()                 # Send EOI
     ↓
📈 system_ticks++               # Update system time
     ↓
🔍 check_and_wake_timer_threads() # Check wake-up time
     ↓
📤 Move from BLOCKED list → READY list
     ↓
🔄 schedule()                   # Thread switch when needed
```

### Complete Implementation Code

**Data Structure Definitions**

```c
// Thread state
typedef enum {
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
} thread_state_t;

// Block reason
typedef enum {
    BLOCK_REASON_NONE,
    BLOCK_REASON_TIMER,  // Sleep waiting
} block_reason_t;

// Thread Control Block (TCB)
typedef struct thread {
    uint32_t stack[1024];        // 4KB stack
    uint32_t esp;                // Stack pointer

    thread_state_t state;        // Thread state
    block_reason_t block_reason; // Block reason

    uint32_t counter;            // Counter (for demo)
    uint32_t delay_ticks;        // Sleep interval
    uint32_t last_tick;          // Last execution time
    uint32_t wake_up_tick;       // Scheduled wake-up time

    int display_row;             // Display row

    struct thread* next_ready;   // For READY list
    struct thread* next_blocked; // For BLOCKED list
} thread_t;

// Kernel context
typedef struct {
    thread_t* current_thread;     // Currently executing thread
    thread_t* ready_thread_list;  // READY circular list
    thread_t* blocked_thread_list; // BLOCKED time-ordered list
    volatile uint32_t system_ticks; // System time
    uint32_t scheduler_lock_count;  // Scheduler lock
} kernel_context_t;

static kernel_context_t k_context = {0};
```

**Sleep Function Implementation**

```c
// Sleep function
static void sleep(uint32_t ticks) {
    if (ticks == 0) {
        return;
    }

    if (ticks > MAX_COUNTER_VALUE) {
        ticks = MAX_COUNTER_VALUE;
    }

    if (!get_current_thread()) {
        return;
    }

    // Calculate wake-up time and block
    uint32_t wake_up_time = get_system_ticks() + ticks;
    block_current_thread(BLOCK_REASON_TIMER, wake_up_time);

    // Transfer control to other thread
    schedule();
}

// Generic block function
void block_current_thread(block_reason_t reason, uint32_t data) {
    asm volatile("cli");

    thread_t* thread = get_current_thread();
    if (!thread) {
        asm volatile("sti");
        return;
    }

    // Remove from READY list
    remove_from_ready_list(thread);

    // Set to blocked state
    thread->state = THREAD_BLOCKED;
    thread->block_reason = reason;
    thread->next_blocked = NULL;

    kernel_context_t* ctx = get_kernel_context();

    if (reason == BLOCK_REASON_TIMER) {
        thread->wake_up_tick = data;

        // Insert to BLOCKED list in time order
        if (!ctx->blocked_thread_list ||
            thread->wake_up_tick < ctx->blocked_thread_list->wake_up_tick) {
            // Insert at head of list
            thread->next_blocked = ctx->blocked_thread_list;
            ctx->blocked_thread_list = thread;
        } else {
            // Insert at appropriate position (time-ordered sort)
            thread_t* current = ctx->blocked_thread_list;
            while (current->next_blocked &&
                   current->next_blocked->wake_up_tick <= thread->wake_up_tick) {
                current = current->next_blocked;
            }
            thread->next_blocked = current->next_blocked;
            current->next_blocked = thread;
        }
    }

    asm volatile("sti");
}
```

**Timer Interrupt Processing**

```c
// Timer interrupt handler
void timer_handler_c(void) {
    // Send EOI first (important)
    eoi_master();

    kernel_context_t* ctx = get_kernel_context();
    ctx->system_ticks++;

    // Wake-up check for blocked threads
    check_and_wake_timer_threads();

    // Time slice determination
    if ((ctx->system_ticks - last_slice_tick) >= slice_ticks) {
        last_slice_tick = ctx->system_ticks;
        schedule();
    }
}

// Wake-up time check & wake-up
static void check_and_wake_timer_threads(void) {
    kernel_context_t* ctx = get_kernel_context();
    uint32_t current_ticks = ctx->system_ticks;

    while (ctx->blocked_thread_list &&
           ctx->blocked_thread_list->wake_up_tick <= current_ticks) {

        thread_t* thread_to_wake = ctx->blocked_thread_list;
        ctx->blocked_thread_list = thread_to_wake->next_blocked;

        // Return to READY list
        unblock_and_requeue_thread(thread_to_wake);
    }
}

// Return thread to READY state
static void unblock_and_requeue_thread(thread_t* thread) {
    if (!thread) return;

    thread->state = THREAD_READY;
    thread->block_reason = BLOCK_REASON_NONE;
    thread->next_blocked = NULL;

    // Add to READY list
    add_thread_to_ready_list(thread);
}
```

**Scheduler Implementation**

```c
// Main scheduler
static void schedule(void) {
    if (is_scheduler_locked()) {
        return;
    }

    acquire_scheduler_lock();

    kernel_context_t* ctx = get_kernel_context();

    if (!ctx->ready_thread_list) {
        handle_blocked_thread_scheduling();
        release_scheduler_lock();
        return;
    }

    if (!ctx->current_thread) {
        handle_initial_thread_selection();
        release_scheduler_lock();
        return;
    }

    perform_thread_switch();
    release_scheduler_lock();
}

// Execute thread switching
static void perform_thread_switch(void) {
    kernel_context_t* ctx = get_kernel_context();
    thread_t* current = ctx->current_thread;
    thread_t* next = ctx->ready_thread_list;

    if (current == next) {
        return; // Same thread, no switch needed
    }

    // Rotate READY list
    ctx->ready_thread_list = next->next_ready;
    ctx->current_thread = next;
    next->state = THREAD_RUNNING;

    if (current && current->state == THREAD_RUNNING) {
        current->state = THREAD_READY;
    }

    // Execute context switch
    context_switch(&current->esp, next->esp);
}
```

**Demo Threads**

```c
// Idle thread
static void idle_thread(void) {
    for (;;) {
        asm volatile("hlt");  // Halt CPU
    }
}

// Demo thread A (50ms interval)
static void threadA(void) {
    for (;;) {
        self->counter++;
        vga_move_cursor(0, self->display_row);
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        vga_puts("Fast Thread A: ");
        vga_putnum(self->counter);
        sleep(5);  // Wait 50ms
    }
}

// Demo thread B (100ms interval)
static void threadB(void) {
    for (;;) {
        self->counter++;
        vga_move_cursor(0, self->display_row);
        vga_set_color(VGA_LIGHT_BLUE, VGA_BLACK);
        vga_puts("Med Thread B: ");
        vga_putnum(self->counter);
        sleep(10); // Wait 100ms
    }
}

// Demo thread C (200ms interval)
static void threadC(void) {
    for (;;) {
        self->counter++;
        vga_move_cursor(0, self->display_row);
        vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
        vga_puts("Slow Thread C: ");
        vga_putnum(self->counter);
        sleep(20); // Wait 200ms
    }
}

// Demo thread D (500ms interval)
static void threadD(void) {
    for (;;) {
        self->counter++;
        vga_move_cursor(0, self->display_row);
        vga_set_color(VGA_LIGHT_MAGENTA, VGA_BLACK);
        vga_puts("Very Slow D: ");
        vga_putnum(self->counter);
        sleep(50); // Wait 500ms
    }
}
```

## Implementation Key Points

**sleep(ticks) function flow:**
1.  Remove current thread from READY list.
2.  Set thread state to `THREAD_BLOCKED` and reason to `BLOCK_REASON_TIMER`.
3.  Calculate wake-up time (`wake_up_tick`) and insert to BLOCKED list in time order.
4.  Call `schedule()` to switch to next executable thread.

**Timer interrupt handler (timer_handler_c) role:**
1.  Increment system tick (`system_ticks`).
2.  Check BLOCKED list and return threads whose wake-up time has arrived to READY list.
3.  If time slice has elapsed, call `schedule()` for preemptive switching.

**Scheduler (schedule) operation:**
-   Always selects next thread from READY list.
-   Even when READY list is empty (only `idle_thread` etc.), `idle_thread` is selected so system doesn't stop.

**Important design patterns:**
- **Time-ordered sort**: BLOCKED list sorted by earliest wake-up time
- **Idle thread**: Dedicated thread to efficiently halt CPU
- **Scheduler lock**: Lock mechanism to prevent re-entry during interrupts
- **Generic block mechanism**: Design that can handle future I/O waiting

## Troubleshooting

-   **"No switching after sleep"**
    -   Verify removal from READY list and insertion to BLOCKED list logic is correct. Pay special attention when manipulating list head or tail.
-   **"Timer stops"**
    -   Verify EOI (End of Interrupt) is sent to PIC at beginning of interrupt handler.
-   **"All threads blocked causing hang"**
    -   Verify idle_thread is working correctly and remains in READY list.
-   **"Wake-up time incorrect"**
    -   Verify time-ordered sort of BLOCKED list is working correctly.

## Understanding Check

1. Why is it necessary to sort BLOCKED list in time order?
2. What happens to system if there's no idle_thread?
3. Why is scheduler lock necessary?

## Next Steps

In Day 11, we'll introduce keyboard input and apply this blocking mechanism to I/O waiting as well.