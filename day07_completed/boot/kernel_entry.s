; Day 07 完成版 - 32ビット カーネルエントリ

[bits  32]
	global kernel_entry
	extern kmain

kernel_entry:
	mov  ax, 0x10
	mov  ds, ax
	mov  es, ax
	mov  fs, ax
	mov  gs, ax
	mov  ss, ax
	mov  esp, 0x00300000
	call kmain

.halt:
	hlt
	jmp .halt
