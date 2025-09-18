; boot.s - 最初のブートローダー
; x86リアルモード（16ビット）で動作

	[org  0x7C00]; BIOSが私たちのコードを0x7C00番地にロードすることを指定
	[bits 16]; 16ビットモードでアセンブル

start:
	;   === レジスタ初期化 ===
	;   セグメントレジスタをすべて0に設定（安全のため）
	cli ; 割り込み無効化（重要な初期化中は邪魔されたくない）
	xor ax, ax; AX = 0（同じレジスタ同士のXORで0になる）
	mov ds, ax; データセグメント = 0
	mov es, ax; エクストラセグメント = 0
	mov ss, ax; スタックセグメント = 0
	mov sp, 0x7C00; スタックポインタをブートローダーの直前に設定

	;    === メッセージ表示 ===
	;    "Hello OS!" を画面に表示
	mov  si, hello_msg; SI レジスタにメッセージのアドレスを設定
	call print_string; 文字列表示関数を呼び出し

	; === 無限ループ ===
	; プログラムを終了させないために無限ループ

infinite_loop:
	hlt ; CPUを休ませる（省電力）
	jmp infinite_loop; 無限ループ

	; === 文字列表示関数 ===
	; 入力：SI = 文字列のアドレス
	; 破壊：AX, BX

print_string:
	mov ah, 0x0E; BIOS機能：文字表示（Teletype output）

.next_char:
	lodsb ; SI が指すメモリから1バイト読み込み、AL に格納、SI++
	cmp   al, 0; 文字が0（文字列終端）か確認
	je    .done; 0なら終了
	int   0x10; BIOS割り込み呼び出し（文字表示）
	jmp   .next_char; 次の文字へ

.done:
	ret ; 呼び出し元に戻る

	; === データ部 ===
	hello_msg db 'Hello OS! Boot successful!', 13, 10, 0
	; db = define byte（バイト定義）
	; 13 = キャリッジリターン（CR）
	; 10 = ラインフィード（LF）
	; 0 = 文字列終端

	;     === ブートシグネチャ ===
	;     512バイトちょうどにするため、残りを0で埋める
	times 510 - ($ - $$) db 0; 現在位置から510バイトまで0で埋める
	dw    0xAA55; ブートシグネチャ（これがないとBIOSが認識しない）
