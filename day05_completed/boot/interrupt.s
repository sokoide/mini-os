; Day 05 完成版 - 例外ISRスタブと共通ハンドラ（32ビット）

[bits 32]

;      エラーコード無し例外用マクロ
%macro ISR_NOERR 1
	global isr%1

isr%1:
	push dword 0; ダミーのエラーコード
	push dword %1; ベクタ番号
	jmp  isr_common
%endmacro

	;      エラーコード有り例外用マクロ
	%macro ISR_ERR 1
	global isr%1

isr%1:
	push dword %1; ベクタ番号（エラーコードはCPUがpush済）
	jmp  isr_common
%endmacro

extern isr_handler_c; C側共通ハンドラ

isr_common:
	pusha ; 汎用レジスタ退避
	cld
	mov   eax, esp; ESP上: [errcode][vec][pusha...] の先頭を渡す
	push  eax
	call  isr_handler_c
	add   esp, 4
	popa
	add   esp, 8; vec + errcode を破棄
	iretd

	; 代表的な例外のスタブ定義
	ISR_NOERR 0    ; Divide-by-zero
	ISR_NOERR 3    ; Breakpoint
	ISR_NOERR 6    ; Invalid Opcode
	ISR_ERR   13   ; General Protection Fault
	ISR_ERR   14   ; Page Fault
