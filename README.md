# Mini OS Learning Curriculum - Master OS Development in 12 Days 🚀

## 📖 Explanation for Software Engineers

**What makes this project special:**

In typical application development, you write programs using APIs provided by the OS (Linux, Windows, macOS). However, in this project, you can **build the OS itself**, gaining fundamental understanding of how computers work.

**Why learn OS development:**

-   **Understanding system operation**: Memory management, process switching, I/O processing mechanisms
-   **Performance optimization**: Deep understanding of bottlenecks and countermeasures
-   **Improved debugging skills**: Low-level knowledge enhances complex problem-solving abilities
-   **Architecture understanding**: Coordination mechanisms of CPU, memory, and I/O devices

## 🖥️ PC Architecture Basics (for Software Engineers)

### What is x86 Processor Architecture

**x86**: A processor family starting from Intel 8086. Foundation of current Intel/AMD processors.

-   **16-bit era**: 8086, 8088 (1978) - MS-DOS era
-   **32-bit era**: 80386 (1985) - Windows 95/Linux emergence
-   **64-bit era**: x86_64 (2003) - Modern PCs/servers

This project targets 32-bit x86.

### CPU Operation Modes

**Real Mode**:

-   16-bit environment, maximum 1MB memory
-   Mode in which BIOS operates
-   Segment:offset addressing format
-   Even modern systems start in this mode after power-on

**Protected Mode**:

-   32-bit environment, maximum 4GB memory
-   Safe execution environment with memory protection
-   Foundation of modern OS
-   Segment management via GDT (Global Descriptor Table)

### Hardware Components

**PIC (Programmable Interrupt Controller) - 8259A**:

-   Transmits interrupt signals from external devices (keyboard, timer, etc.) to CPU
-   16 interrupt lines: IRQ0-7 (master), IRQ8-15 (slave)
-   OS typically remaps from BIOS settings (IRQ0-15) to 32-47

**PIT (Programmable Interval Timer) - 8254**:

-   Generates periodic timer interrupts (necessary for OS scheduling)
-   Divides reference clock 1.193182MHz to generate arbitrary periods
-   Channel 0 used as system timer

**VGA (Video Graphics Array)**:

-   Text mode: 80x25 characters, each character with color attributes
-   Frame buffer: Direct memory access from address 0xB8000
-   Screen control via character+attribute pairs (2 bytes)

**PS/2 Keyboard**:

-   Sends scan codes via serial communication
-   Asynchronous input processing via IRQ1 interrupts
-   Access via ports 0x60(data), 0x64(status)

## 💾 Floppy Disk Structure

This OS is created as a 1.44MB floppy disk image.

### Physical Structure

```
Floppy disk cross-section:
        ┌────────────────────────────┐
        │           Track 0          │ Track 0 (outer)
        │ ┌───────────────────────┐  │
        │ │         Track 1       │  │ Track 1
        │ │  ┌─────────────────┐  │  │
        │ │  │      Track 2    │  │  │ Track 2
        │ │  │      Track 3    │  │  │ Track 3
        │ │  │       ...       │  │  │  ...
        │ │  │      Track 79   │  │  │ Track 79 (inner)
        │ │  │                 │  │  │
        │ │  └─────────────────┘  │  │
        │ └───────────────────────┘  │
        └────────────────────────────┘

One track (actually circular) stretched horizontally:
  ┌──────────┬───┬───┬┬┬┬┬┬┬────┐
  │ Sector 1 | 2 | 3 | ... | 18 |
  └──────────┴───┴───┴┴┴┴┴┴┴────┘

Track = Concentric data track (0-79)
Sector = Fan-shaped section on track (1-18)
```

### Logical Structure

```
1.44MB Floppy Disk Layout:

Total capacity: 1,474,560 bytes (1440KB)
├─ Tracks: 80 (0-79)
├─ Heads: 2 (front/back surfaces)
├─ Sectors per Track: 18 (1-18)
└─ Bytes per Sector: 512

Calculation: 80 tracks × 2 heads × 18 sectors × 512 bytes = 1,474,560 bytes

Sector number calculation:
Physical Sector = (Track × 2 + Head) × 18 + (Sector - 1)

Example: Track 0, Head 0, Sector 1 = 0 (Boot Sector)
Example: Track 0, Head 0, Sector 2 = 1 (Kernel start position)
```

### Boot Sector Layout

