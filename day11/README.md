# Day 11: Keyboard Input System ⌨️

## Today's Goal

Implement PS/2 keyboard driver with interrupt-driven input processing and provide scanf-equivalent functionality.

## Background

Day 10 completed timer-based thread management, but user interaction requires keyboard input. Today we'll implement a complete keyboard input system with PS/2 controller, scan code translation, and input buffer management.

## New Concepts

-   **PS/2 Keyboard**: Standard keyboard interface using serial communication with scan codes
-   **Scan Codes**: Hardware-specific codes sent by keyboard for key press/release events
-   **Input Buffer**: Circular buffer for storing keystrokes in interrupt-driven system
-   **IRQ1**: Keyboard interrupt line for asynchronous input processing

## Learning Content

-   PS/2 controller programming (ports 0x60, 0x64)
-   Scan code to ASCII conversion tables
-   Circular buffer implementation for keystroke storage
-   IRQ1 interrupt handler integration
-   Blocking input functions (getchar, read_line)

## Task List

-   [ ] Initialize PS/2 keyboard controller
-   [ ] Implement IRQ1 keyboard interrupt handler
-   [ ] Create scan code to ASCII conversion tables
-   [ ] Implement circular input buffer
-   [ ] Add keyboard IRQ to interrupt system
-   [ ] Implement getchar() and read_line() functions
-   [ ] Test keyboard input with interactive demo

## Implementation Highlights

- Real-time keyboard input via IRQ1 interrupts
- Thread-safe circular buffer for keystroke storage
- Support for printable characters and special keys
- `getchar()` - single character input (scanf("%c") equivalent)
- `read_line()` - string input with backspace support (scanf("%s") equivalent)
- Integration with existing thread scheduling system

This provides essential user interaction capabilities to the operating system.