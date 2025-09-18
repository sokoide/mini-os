# Day 08: Context Switch 🔄

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
|   Thread Data    |
+------------------+ <- ESP (before context switch)

After pushfd/pusha:
+------------------+
|   Thread Data    |
+------------------+
|     EFLAGS       | <- pushfd
+------------------+
|      EAX         | <- pusha
|      ECX         |
|      EDX         |
|      EBX         |
|   ESP (dummy)    |
|      EBP         |
|      ESI         |
|      EDI         |
+------------------+ <- ESP (saved to TCB)

Switch to ThreadB:
ThreadB ESP restored from TCB → popa/popfd → ThreadB execution
```

This establishes the foundation for multithreading with proper register preservation and stack management.