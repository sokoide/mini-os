; boot.s - 16ビットから32ビットへの完全ブートローダ
; MBR（Master Boot Record）形式で起動する

%include "boot_constants.inc"

	[org  BOOT_LOAD_ADDRESS]; BIOS loads us at 0x7c00
	[bits 16]; 16ビットモードで開始

_start16:
	cli ; 割り込み無効

	;   セグメントレジスタ初期化
	xor ax, ax
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov sp, BOOT_LOAD_ADDRESS; スタックをブートセクタの直前に設定

	;    起動メッセージ表示
	mov  si, boot_msg
	call print_string

	;   カーネルをディスクから読み込み（1MB位置）
	mov ah, BIOS_READ_FUNCTION; BIOS読み取り機能
	mov al, KERNEL_SECTORS; 読み取るセクタ数
	mov ch, BOOT_CYLINDER; シリンダ0
	mov cl, BOOT_START_SECTOR; セクタ2から（セクタ1はブートセクタ）
	mov dh, BOOT_HEAD; ヘッド0
	mov dl, BOOT_DRIVE; ドライブA（フロッピー）
	mov bx, KERNEL_TEMP_LOAD >> 4; 一時的に0x10000に読み込み（セグメント形式）
	mov es, bx
	mov bx, 0
	int BIOS_DISK_INT; ディスク読み取り
	jc  disk_error

	;    A20ライン有効化
	call enable_a20

	;    A20有効化確認メッセージ
	mov  si, a20_msg
	call print_string

	;    GDT読み込み
	lgdt [gdt_descriptor]

	;    プロテクトモード移行メッセージ
	mov  si, pmode_msg
	call print_string

	;   プロテクトモードに移行
	mov eax, cr0
	or  eax, PROTECTED_MODE_ENABLE; PE bit設定
	mov cr0, eax

	;   32ビットコードセグメントにジャンプ
	;   絶対アドレスを使用
	jmp CODE_SEGMENT_SELECTOR:BOOT_LOAD_ADDRESS+start32-$$

disk_error:
	mov  si, disk_error_msg
	call print_string
	jmp  $

	; 文字列出力関数（16ビット）

print_string:
	pusha
	mov ah, BIOS_VIDEO_WRITE; BIOS文字出力

.loop:
	lodsb
	cmp al, 0
	je  .done
	int BIOS_VIDEO_INT
	jmp .loop

.done:
	popa
	ret

	; A20ライン有効化（簡易版）

enable_a20:
	;   Fast A20 Gate method
	in  al, A20_FAST_GATE_PORT
	or  al, A20_ENABLE_MASK
	out A20_FAST_GATE_PORT, al
	ret

	;     32ビットコード開始
	[bits 32]

start32:
	;   デバッグ: 32ビットコードに到達
	mov dword [VGA_TEXT_BUFFER], DEBUG_MARKER_A; 'A' - 32ビット開始マーカー

	;   32ビットセグメント設定
	mov ax, DATA_SEGMENT_SELECTOR; データセグメントセレクタ
	mov ds, ax
	mov dword [VGA_TEXT_BUFFER+4], DEBUG_MARKER_B; 'B' - セグメント設定後
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax

	;   デバッグ: セグメント設定完了
	mov dword [VGA_TEXT_BUFFER+8], DEBUG_MARKER_C; 'C' - セグメント設定完了

	;   デバッグ: カーネルコピー開始前
	mov dword [VGA_TEXT_BUFFER+12], DEBUG_MARKER_D; 'D' - コピー開始前

	;   カーネルを0x10000から0x100000に移動
	mov esi, KERNEL_TEMP_LOAD; ソースアドレス
	mov edi, KERNEL_FINAL_ADDRESS; デスティネーション（1MB）
	mov ecx, KERNEL_COPY_SIZE; 32KB分コピー（DWORD単位）
	cld
	rep movsd

	;   デバッグ: カーネルコピー完了
	mov dword [VGA_TEXT_BUFFER+16], DEBUG_MARKER_E; 'E' - コピー完了

	;   スタック設定（2MB位置）
	mov esp, STACK_TOP_ADDRESS
	mov ebp, 0

	;   デバッグ: スタック設定完了
	mov dword [VGA_TEXT_BUFFER+20], DEBUG_MARKER_F; 'F' - スタック設定完了

	;   デバッグ: カーネルジャンプ前
	mov dword [VGA_TEXT_BUFFER+24], DEBUG_MARKER_G; 'G' - ジャンプ前

	;   カーネルメイン呼び出し
	jmp KERNEL_FINAL_ADDRESS; カーネルの先頭にジャンプ

	;     GDT（Global Descriptor Table）
	align 4

gdt_start:
	;  NULLセグメント
	dd 0x0, 0x0

	;  コードセグメント (0x08)
	dw 0xffff; limit (bits 0-15)
	dw 0x0; base (bits 0-15)
	db 0x0; base (bits 16-23)
	db GDT_CODE_ACCESS; access byte (executable, readable)
	db GDT_GRANULARITY_4KB; granularity (4KB pages, 32-bit)
	db 0x0; base (bits 24-31)

	;  データセグメント (0x10)
	dw 0xffff; limit (bits 0-15)
	dw 0x0; base (bits 0-15)
	db 0x0; base (bits 16-23)
	db GDT_DATA_ACCESS; access byte (writable)
	db GDT_GRANULARITY_4KB; granularity (4KB pages, 32-bit)
	db 0x0; base (bits 24-31)

gdt_end:

gdt_descriptor:
	dw gdt_end - gdt_start - 1; GDT size
	dd gdt_start; GDT address

	; データ
	boot_msg db 'Mini OS Booting...', 13, 10, 0
	a20_msg db 'A20 Line Enabled', 13, 10, 0
	pmode_msg db 'Entering Protected Mode...', 13, 10, 0
	disk_error_msg db 'Disk read error!', 13, 10, 0

	;     ブートセクタ署名（510バイト目に配置）
	times 510-($-$$) db 0
	dw    0xaa55