```
Sector 0 (First 512 bytes):
┌─────────────────┬───────────────────────────────────────────┐
│ Offset          │ Contents                                  │
├─────────────────├───────────────────────────────────────────┤
│ 0x000 - 0x1FD   │ Boot Code (510 bytes)                     │
│                 │ ├─ A20 Line Enable                        │
│                 │ ├─ GDT (Global Descriptor Table) Setup    │
│                 │ ├─ Load Kernel from Sector 1+             │
│                 │ └─ Switch to Protected Mode               │
├─────────────────├───────────────────────────────────────────┤
│ 0x1FE - 0x1FF   │ Boot Signature (0x55AA)                   │
└─────────────────┴───────────────────────────────────────────┘

Sectors 1+ (Kernel):
┌─────────────────┬───────────────────────────────────────────┐
│ Sectors 1-N     │ Kernel Binary (Variable Size)             │
│                 │ ├─ kernel_entry.s (Assembly entry point)  │
│                 │ ├─ interrupt.s (Interrupt handlers)       │
│                 │ ├─ context_switch.s (Thread switching)    │
│                 │ └─ kernel.c (Main kernel code)            │
├─────────────────├───────────────────────────────────────────┤
│ Sectors N+1-2879│ Unused Space (Padded with zeros)          │
│ (End of disk)   │ Total: 1440KB = 2880 sectors              │
└─────────────────┴───────────────────────────────────────────┘
```

### Memory Layout After Boot

```
Physical Memory Layout (32-bit Protected Mode):

0x00000000 ┌─────────────────────────────────────┐
           │ Interrupt Vector Table (IVT)        │
0x00000400 ├─────────────────────────────────────┤
           │ BIOS Data Area                      │
0x00000500 ├─────────────────────────────────────┤
           │ Free Conventional Memory            │
0x00007C00 ├─────────────────────────────────────┤
           │ Boot Sector (Loaded by BIOS)        │ ← 512 bytes
0x00007E00 ├─────────────────────────────────────┤
           │ Free Memory                         │
0x000A0000 ├─────────────────────────────────────┤
           │ VGA Memory                          │
0x000B8000 │ ├─ Text Mode Buffer                 │ ← VGA text display
0x000C0000 ├─────────────────────────────────────┤
           │ BIOS ROM                            │
0x00100000 ├─────────────────────────────────────┤ ← 1MB boundary
           │ Kernel Code (Loaded here)           │ ← Our OS kernel
0x00200000 ├─────────────────────────────────────┤ ← 2MB
           │ Kernel Stack                        │ ← Stack grows down
0x00300000 ├─────────────────────────────────────┤
           │ Thread Stacks & Data                │
           │ Available RAM...                    │
0xFFFFFFFF └─────────────────────────────────────┘
```

## Overview

This repository is an educational curriculum for learning step-by-step from basic PC boot concepts to a full-featured multithreaded operating system in 12 days. Each day builds upon previous concepts, accumulating knowledge through practical development.

C language knowledge is assumed, while x86 assembly is explained in a beginner-friendly manner. At each stage, you'll learn core computer science concepts experientially while building a working OS.

The final completed version is in [day99_completed](day99_completed).

## Prerequisites & Environment

### Required Prerequisites

**C Language (intermediate level)**:

-   Understanding of pointers, structures, arrays
-   Concepts of function pointers and callbacks
-   Understanding of memory layout (stack, heap)
-   Bit operations and hexadecimal notation

**Computer Science Fundamentals (recommended)**:

-   Data structures (lists, queues, stacks)
-   Basic algorithm concepts
-   Reading and writing hexadecimal numbers
-   Understanding of binary format

**x86 Assembly (learn as you go)**:

-   Explained step-by-step in this curriculum
-   No prior understanding needed, start with copy & paste

### Development Environment Setup

**For macOS:**

```bash
# Install tools with Homebrew
brew install i686-elf-gcc nasm qemu clang-format
```

**For Linux (Ubuntu/Debian):**

```bash
# Cross-compiler and assembler
sudo apt-get update
sudo apt-get install build-essential nasm qemu-system-i386 clang-format

# download i686-elf-tools-linux.zip
# from https://github.com/lordmilko/i686-elf-tools/releases/
wget https://github.com/lordmilko/i686-elf-tools/releases/download/13.2.0/i686-elf-tools-linux.zip
# extract it in /usr/local
cd /usr/local
sudo unzip ~/Downloads/i686-elf-tools-linux.zip
```

**For Windows:**

1. **WSL2 (recommended)**: Run Ubuntu on Windows and follow Linux instructions above
2. **MSYS2**: Native Windows development environment
3. **VMware/VirtualBox**: Use Linux virtual machine

