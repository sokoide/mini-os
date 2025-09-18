; Day 07 完成版 - ブートローダ（16ビット）: A20, GDT（本ファイル内定義）, カーネル読込, プロテクトモード移行
; 512バイトのブートセクタ + シグネチャを生成

[org  0x7C00]
[bits 16]

%define KERNEL_LOAD_SEG  0x1000
%define KERNEL_LOAD_OFF  0x0000
%define KERNEL_LOAD_LIN  0x00010000
%define KERNEL_SECTORS   127

start:
	cli
	xor ax, ax
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov sp, 0x7C00

	mov [boot_drive], dl

	in  al, 0x92
	or  al, 0x02
	out 0x92, al

	mov ax, KERNEL_LOAD_SEG
	mov es, ax
	mov bx, KERNEL_LOAD_OFF

	mov si, 3

.read_retry:
	mov ah, 0x02
	mov al, KERNEL_SECTORS
	mov ch, 0x00
	mov cl, 0x02
	mov dh, 0x00
	mov dl, [boot_drive]
	int 0x13
	jc  .read_fail
	jmp .read_ok

.read_fail:
	dec si
	jnz .read_retry
	jmp $

.read_ok:

	lgdt [gdt_descriptor]

	mov eax, cr0
	or  eax, 1
	mov cr0, eax
	jmp dword 0x08:KERNEL_LOAD_LIN

boot_drive db 0

align 8

gdt_start:
	dq 0x0000000000000000
	dq 0x00CF9A000000FFFF
	dq 0x00CF92000000FFFF

gdt_end:

gdt_descriptor:
	dw gdt_end - gdt_start - 1
	dd gdt_start

	times 510 - ($ - $$) db 0
	dw    0xAA55
