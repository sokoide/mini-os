# Day 09: Preemptive Scheduling ⚡

## Today's Goal

Implement preemptive multitasking by integrating timer interrupts with context switching for automatic thread switching.

## Background

Day 8 achieved manual context switching, but modern OS requires automatic thread switching via timer interrupts. Today we'll combine PIT timer interrupts with the scheduler to implement preemptive multitasking.

## New Concepts

-   **Preemptive Scheduling**: OS automatically switches threads using timer interrupts without waiting for threads to voluntarily yield control.
-   **Round-robin**: Simple scheduling algorithm that gives each thread equal time slices in rotation.
-   **Time Slice**: Fixed time period (e.g., 10ms) each thread gets to execute before being preempted.

## Learning Content

-   Integration of timer interrupt handler with scheduler
-   Round-robin scheduling algorithm implementation
-   Time slice management and thread rotation
-   Preemptive vs cooperative multitasking concepts

## Task List

-   [ ] Modify timer interrupt handler to call scheduler
-   [ ] Implement round-robin scheduler function
-   [ ] Add time slice tracking to TCB
-   [ ] Create multiple demo threads with different behaviors
-   [ ] Test preemptive switching with concurrent thread execution
-   [ ] Verify round-robin rotation in QEMU

## Implementation Highlights

- Timer IRQ0 handler calls `schedule()` function
- Scheduler rotates through READY list using round-robin
- Each thread gets equal time slice before preemption
- Threads execute concurrently with visible switching behavior

This completes the foundation for a preemptive multitasking operating system.