**Required tools:**

-   `i686-elf-gcc`: 32-bit x86 cross-compiler
-   `nasm`: x86 assembler
-   `qemu-system-i386`: x86 emulator
-   `make`: Build automation tool

## 🚨 Pre-learning Preparation Check

### Required Prerequisite Knowledge Level

-   **C Language**: Intermediate (can handle pointers, structures, arrays)
-   **Assembly**: Beginner (knows concepts of registers, memory, basic instructions)
-   **Linux/Unix**: Basic operations (can use make, compile, terminal operations)

### Common Learning Stumbling Points

1. **Day 01-02**: Boot process is complex → **Solution**: Start with Day03, return to Day01 when comfortable
2. **Day 04-05**: Inline assembly is cryptic → **Solution**: Run sample code first, then deepen understanding
3. **Day 08-09**: Context switching is hard to grasp → **Solution**: Observe actual register changes with debugger
4. **Day 10-11**: Scheduler is complex → **Solution**: Start with simple 2 threads, expand gradually

### Troubleshooting

-   **Build failures**: Re-check development environment setup steps
-   **QEMU won't start**: Test with day99_completed directory, then compare with your code
-   **Difficult to understand**: Use "Understanding Check" in each day's README.md

## 🎯 Learning Roadmap

### Phase 1: Foundations (Day 01-04)

**Goal**: Bootloader and C kernel basics

| Day              | Theme                     | Main Learning Content                         | Output                    |
| ---------------- | ------------------------- | --------------------------------------------- | ------------------------- |
| [Day 01](day01/) | Bootloader basics         | BIOS, MBR, 16-bit x86 assembly               | "Hello World" bootloader  |
| [Day 02](day02/) | Protected mode transition | A20 line, GDT, 32-bit switching              | VGA text display          |
| [Day 03](day03/) | C language integration    | freestanding C, VGA API                       | C kernel foundation       |
| [Day 04](day04/) | Serial debugging          | UART, I/O ports, inline assembly             | Debug environment setup   |

### Phase 2: Systems (Day 05-08)

**Goal**: Interrupt system and multithreading foundations

| Day              | Theme                | Main Learning Content         | Output                   |
| ---------------- | -------------------- | ----------------------------- | ------------------------ |
| [Day 05](day05/) | Interrupt infrastructure | IDT, exception handling, ISR stubs | Exception handler system |
| [Day 06](day06/) | Timer interrupts     | PIC, PIT, IRQ0 handling       | 100Hz timer operation    |
| [Day 07](day07/) | Thread data structures | TCB, READY list, state management | Multithreading foundation |
| [Day 08](day08/) | Context switching    | Register save/restore, ESP switching | Thread switching functionality |

### Phase 3: Applications (Day 09-12)

**Goal**: Complete practical OS

| Day              | Theme                              | Main Learning Content          | Output               |
| ---------------- | ---------------------------------- | ------------------------------ | -------------------- |
| [Day 09](day09/) | Preemptive scheduling              | Round-robin, time slicing      | Automatic thread switching |
| [Day 10](day10/) | Sleep/timing                       | Blocking, wake-up              | sleep() function implementation |
| [Day 11](day11/) | Keyboard input system              | PS/2, ring buffer, IRQ1        | User input processing |
| [Day 12](day12/) | Integration and final project      | Quality improvement, testing, documentation | Complete OS |

## 🚀 Quick Start

### 1. Clone repository and check environment

```bash
# Clone repository
git clone <repository-url>
cd mini-os

# Check development tools
make check-env
```

### 2. Start from Day 01

```bash
cd day01
make clean
make all
make run  # Launch OS in QEMU
```

### 3. How to proceed through each Day

1. **Read README.md**: Understand learning goals and theory for that day
2. **Implement**: Create code step by step
3. **Test**: Verify operation with `make run`
4. **Understand**: Consider why that code is necessary
5. **Move on**: Compare with completed version and proceed to next Day

## 📁 Directory Structure

```
mini-os/
├── README.md                    # This file
├── day01/                       # Day 01: Bootloader basics
│   ├── README.md
│   ├── boot.s
│   └── Makefile
├── day01_completed/             # Day 01 completed version (reference)
├── day02/                       # Day 02: Protected mode
├── day02_completed/
...
├── day12/                       # Day 12: Final integration
├── day12_completed/
└── day99_completed/             # Final completed version (with extensions)
    ├── src/
    │   ├── kernel/
    │   ├── drivers/
    │   ├── boot/
    │   └── include/
    ├── tests/
    └── docs/
```

