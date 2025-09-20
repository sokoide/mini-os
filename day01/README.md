# Day 01: PC Boot Fundamentals 🖥️

---

🌐 Available languages:

[English](./README.md) | [日本語](./README_ja.md)

## Today's Goal

Understand the x86 computer boot process and create a minimal bootloader to display "Hello OS!".

## Background

The first step in OS development is understanding the computer boot process. From power-on, BIOS initializes hardware and loads and executes the first program (bootloader) from the boot device. Today we'll learn these fundamentals and create an actual working bootloader.

## New Concepts

-   **Boot Sector**: 512-byte data area that BIOS loads first. The 512-byte size is due to hardware constraints, and BIOS loads this into memory at address 0x7C00 and begins execution.
-   **org 0x7c00**: Assembler directive declaring that the program is placed at memory address 0x7C00. This address was chosen for historical reasons to maintain compatibility with CP/M-86.
-   **Real Mode**: 16-bit execution environment. Even modern 64-bit CPUs start in this mode after power-on.

## Learning Content

-   **x86 PC boot process** - How computers operate after power-on
-   **BIOS (Basic Input/Output System)** - Bridge between hardware and OS
-   **MBR (Master Boot Record)** - First program to execute
-   **16-bit real mode** - Historical x86 execution mode
-   **Assembly language basics** - Language for direct CPU communication

## Task List

-   [ ] Create boot.s file and write assembly code
-   [ ] Implement function to display characters on screen using BIOS interrupts
-   [ ] Add 512-byte boot signature (0xAA55)
-   [ ] Create Makefile to build environment
-   [ ] Execute bootloader in QEMU and confirm "Hello OS!" is displayed

## Prerequisites Check

### Required Knowledge

-   **C language basics**: Concepts of functions, variables, pointers
-   **Hexadecimal**: Familiarity with notation like 0x7C00, 0x10
-   **Basic computer concepts**: Awareness of CPU, memory, registers

### What's New Today

-   **x86 registers**: High-speed storage inside CPU
-   **Segment registers**: Old-style memory management
-   **BIOS interrupts**: Hardware control without OS
-   **Assembly language**: Direct instructions to CPU

## Understanding x86 PC Boot Process

### 1. From Power-On to BIOS

```
Power-On → CPU Initialization → BIOS Startup → Hardware Detection → Bootloader Search
```

1. **CPU Initialization**: When power is applied, CPU begins execution from predetermined address (0xFFFFFFF0)
2. **BIOS Execution**: BIOS program in ROM checks hardware
3. **Boot Device Search**: Searches for bootable devices in order: floppy disk, hard disk
4. **MBR Load**: Loads first 512 bytes of found device to address 0x7C00
5. **Control Transfer**: Begins execution from 0x7C00 (this is our program!)

### 2. Real Mode and Registers

**Real Mode** is 16-bit execution mode compatible with 8086 CPU. Even modern 64-bit CPUs always start in this mode after power-on.

#### Main Registers (CPU Internal Storage)

| Register | Role                              | Example                |
| -------- | --------------------------------- | ---------------------- |
| **AX**   | Accumulator (for calculations)    | Temporary save of calculation results |
| **BX**   | Base register (address calculation) | Memory address calculation |
| **CX**   | Counter register (for loops)      | Managing repeat counts |
| **DX**   | Data register (for I/O)          | Storing input/output data |

#### Segment Registers (Memory Area Specification)

| Register | Role                | Description                    |
| -------- | ------------------- | ------------------------------ |
| **DS**   | Data segment        | Specifying data storage area   |
| **ES**   | Extra segment       | Additional data area           |
| **SS**   | Stack segment       | Save area for function calls  |
| **CS**   | Code segment        | Area of executing program      |

## Hands-on: Creating First Bootloader

### Step 1: Development Environment Check

First, let's verify that necessary tools are installed:

```bash
# Check cross-compiler
i686-elf-gcc --version

# Check assembler
nasm --version

# Check emulator
qemu-system-i386 --version
```

