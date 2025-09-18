;     Day 09 完成版 - 例外ISR + IRQ0スタブと共通ハンドラ（32ビット）
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
	;     全汎用レジスタをスタックに保存
	pusha ; EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI を保存

	;    セグメントレジスタも保存
	push ds
	push es
	push fs
	push gs

	;   カーネルのデータセグメントに切り替え
	mov ax, 0x10; データセグメントセレクタ
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

	;    C言語で書かれたタイマーハンドラを呼び出し
	;    【重要】ここでスケジューラが動作し、current_threadが変更される可能性
	call timer_handler_c

	;   セグメントレジスタを復元
	pop gs
	pop fs
	pop es
	pop ds

	; 汎用レジスタを復元
	; 【重要】ここで復元されるレジスタは、timer_handler_c内で
	; スレッドが切り替わった場合、別のスレッドのレジスタになる！
	popa

	; 割り込み終了（IRETで元の処理に戻る）
	iret

	ISR_NOERR 0
	ISR_NOERR 3
	ISR_NOERR 6
	ISR_ERR   13
	ISR_ERR   14