## 🎓 Learning Tips

### Effective Learning Methods

**1. Gradual understanding**:

-   Don't try to understand everything at once
-   Verify it works before learning details
-   Note questions and research later

**2. Hands-on practice**:

-   Copy & paste is OK initially
-   Verify operation before understanding meaning
-   Make small changes and experiment

**3. Debugging skills**:

-   Utilize serial output
-   Learn QEMU monitor commands
-   Get comfortable with system hangs (normal phenomenon)

**4. Review and application**:

-   Review code from previous Days
-   Compare with other architectures
-   Consider differences from modern OS

### Common Stumbling Points

**Assembly language**:

-   Memorization is OK initially, understand gradually
-   Distinguish register names and sizes (AX, EAX, etc.)
-   Addressing modes (direct, indirect, etc.)

**Memory layout**:

-   Physical vs logical addresses
-   Segmentation concepts
-   Stack growth direction (downward)

**Interrupt handling**:

-   Difference between synchronous (exceptions) and asynchronous (IRQ)
-   Need for interrupt disabling
-   EOI (End of Interrupt) transmission timing

## 🔧 Troubleshooting

### Common Problems and Solutions

**1. Build errors**:

```bash
# Tools not found
brew install i686-elf-gcc nasm qemu  # macOS
sudo apt-get install build-essential nasm qemu-system-i386  # Linux

# Path not set
export PATH="/usr/local/bin:$PATH"
```

**2. QEMU won't start**:

```bash
# Check QEMU binary
which qemu-system-i386

# Check permissions
ls -la os.img
```

**3. Black screen**:

-   Check boot signature (0x55AA)
-   Jump instruction destination address
-   GDT settings and segment selectors

**4. System hang**:

-   Check intentional infinite loop placement
-   Interrupt setup order
-   Stack pointer initialization

### Debugging Techniques

**Using serial output**:

```bash
# Save serial output to file
make run > debug.log 2>&1

# Check log in real-time
make run | tee debug.log
```

**QEMU monitor**:

```bash
# Launch in debug mode
make debug

# Monitor commands (QEMU console)
(qemu) info registers  # Register status
(qemu) info mem        # Memory map
(qemu) x/10i $eip      # Disassemble current instruction
```

## 🤝 Contributions and Feedback

### Participating in Improvements

This project aims to optimize for educational purposes:

**Welcome contributions**:

-   Corrections for typos and technical errors
-   Proposals for clearer explanations
-   Additional debug information and troubleshooting

**Pull request guidelines**:

1. Clearly explain changes
2. Maintain existing learning sequence
3. Prioritize clarity for beginners
4. Specify that changes have been tested

## 💪 Tips for Maintaining Learning Motivation

### Value gradual achievements

-   **Day 01-02**: "Hello World" output to screen → Feel of controlling hardware
-   **Day 03-04**: Build kernel with C → Complete foundation for serious OS development
-   **Day 05-06**: Heartbeat display with interrupts → Sense of system being "alive"
-   **Day 07-08**: Successful thread switching → Learn basic principles of multitasking
-   **Day 09-10**: Preemptive scheduling → Understand same mechanisms as modern OS
-   **Day 11-12**: Keyboard input support → First step toward practical OS

### When feeling like giving up

1. **Run day99_completed**: See the completed form and regain motivation
2. **Step back**: Return to previous Day to reconfirm basics if current Day is difficult
3. **Community**: Share learning progress with peers in similar situations
4. **Career perspective**: Imagine how this knowledge can be applied to work

### Learning Time Estimates

-   **Beginners**: 2-4 hours per Day, 40-60 hours total
-   **Intermediate**: 1-2 hours per Day, 15-30 hours total
-   **Advanced**: 0.5-1 hour per Day, 8-15 hours total

### Creating Environment for Continuity

-   **Regular time allocation**: Concentrate on weekends or 30 minutes daily on weekdays
-   **Visualize results**: Record videos or take screenshots of each Day's operation
-   **Learning log**: Note what was understood and stumbling points
-   **SNS sharing**: Share learning progress on Twitter etc. to boost motivation

## 🎉 Next Steps After Completion

### Further Learning

**Content you can challenge after completing this project**:

**Intermediate level**:

-   **Shell creation**: Create command-line shell
-   **64-bit migration**: Extend to x86_64 architecture
-   **Memory management**: Paging, virtual memory, MMU
-   **File system**: FAT12/16, simple file operations
-   **Networking**: Basic TCP/IP implementation

