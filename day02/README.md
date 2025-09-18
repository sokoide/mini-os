# Day 02: Protected Mode Transition ⚡

## Today's Goal

Safely transition from 16-bit real mode to 32-bit protected mode and display strings directly to the VGA text buffer.

## Background

On Day 1, we displayed "Hello OS!" in real mode, but real mode has memory limitations (1MB) that are insufficient for modern OS development. Today we'll transition to protected mode to gain 4GB memory space and memory protection features, establishing the foundation for serious OS development.

## New Concepts

-   **Protected Mode**: Opposite of real mode; 32-bit protected execution environment. Provides memory protection and 4GB memory space, essential for modern OS. "What" is protected "from what": Protects memory between programs and prevents invalid access.
-   **A20 Line**: Address line to fix address wraparound caused by 8086 design bug. Why activation is necessary: To correctly access memory beyond 1MB.
-   **GDT (Global Descriptor Table)**: Table defining memory segments in protected mode. Manages access permissions and sizes.
-   **CR0.PE**: PE bit in CR0 register that switches to enable protected mode.
-   **VGA Text Buffer / 0xB8000**: Screen display by writing directly to video memory. Why this address: Memory-mapped I/O method where hardware maps this address to screen.

### Learning Content

-   A20 line activation (using memory space beyond 1MB)
-   GDT (Global Descriptor Table) construction
-   Setting CR0.PE to switch to protected mode
-   Far jump to update CS and transition to 32-bit code
-   Direct output to VGA text buffer (0xB8000)

## Task List

-   [ ] Enable A20 line to make memory space beyond 1MB available
-   [ ] Build GDT (null/code/data segments) to create memory management foundation
-   [ ] Set CR0.PE=1 to transition to protected mode
-   [ ] Use far jump to update CS and transition to 32-bit code
-   [ ] Set segment registers and stack on 32-bit side
-   [ ] Write strings directly to VGA buffer 0xB8000 for screen display
-   [ ] Verify operation in QEMU

## Prerequisites Check

### Required Knowledge

-   Day 01 content (real mode, BIOS, 0x7C00)
-   Hexadecimal basics and CPU register fundamentals

### What's New Today

-   Role of A20 line (extension from 8086-compatible 20-bit addressing)
-   GDT structure (descriptors, access bytes, granularity)
-   CR0 register and processor mode switching
-   Segment setup and stack initialization in 32-bit code

## Why Do We Need to Transition to Protected Mode?

**Real Mode** is the operating mode of the Intel 8086 processor from 1978. Even today, systems start in this mode after power-on for compatibility:

**Real Mode Limitations:**

-   **16-bit environment**: CPU can only process in 16-bit units (slow compared to modern 32/64-bit)
-   **1MB memory limit**: Address space limited to 1MB (0x00000~0xFFFFF)
-   **Segment:offset**: Complex address calculation using `segment×16+offset`
-   **No memory protection**: Programs can accidentally overwrite other programs' memory

**Protected Mode Advantages:**

-   **32-bit processing**: Unleashes CPU's true performance (even 64-bit CPUs run fast in 32-bit mode)
-   **4GB memory space**: Sufficient memory for modern applications (32-bit=2³²=4,294,967,296 bytes)
-   **Memory protection**: Safe program separation via GDT (described later)
-   **Modern OS foundation**: Core technology for multitasking OS like Linux, Windows

## What is A20 Line and Why Must It Be Enabled?

**A20 Line** is a concept born from historical compatibility issues in computers.

**Historical Background:**

-   **8086 CPU (1978)**: 20 address lines (A0-A19) addressing 1MB (2²⁰=1,048,576 bytes)
-   **16-bit register limitations**: Segment:offset could address maximum 1,048,575 bytes + 15 bytes = 1,048,590 bytes
-   **Wraparound problem**: Addresses beyond 1MB boundary returned to address 0 (0x100000 → 0x00000)

**Problems from 80286 onwards:**

-   **24 address lines**: A20-A23 added, enabling addressing up to 16MB
-   **Compatibility issues**: Old software depended on wraparound behavior
-   **A20 Gate**: Disables 20th address line to emulate 8086

**Handling in Modern OS Development:**

-   **A20 activation**: Quick activation using Fast A20 method (port 0x92)
-   **Protected mode requirement**: Necessary to correctly use 32-bit address space
-   **Compatibility maintenance**: BIOS starts with A20 disabled

**A20 Line Background:**

-   8086 CPU design bug caused "wraparound" where addresses beyond 1MB (e.g., 0x100000) returned to address 0
-   Enabling A20 (Address line 20) fixes this bug and makes full address space available
-   Entering protected mode with A20 disabled causes memory access to break down, preventing proper OS operation

## Role of GDT (Global Descriptor Table)

**GDT** is the core data structure for memory management in protected mode.

**Difference from Real Mode:**

-   **Real mode**: Simple calculation of segment value ×16 + offset
-   **Protected mode**: Segment registers point to GDT indices