If not installed:

```bash
# For macOS
brew install i686-elf-gcc nasm qemu

# For Ubuntu
sudo apt install gcc-multilib nasm qemu-system-i386
```

### Step 2: Understanding Project Structure

File structure we'll create today:

```
day01/
├── README.md          # This file
├── boot.s            # Bootloader assembly code
├── Makefile          # Build automation script
└── os.img           # Created bootable image (after execution)
```

### Step 3: Creating Bootloader Assembly Code

Create `boot.s` file and enter the following content:

```assembly
; boot.s - First bootloader
; Runs in x86 real mode (16-bit)

[org 0x7C00]          ; Specify that BIOS loads our code at address 0x7C00
[bits 16]             ; Assemble in 16-bit mode

start:
    ; === Register Initialization ===
    ; Set all segment registers to 0 (for safety)
    cli                ; Disable interrupts (don't want to be disturbed during important initialization)
    xor ax, ax         ; AX = 0 (XOR of same register results in 0)
    mov ds, ax         ; Data segment = 0
    mov es, ax         ; Extra segment = 0
    mov ss, ax         ; Stack segment = 0
    mov sp, 0x7C00     ; Set stack pointer just before bootloader

    ; === Display Message ===
    ; Display "Hello OS!" on screen
    mov si, hello_msg  ; Set message address in SI register
    call print_string  ; Call string display function

    ; === Infinite Loop ===
    ; Infinite loop to prevent program termination
infinite_loop:
    hlt               ; Put CPU to sleep (power saving)
    jmp infinite_loop ; Infinite loop

; === String Display Function ===
; Input: SI = string address
; Destroys: AX, BX
print_string:
    mov ah, 0x0E      ; BIOS function: character display (Teletype output)
.next_char:
    lodsb             ; Load 1 byte from memory pointed by SI into AL, SI++
    cmp al, 0         ; Check if character is 0 (string terminator)
    je .done          ; If 0, exit
    int 0x10          ; Call BIOS interrupt (character display)
    jmp .next_char    ; Go to next character
.done:
    ret               ; Return to caller

; === Data Section ===
hello_msg db 'Hello OS! Boot successful!', 13, 10, 0
    ; db = define byte
    ; 13 = Carriage Return (CR)
    ; 10 = Line Feed (LF)
    ; 0 = String terminator

; === Boot Signature ===
; Fill remainder with 0 to make exactly 512 bytes
times 510 - ($ - $$) db 0  ; Fill from current position to 510 bytes with 0
dw 0xAA55                  ; Boot signature (without this BIOS won't recognize it)
```

### Step 4: Creating Makefile

Create `Makefile` to automate the build process:

