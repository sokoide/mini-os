; Day 05 完成版 - ブートローダ（16ビット）: A20, GDT（本ファイル内定義）, カーネル読込, プロテクトモード移行
; 512バイトのブートセクタ + シグネチャを生成

[org  0x7C00]
[bits 16]

%define KERNEL_LOAD_SEG  0x1000; 0x1000:0000 => 0x00010000（読み込み先）
%define KERNEL_LOAD_OFF  0x0000
%define KERNEL_LOAD_LIN  0x00010000; プロテクトモードでのジャンプ先（リンク時アドレス）
%define KERNEL_SECTORS   127

start:
	cli
	xor ax, ax
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov sp, 0x7C00

	;   ブートドライブ番号（DL）を保存
	mov [boot_drive], dl

	;   A20ライン有効化（0x92ポート / Fast A20）
	in  al, 0x92
	or  al, 0x02
	out 0x92, al

	;   BIOS INT 13h を使って 0x00010000 にカーネルを読み込む
	mov ax, KERNEL_LOAD_SEG
	mov es, ax
	mov bx, KERNEL_LOAD_OFF

	mov si, 3; リトライ回数

.read_retry:
	mov ah, 0x02; セクタ読み込み
	mov al, KERNEL_SECTORS
	mov ch, 0x00; シリンダ 0
	mov cl, 0x02; セクタ 2（LBA1）
	mov dh, 0x00; ヘッド 0
	mov dl, [boot_drive]; BIOS のドライブ番号
	int 0x13
	jc  .read_fail
	jmp .read_ok

.read_fail:
	dec si
	jnz .read_retry
	jmp $; 致命的エラー（停止）

.read_ok:

	;    GDT をロード（本ファイル内定義）
	lgdt [gdt_descriptor]

	;   プロテクトモードへ移行
	mov eax, cr0
	or  eax, 1
	mov cr0, eax

	;   32ビットオフセット指定のファージャンプで kernel_entry へ
	jmp dword 0x08:KERNEL_LOAD_LIN

	; ---------------- データ ----------------
	boot_drive db 0

	;     ---------------- GDT（本ファイル内） ----------------
	align 8

gdt_start:
	dq 0x0000000000000000; ヌルディスクリプタ
	dq 0x00CF9A000000FFFF; コード: base=0, limit=4GB, 32ビット
	dq 0x00CF92000000FFFF; データ: base=0, limit=4GB, 32ビット

gdt_end:

gdt_descriptor:
	dw gdt_end - gdt_start - 1
	dd gdt_start

	;     ---------------- ブートシグネチャ ----------------
	times 510 - ($ - $$) db 0
	dw    0xAA55
