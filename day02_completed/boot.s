[org  0x7C00]
[bits 16]

start:
	cli
	xor ax, ax
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov sp, 0x7C00

	;   A20 有効化（Fast A20: port 0x92）
	in  al, 0x92
	or  al, 0x02
	out 0x92, al

	;    GDT をロード
	lgdt [gdt_descriptor]

	;   CR0.PE = 1 でプロテクトモードへ
	mov eax, cr0
	or  eax, 1
	mov cr0, eax

	;   CS を 0x08（コードセグメント）にして 32bitへ（32bitオフセット指定）
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

	;   VGA テキストバッファに表示
	mov esi, msg_pm
	mov edi, 0xB8000
	mov bl, 0x0F

.next:
	lodsb
	test al, al
	jz   .done
	mov  [edi], al
	mov  [edi+1], bl
	add  edi, 2
	jmp  .next

.done:
	cli

.halt:
	hlt
	jmp .halt

	;     GDT定義（null / code / data）
	align 8

gdt_start:
	dq 0x0000000000000000; null descriptor
	dq 0x00CF9A000000FFFF; code: base=0, limit=4GB, 32-bit, RX
	dq 0x00CF92000000FFFF; data: base=0, limit=4GB, 32-bit, RW

gdt_end:

gdt_descriptor:
	dw gdt_end - gdt_start - 1
	dd gdt_start

	msg_pm db 'Now in 32-bit protected mode! Hello VGA!', 0

	times 510 - ($ - $$) db 0
	dw    0xAA55
