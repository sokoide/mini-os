; boot_constants.inc - Boot-related constants for assembly and C integration
; 【役割】ブートローダとカーネルで共有する定数定義

; Memory Layout Constants
%define BOOT_LOAD_ADDRESS      0x7c00      ; BIOS loads us here
%define KERNEL_TEMP_LOAD        0x10000     ; Temporary kernel load location
%define KERNEL_FINAL_ADDRESS    0x100000    ; Final kernel location (1MB)
%define STACK_TOP_ADDRESS       0x200000    ; Stack top (2MB)

; Boot Sector Reading Constants
%define BIOS_READ_FUNCTION      0x02        ; BIOS disk read function
%define KERNEL_SECTORS          127         ; Number of sectors to read (63.5KB)
%define BOOT_CYLINDER           0           ; Cylinder 0
%define BOOT_START_SECTOR       2           ; Start from sector 2 (sector 1 is boot)
%define BOOT_HEAD               0           ; Head 0
%define BOOT_DRIVE              0x00        ; Drive A (floppy)

; GDT Segment Selectors
%define CODE_SEGMENT_SELECTOR   0x08        ; Code segment selector
%define DATA_SEGMENT_SELECTOR   0x10        ; Data segment selector

; A20 Gate Constants
%define A20_FAST_GATE_PORT      0x92        ; Fast A20 gate port
%define A20_ENABLE_MASK         2           ; A20 enable bit

; Protected Mode Constants
%define PROTECTED_MODE_ENABLE   0x1         ; CR0 PE bit

; BIOS Functions
%define BIOS_VIDEO_WRITE        0x0e        ; BIOS character output
%define BIOS_VIDEO_INT          0x10        ; BIOS video interrupt
%define BIOS_DISK_INT           0x13        ; BIOS disk interrupt

; VGA Text Mode Constants
%define VGA_TEXT_BUFFER         0xb8000     ; VGA text buffer address

; Debug Marker Constants (VGA display)
%define DEBUG_MARKER_A          0x0741      ; 'A' - 32-bit start marker
%define DEBUG_MARKER_B          0x0742      ; 'B' - segment setup complete
%define DEBUG_MARKER_C          0x0743      ; 'C' - segment config complete
%define DEBUG_MARKER_D          0x0744      ; 'D' - before copy
%define DEBUG_MARKER_E          0x0745      ; 'E' - copy complete
%define DEBUG_MARKER_F          0x0746      ; 'F' - stack setup complete
%define DEBUG_MARKER_G          0x0747      ; 'G' - before jump

; GDT Access Byte Constants
%define GDT_CODE_ACCESS         10011010b   ; Code segment: executable, readable
%define GDT_DATA_ACCESS         10010010b   ; Data segment: writable

; GDT Granularity Constants
%define GDT_GRANULARITY_4KB     11001111b   ; 4KB pages, 32-bit mode

; Copy Operation Constants
%define KERNEL_COPY_SIZE        8192        ; 32KB in DWORD units (32KB/4)