**Advanced level**:

-   **Multi-core support**: SMP, inter-CPU synchronization
-   **Device drivers**: Support for more hardware
-   **Userland**: System calls, process isolation
-   **GUI**: Basic window system

**Other architectures**:

-   **ARM**: OS development for Raspberry Pi
-   **RISC-V**: New open architecture
-   **Embedded**: Real-time OS for microcontrollers

### Career Applications

**Fields where these skills are useful**:

-   **System software**: OS, device drivers, firmware
-   **Embedded systems**: IoT, automotive, industrial equipment
-   **High-performance computing**: HPC, distributed systems, databases
-   **Security**: Malware analysis, system auditing, vulnerability research
-   **Game development**: Engine optimization, low-level performance tuning

**Interview appeal points**:

-   Optimization abilities based on hardware understanding
-   Practical experience with low-level debugging
-   Design capability to oversee entire systems
-   Persistent approach to difficult problems

---

## 📚 Glossary

### Early stages (Day 01-03)

-   **Boot Sector / MBR**: 512-byte data area that BIOS loads first. This size is determined by hardware constraints, and BIOS loads this into memory at address 0x7C00 and begins execution. Important entry point for OS development.
-   **org 0x7c00**: Assembler directive declaring that program is placed at memory address 0x7C00. For historical reasons, this address was chosen to maintain compatibility with CP/M-86. Based on real mode memory mapping.
-   **Protected Mode**: Opposite of real mode; 32-bit/64-bit protected execution environment. Provides memory protection, virtual memory, and privilege level concepts. While real mode had 1MB memory limit, protected mode enables 4GB+ memory usage and is essential for modern OS.
-   **VGA Text Buffer / 0xB8000**: Mechanism for screen display by writing directly to video card memory area (memory-mapped I/O). Writing character codes and attributes in 2-byte units to address 0xB8000 automatically displays on screen. Important concept bridging hardware and software.
-   **Linker Script (.ld)**: Instructions for arranging compiled object files into final executable. Controls which memory addresses to place object file sections (.text, .data, etc.). Important for OS development in freestanding environment as memory layout must be precisely defined.

### Middle stages (Day 04-08)

-   **Interrupt**: "Event notification" mechanism for CPU. Responds to asynchronous requests from hardware or software, temporarily suspending current processing to execute handler. Core concept for OS responsiveness.
-   **IRQ (Interrupt ReQuest)**: Physical lines through which interrupt request signals pass (interrupt lines). PIC manages multiple IRQs and transmits to CPU. IRQ 0-15 are standardly assigned, corresponding to devices like keyboard, timer, etc.
-   **PIC (Programmable Interrupt Controller)**: 8259A chip that bundles IRQs from multiple devices and efficiently transmits to CPU. In BIOS settings, remaps IRQ0-15 to 32-47 for use, enabling OS interrupt management.
-   **PIT (Programmable Interval Timer)**: 8254 chip that generates periodic interrupts as timer device. Divides reference clock 1.193182MHz to generate arbitrary periods. Channel 0 is used as system timer, forming foundation for scheduling.
-   **I/O Port (inb, outb)**: Mechanism for communicating with hardware using address space (I/O space) separate from memory space. inb instruction reads data from port, outb writes. Basic method for hardware control.
-   **Context Switch**: Core concept of multitasking. Process of "saving execution state (registers, stack pointer, etc.) of one thread and restoring state of another thread". Efficiently managed using TCB.
-   **TCB (Thread Control Block)**: Data structure for storing thread state. Contains information like register values, stack pointer, execution state, priority. One assigned per thread, manipulated by scheduler.

### Final stages / Overall

-   **Privilege Level (Ring 0)**: Part of CPU protection functionality; highest privilege mode where OS kernel operates. Ring 0 allows all hardware access, while Ring 3 (user mode) has restrictions. Foundation of OS security.
-   **Ring Buffer (SPSC)**: Abbreviation for Single Producer Single Consumer; "simple circular buffer with one writer and one reader". Can safely exchange data without collisions. Suitable for asynchronous communication like keyboard input, implemented in technical detail in day99_completed.
-   **Toolchain (i686-elf-gcc, nasm)**: Cross-compilation environment. While normal gcc generates code for host OS (macOS, etc.), i686-elf-gcc is a "cross-compiler" that generates code for target OS (custom OS). nasm is assembler; both essential for OS development.

---

**🎯 With this curriculum, you too will join the ranks of OS developers!**

We welcome questions, feedback, and improvement suggestions. Let's build better learning resources together!