```makefile
# Makefile for Day 01 - Basic Boot
# Simple bootloader build script

# === Tool Settings ===
AS = nasm              # Assembler
QEMU = qemu-system-i386 # Emulator

# === File Name Settings ===
BOOT_SRC = boot.s      # Source file
OS_IMG = os.img        # Boot image

# === Main Target ===
# Executed with 'make' or 'make all'
all: $(OS_IMG)
	@echo "✅ Bootloader creation completed!"
	@echo "📁 $(OS_IMG) has been created"
	@echo ""
	@echo "🚀 To run: make run"
	@echo "🧹 To clean up: make clean"

# === Image File Creation ===
$(OS_IMG): $(BOOT_SRC)
	@echo "🔨 Assembling bootloader..."
	$(AS) -f bin $(BOOT_SRC) -o $(OS_IMG)
	@echo "📏 Checking file size..."
	@ls -l $(OS_IMG) | awk '{print "   Size: " $$5 " bytes"}'
	@if [ `wc -c < $(OS_IMG)` -eq 512 ]; then \
		echo "✅ Correctly 512 bytes!"; \
	else \
		echo "❌ Error: Not 512 bytes"; \
		exit 1; \
	fi

# === QEMU Execution ===
run: $(OS_IMG)
	@echo "🚀 Starting OS in QEMU..."
	@echo "💡 To exit, close QEMU window or press Ctrl+C"
	@echo ""
	$(QEMU) -fda $(OS_IMG) -boot a


# === Debug Execution ===
# Execute with serial output (useful for debugging, used from Day4 onwards)
debug: $(OS_IMG)
	@echo "🔍 Starting in debug mode..."
	$(QEMU) -fda $(OS_IMG) -boot a -serial stdio -monitor tcp:127.0.0.1:4444,server,nowait

# === Cleanup ===
clean:
	@echo "🧹 Deleting generated files..."
	rm -f $(OS_IMG)
	@echo "✅ Cleanup complete"

# === Help ===
help:
	@echo "=== Day 01 Bootloader Development ==="
	@echo ""
	@echo "Available commands:"
	@echo "  make all     - Build bootloader"
	@echo "  make run     - Execute in QEMU"
	@echo "  make debug   - Execute in debug mode"
	@echo "  make clean   - Delete generated files"
	@echo "  make help    - Display this help"
	@echo ""
	@echo "📚 Learning points:"
	@echo "  - 512-byte boot sector"
	@echo "  - Character display via BIOS interrupts"
	@echo "  - x86 real mode basics"

# === Phony Targets ===
# Declare to avoid conflicts with file names
.PHONY: all run debug clean help
```

### Step 5: Build and Execute

#### 1. Execute Build

```bash
cd day01
make all
```

If successful, output like this will be displayed:

```
🔨 Assembling bootloader...
📏 Checking file size...
   Size: 512 bytes
✅ Correctly 512 bytes!
✅ Bootloader creation completed!
```

#### 2. Execute

```bash
make run
```

If QEMU window opens and displays "Hello OS! Boot successful!", it's successful!

## Code Explanation: Detailed Line-by-Line Understanding

### Understanding Assembly Instructions

#### Basic Instructions

| Instruction | Meaning                      | Example                                             |
| ----------- | ---------------------------- | --------------------------------------------------- |
| `mov`       | Move data                    | `mov ax, 0` → Assign 0 to AX                       |
| `xor`       | Exclusive OR                 | `xor ax, ax` → AX = 0 (fast way to assign 0)       |
| `cmp`       | Compare                      | `cmp al, 0` → Compare AL with 0                     |
| `je`        | Jump if equal                | `je .done` → Jump to .done if comparison is equal  |
| `jmp`       | Unconditional jump           | `jmp infinite_loop` → Infinite loop                 |
| `int`       | Interrupt call               | `int 0x10` → BIOS screen display function          |
| `hlt`       | Halt CPU                     | Wait until next interrupt (power saving)           |

#### BIOS Interrupt 0x10 Details

```assembly
mov ah, 0x0E    ; Function number: Teletype Output
mov al, 'A'     ; Character to display
int 0x10        ; Execute → 'A' is displayed on screen
```

-   **AH = 0x0E**: Specify character display function
-   **AL**: ASCII code of character to display
-   **INT 0x10**: Call BIOS screen service

### Understanding Memory Layout

#### Real Mode (16-bit) Memory Map

| Address      | Size   | Purpose                                                             | Importance |
| ------------ | ------ | ------------------------------------------------------------------- | ---------- |
| `0x00000000` | 1KB    | Interrupt Vector Table (IVT)<br>- BIOS interrupt function addresses | ⭐⭐⭐     |
| `0x00000400` | 254B   | BIOS Data Area<br>- Hardware information                           | ⭐⭐       |
| `0x00000500` | ~30KB  | Free area (DOS era remnant)<br>- Can be used freely               | ⭐         |
| `0x00007C00` | 512B   | Boot sector (our code)<br>- BIOS places bootloader here          | ⭐⭐⭐⭐⭐ |
| `0x00007E00` | ~608KB | Free area<br>- Kernel load destination<br>- Can use as stack area | ⭐⭐       |
| `0x000A0000` | 128KB  | Video RAM<br>- VGA text: `0xB8000`~<br>- Graphics: `0xA0000`~     | ⭐⭐⭐⭐   |
| `0x000C0000` | 256KB  | BIOS extension ROM<br>- For adapter cards                         | ⭐         |
| `0x000F0000` | 64KB   | System ROM (BIOS main)<br>- First to execute at startup           | ⭐⭐       |

