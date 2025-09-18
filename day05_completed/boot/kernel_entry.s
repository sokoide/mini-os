; Day 05 完成版 - 32ビット カーネルエントリ

[bits  32]
	global kernel_entry
	extern kmain

kernel_entry:
	;   データセグメントを設定
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax

	;   スタック初期化
	mov esp, 0x00300000

	;    C の kmain を呼び出し
	call kmain

.halt:
	hlt
	jmp .halt
