;     Day 11 完成版 - 例外ISR + IRQ0/IRQ1スタブと共通ハンドラ（32ビット）
[bits 32]

%macro ISR_NOERR 1
	global isr%1

isr%1:
	push dword 0
	push dword %1
	jmp  isr_common
%endmacro

%macro ISR_ERR 1
global isr%1

isr%1:
	push dword %1
	jmp  isr_common
%endmacro

extern isr_handler_c
extern timer_handler_c
extern keyboard_handler_c

	; 例外用の共通ハンドラ（コンテキストスイッチしない）

isr_common:
	pusha
	cld
	mov  eax, esp
	push eax
	call isr_handler_c
	add  esp, 4
	popa
	add  esp, 8
	sti  ; 明示的に割り込み有効化
	iretd

	;      タイマー割り込み専用ハンドラ（コンテキストスイッチ対応）
	global timer_interrupt_handler

timer_interrupt_handler:
	pusha
	push ds
	push es
	push fs
	push gs
	mov  ax, 0x10
	mov  ds, ax
	mov  es, ax
	mov  fs, ax
	mov  gs, ax
	call timer_handler_c
	pop  gs
	pop  fs
	pop  es
	pop  ds
	popa
	iret

	;      キーボード（IRQ1）割り込みハンドラ
	global keyboard_interrupt_handler

keyboard_interrupt_handler:
	pusha
	push ds
	push es
	push fs
	push gs
	mov  ax, 0x10
	mov  ds, ax
	mov  es, ax
	mov  fs, ax
	mov  gs, ax
	call keyboard_handler_c
	pop  gs
	pop  fs
	pop  es
	pop  ds
	popa
	iret

	; 例外
	ISR_NOERR 0
	ISR_NOERR 3
	ISR_NOERR 6
	ISR_ERR   13
	ISR_ERR   14
