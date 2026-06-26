; kernel_entry.s - カーネルエントリーポイント
; ブートローダーからカーネルへの制御移行を処理

[bits   32]
	[global _start]; リンカのエントリーポイント
	[extern kernel_main]; kernel.c のメイン関数
	[extern stack_top]; linker script で定義されたスタック

	section .text

_start:
	;   デバッグ: カーネルエントリーポイント到達
	mov dword [0xb801c], 0x0748; 'H' - カーネルエントリ到達

	;   プロテクトモードでのセグメント設定確認
	mov ax, 0x10; データセグメントセレクタ
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax

	;   デバッグ: セグメント設定完了
	mov dword [0xb8020], 0x0749; 'I' - セグメント設定完了

	;   スタック設定
	mov esp, 0x00300000
	mov ebp, 0

	;   デバッグ: スタック設定完了
	mov dword [0xb8024], 0x074a; 'J' - スタック設定完了

	;   デバッグ: BSS クリア開始前
	mov dword [0xb8028], 0x074b; 'K' - BSS クリア開始前

	;      BSS セクションをゼロクリア（重要！）
	extern __bss_start
	extern __bss_end
	mov    edi, __bss_start
	mov    ecx, __bss_end
	sub    ecx, edi
	xor    eax, eax
	cld
	rep    stosb

	;   デバッグ: BSS クリア完了
	mov dword [0xb802c], 0x074c; 'L' - BSS クリア完了

	;   デバッグ: kernel_main 呼び出し前
	mov dword [0xb8030], 0x074d; 'M' - kernel_main 呼び出し前

	;    カーネルメイン関数を呼び出し
	call kernel_main

	;   デバッグ: kernel_main から戻り（通常到達しない）
	mov dword [0xb8034], 0x074e; 'N' - kernel_main から戻り

	; カーネルが終了した場合のハルト（通常は到達しない）

.halt:
	hlt
	jmp .halt
