;       Day 12 完成版 - ブートローダ（16ビット）
[org    0x7C00]
[bits   16]
%define KERNEL_LOAD_SEG 0x1000
%define KERNEL_LOAD_OFF 0x0000
%define KERNEL_LOAD_LIN 0x00010000
%define KERNEL_SECTORS  127

start:
	cli
	xor ax, ax
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov sp, 0x7C00
	mov [boot_drive], dl
	;   A20ライン有効化
	in  al, 0x92
	or  al, 0x02
	out 0x92, al
	;   カーネル読込（FAT等を使わない最小例）
	mov ax, KERNEL_LOAD_SEG
	mov es, ax
	mov bx, KERNEL_LOAD_OFF
	mov si, 3

.r:
	mov ah, 0x02
	mov al, KERNEL_SECTORS
	mov ch, 0
	mov cl, 2
	mov dh, 0
	mov dl, [boot_drive]
	int 0x13
	jc  .f
	jmp .ok

.f:
	dec si
	jnz .r
	jmp $

.ok:
	;    プロテクトモード移行
	lgdt [gdt_descriptor]
	mov  eax, cr0
	or   eax, 1
	mov  cr0, eax
	jmp  dword 0x08:KERNEL_LOAD_LIN
boot_drive db 0
align 8

gdt_start:
	dq 0
	dq 0x00CF9A000000FFFF
	dq 0x00CF92000000FFFF

gdt_end:
gdt_descriptor:
	dw    gdt_end-gdt_start-1
	dd    gdt_start
	times 510-($-$$) db 0
	dw    0xAA55