#### Important Address Details

**0x7C00 (Bootloader)**

-   Historical reason: For CP/M-86 compatibility
-   Must be exactly 512 bytes: BIOS specification
-   Last 2 bytes must be 0x55AA (boot signature)

**0xB8000 (VGA Text Buffer)**

-   80×25 characters = 2000 characters
-   Each character 2 bytes (character code + attribute)
-   Total size: 4000 bytes

#### Memory Layout Example

```
Actual layout example (Day 01 execution):

0x7C00  |48 65 6C 6C| "Hello OS!" message
0x7C04  |6F 20 4F 53|
...     |    ...     |
0x7DFE  |55 AA      | Boot signature
0x7E00  |00 00 00 00| Free area start
```

### Importance of Boot Signature

```assembly
times 510 - ($ - $$) db 0  ; Fill remaining area with 0
dw 0xAA55                  ; Magic number
```

-   `$`: Current position
-   `$$`: Section start position
-   `510 - ($ - $$)`: Calculate remaining bytes
-   `0xAA55`: Magic number for BIOS to recognize "this is a bootable sector"

## Troubleshooting

### Common Errors and Solutions

#### 1. "nasm: command not found"

```bash
# For macOS
brew install nasm

# For Ubuntu
sudo apt install nasm
```

#### 2. "qemu-system-i386: command not found"

```bash
# For macOS
brew install qemu

# For Ubuntu
sudo apt install qemu-system-i386
```

#### 3. "❌ Error: Not 512 bytes"

-   Boot sector must be exactly 512 bytes
-   If message is too long, shorten it
-   Check calculation in `times 510 - ($ - $$) db 0`

#### 4. Nothing displayed on screen

-   Check BIOS interrupt setting: `mov ah, 0x0E`
-   Confirm string ends with 0
-   Check segment register initialization

#### 5. QEMU won't start

```bash
# Display detailed error information
qemu-system-i386 -drive file=os.img,format=raw,if=floppy -boot a -serial stdio
```

## Understanding Check

Verify you can answer the following questions:

### Basic Understanding

1. **What is the role of BIOS?**
2. **What is real mode?**
3. **What is the meaning of address 0x7C00?**
4. **Why must it be exactly 512 bytes?**

### Technical Details

5. **Can you explain the roles of AX, BX, CX, DX registers?**
6. **What does INT 0x10 function 0x0E do?**
7. **What is the meaning of 0xAA55?**
8. **What does `times 510 - ($ - $$) db 0` do?**

### Application Problems

9. **Try changing the message**
10. **Try displaying characters in a different color (hint: BL register)**

## Preparing for Next Steps

Tomorrow's Day 02 will transition from this 16-bit real mode to 32-bit protected mode. Make sure you understand today's content well:

-   ✅ x86 boot process
-   ✅ Real mode and registers
-   ✅ BIOS interrupt basics
-   ✅ Assembly language fundamentals
-   ✅ Memory layout

### Recommended Review Items

1. **Read assembly instructions aloud** (mov, xor, cmp, etc.)
2. **Write down and organize register roles on paper**
3. **Understand why 0x7C00 is special**
4. **Diagram the boot sequence flow**

### Experimental Modification Ideas

Additional challenges for motivated learners:

1. **Countdown display**: Show 3, 2, 1... then message
2. **Colorful display**: Multi-color character display
3. **Wait for key input**: Add function to wait for keyboard input
4. **Time display**: Get and display BIOS time

---

🎉 **Great job!**

Did your first bootloader work? You now understand the fundamentals of the computer's "heart" - the boot process. Tomorrow we'll advance further and step into the world of modern 32-bit protected mode!