**GDT Entry Structure (8 bytes):**

-   **Base address (32bit)**: Segment start position
-   **Limit (20bit)**: Segment size (4KB units, maximum 4GB)
-   **Access rights (8bit)**: Executable/read-write/privilege level etc.
-   **Flags (4bit)**: Attributes like 32bit/16bit, granularity

**Basic GDT Configuration:**

1. **NULL descriptor (entry 0)**: Dummy to detect invalid access
2. **CODE segment (0x08)**: Executable code area
3. **DATA segment (0x10)**: Data/stack area

**Relationship to Modern OS:**

-   **Linux**: Minimal GDT + paging-centric
-   **Windows**: More complex segmentation + paging
-   **This project**: Flat memory model (all segments 0-4GB)

**What is GDT:**

-   Table defining segment attributes (base address, size, access permissions)
-   In protected mode, segment registers like CS/DS point to GDT entries
-   This enables memory area protection and separation

**Why necessary:**

-   Transition from real mode segments (simple 16× multiplication) to flexible memory management
-   null descriptor (entry 0) is dummy to prevent invalid access

## Protected Mode Transition Flow

```
Real mode init → A20 enable → GDT register(lgdt) → CR0.PE=1 → far jump → 32bit init → VGA output
```

1. Quick A20 activation using Fast A20 (I/O port 0x92)
2. Define GDT (null/code/data) and register with CPU using `lgdt`
3. Enable protected mode with `CR0.PE=1`
4. Load CS with far jump and enter 32-bit code (execute with 32-bit offset)
5. Set segment registers and stack on 32-bit side, output strings to VGA

【Note】Minimal GDT and segments

-   First prepare GDT with null/code/data (null is sentinel).
-   Remember the "ritual" of setting CS=0x08, DS=ES=FS=GS=SS=0x10 (detailed flags can be standard).

## Hands-on: 2-File Configuration

### Step 1: Project Structure

```
day02/
├── README.md   # This file
├── boot.s      # 16bit→32bit switching and VGA display, GDT definition
└── Makefile    # Build and execution
```

See `day02_completed/` for the completed version.

### Step 2: Creating boot.s

In `boot.s`, proceed in order: A20→GDT→CR0→far jump→VGA.

```assembly
; boot.s - 16bit→32bit switching and VGA display
[org 0x7C00]
[bits 16]

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    ; Enable A20 (Fast A20: port 0x92)
    in  al, 0x92
    or  al, 0x02
    out 0x92, al

    ; Load GDT
    lgdt [gdt_descriptor]

    ; CR0.PE = 1 to enter protected mode
    mov eax, cr0
    or  eax, 1
    mov cr0, eax

    ; Set CS to 0x08 (code segment) and enter 32bit (specify 32bit offset)
    jmp dword 0x08:pm32_start

[bits 32]
pm32_start:
    cld
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x00300000

    ; Display to VGA text buffer
    mov esi, msg_pm
    mov edi, 0xB8000
    mov bl, 0x0F
.next:
    lodsb
    test al, al
    jz .done
    mov [edi], al
    mov [edi+1], bl
    add edi, 2
    jmp .next
.done:
    cli
.halt:
    hlt
    jmp .halt

; GDT definition (null / code / data)
; - `0x9A`=code access byte, `0x92`=data access byte
; - `0xCF`=4KB granularity+32bit flag+upper limit

align 8
gdt_start:
    dq 0x0000000000000000          ; null descriptor
    dq 0x00CF9A000000FFFF          ; code: base=0, limit=4GB, 32-bit, RX
    dq 0x00CF92000000FFFF          ; data: base=0, limit=4GB, 32-bit, RW
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start


msg_pm db 'Now in 32-bit protected mode! Hello VGA!', 0

times 510 - ($ - $$) db 0
dw 0xAA55
```

### Step 3: Creating Makefile

```makefile
# Makefile for Day 02 - Protected Mode Switch

AS   = nasm
QEMU = qemu-system-i386

BOOT_SRC = boot.s
OS_IMG   = os.img

all: $(OS_IMG)
	@echo "✅ Day 02: Protected mode boot image creation complete"
	@echo "🚀 Execute: make run"

$(OS_IMG): $(BOOT_SRC)
	@echo "🔨 Assembling..."
	$(AS) -f bin $(BOOT_SRC) -o $(OS_IMG)
	@echo "📏 Checking file size..."
	@if [ `wc -c < $(OS_IMG)` -eq 512 ]; then \
	  echo "✅ 512 bytes OK"; \
	else \
	  echo "❌ Error: Not 512 bytes"; exit 1; \
	fi

run: $(OS_IMG)
	@echo "🚀 Starting in QEMU..."
	$(QEMU) -fda $(OS_IMG) -boot a

debug: $(OS_IMG)
	@echo "🔍 Starting in debug mode..."
	$(QEMU) -fda $(OS_IMG) -boot a -serial stdio -monitor tcp:127.0.0.1:4444,server,nowait

clean:
	@echo "🧹 Cleaning up..."
	rm -f $(OS_IMG)
	@echo "✅ Complete"

.PHONY: all run debug clean
```

### Step 4: Build and Execute

```bash
cd day02
make clean
make all
make run
```

If "Now in 32-bit protected mode! Hello VGA!" appears in white text on the QEMU screen, it's successful.

## Code Explanation

### Detailed GDT Structure Diagram

#### GDT Memory Layout

```
Overall GDT diagram:
+---------+--------+--------+--------+--------+--------+--------+--------+
| Byte 7  | Byte 6 | Byte 5 | Byte 4 | Byte 3 | Byte 2 | Byte 1 | Byte 0 |
+---------+--------+--------+--------+--------+--------+--------+--------+

Entry 0 (NULL): 0x0000000000000000
+---------+--------+--------+--------+--------+--------+--------+--------+
|   00    |   00   |   00   |   00   |   00   |   00   |   00   |   00   |
+---------+--------+--------+--------+--------+--------+--------+--------+

Entry 1 (CODE): 0x00CF9A000000FFFF
+---------+--------+--------+--------+--------+--------+--------+--------+
|   00    |   CF   |   9A   |   00   |   00   |   00   |   FF   |   FF   |
+---------+--------+--------+--------+--------+--------+--------+--------+
    ↑        ↑        ↑        ↑      ←Base→      ←----Limit---→
Base[31:24] Flags   Access Base[23:16]  Address      0xFFFFF
            0xC=Granular+32bit  0x9A    0x00000000   4GB

Entry 2 (DATA): 0x00CF92000000FFFF
+---------+--------+--------+--------+--------+--------+--------+--------+
|   00    |   CF   |   92   |   00   |   00   |   00   |   FF   |   FF   |
+---------+--------+--------+--------+--------+--------+--------+--------+
```

#### Selector to GDT Entry Conversion

```
Selector structure:
15                    3  2   0
+--------------------+--+---+
|      Index         |TI|RPL|
+--------------------+--+---+

Example: CS = 0x08 = 0000 1000
- Index = 1 → GDT entry 1 (CODE)
- TI = 0 (use GDT)
- RPL = 0 (kernel privilege)

Example: DS = 0x10 = 0001 0000
- Index = 2 → GDT entry 2 (DATA)
- TI = 0 (use GDT)
- RPL = 0 (kernel privilege)
```

#### Access Rights Details

| Field              | Code(0x9A) | Data(0x92) | Meaning                 |
| ------------------ | ---------- | ---------- | ----------------------- |
| **P** (Present)    | 1          | 1          | Segment present         |
| **DPL** (Privilege) | 00         | 00         | Kernel level            |
| **S** (Type)       | 1          | 1          | Code/data segment       |
| **E** (Executable) | 1          | 0          | Executable/non-executable |
| **DC**             | 0          | 0          | Expand direction        |
| **RW**             | 1          | 1          | Read/write allowed      |
| **A** (Accessed)   | 0          | 0          | 0 when unused           |

#### Flag Details

| Bit     | Name      | Value | Meaning               |
| ------- | --------- | ----- | --------------------- |
| **G**   | Granularity | 1     | 4KB page units       |
| **D/B** | Size      | 1     | 32-bit segment        |
| **L**   | 64-bit    | 0     | 32-bit mode           |
| **AVL** | Available | 0     | OS available bit      |

#### Protected Mode Transition Flow

```
Step 1: GDT preparation
[Real mode] Register gdt_start address with lgdt

Step 2: PE setting
CR0.PE = 1 enables protected mode

Step 3: Segment update
far jump 0x08:pm32_start updates CS (to 32-bit mode)

Step 4: Data segment setting
mov ax, 0x10  ; Data segment selector
mov ds, ax    ; Update all data segments
```

### CR0.PE and Far Jump

-   Immediately after setting PE with `mov cr0, eax`, 16-bit instructions might remain in instruction prefetch, so use far jump to explicitly reload CS and transition to 32-bit code.

## Troubleshooting

1. Screen remains black
    - Check if `lgdt` operand (`gdt_descriptor`) is correct
    - Far jump selector (0x08), `[bits 32]` position
2. Reset occurs/system hangs
    - A20 activation (port 0x92) procedure
    - Order of procedures before/after CR0.PE setting
3. Garbled characters
    - Writing to VGA buffer 0xB8000 in 2-byte units (character+attribute)?

## Understanding Check

1. What happens if A20 line is disabled?
2. What do GDT access bytes 0x9A/0x92 mean respectively?
3. What could happen if PE=1 is set without inserting far jump?
4. Why not use BIOS interrupts after 32-bit transition?

## Preparing for Next Steps

-   ✅ A20 line activation
-   ✅ GDT construction and registration
-   ✅ Protected mode switching and far jump
-   ✅ Direct VGA buffer output

Tomorrow's Day 03 will develop VGA text display further, handling colors, cursor, scrolling, etc.