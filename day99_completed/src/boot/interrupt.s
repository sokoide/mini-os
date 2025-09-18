; interrupt.s - 割り込みハンドラのアセンブリ部分

	section .text

;      外部関数の宣言
	extern timer_handler_c
	extern keyboard_handler_c

;      タイマー割り込みハンドラ（アセンブリ部分）
;      【重要】この関数は割り込みが発生すると自動的に呼ばれる
	global timer_interrupt_handler

timer_interrupt_handler:
	; === 割り込み処理の開始 ===
	; CPUは割り込み発生時に以下を自動で行う：
	; 1. 現在のEFLAGS、CS、EIPをスタックにプッシュ
	; 2. 割り込みを無効化（CLI相当）
	; 3. IDTから該当エントリのアドレスにジャンプ

	;     全汎用レジスタをスタックに保存
	;     【なぜ必要？】コンテキストスイッチでは、現在実行中のスレッドの
	;     全レジスタ状態を保存して、後で完全に復元する必要がある
	pusha ; EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI を保存

	;    セグメントレジスタも保存
	push ds
	push es
	push fs
	push gs

	;   カーネルのデータセグメントに切り替え
	;   【理由】割り込みハンドラはカーネル空間で実行される
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
	; CPUが自動で以下を行う：
	; 1. スタックからEIP、CS、EFLAGSを復元
	; 2. 割り込み有効化
	; 3. 元の処理位置にジャンプ
	iret

	;      キーボード割り込みハンドラ（アセンブリ部分）
	;      【重要】IRQ1（キーボード）が発生すると自動的に呼ばれる
	global keyboard_interrupt_handler

keyboard_interrupt_handler:
	; === キーボード割り込み処理の開始 ===
	; CPUは割り込み発生時に以下を自動で行う：
	; 1. 現在のEFLAGS、CS、EIPをスタックにプッシュ
	; 2. 割り込みを無効化（CLI相当）
	; 3. IDTから該当エントリのアドレスにジャンプ

	;     全汎用レジスタをスタックに保存
	;     【重要】キーボード割り込みでは通常コンテキストスイッチは発生しないが、
	;     一貫性のためにすべてのレジスタを保存
	pusha ; EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI を保存

	;    セグメントレジスタも保存
	push ds
	push es
	push fs
	push gs

	;   カーネルのデータセグメントに切り替え
	;   【理由】割り込みハンドラはカーネル空間で実行される
	mov ax, 0x10; データセグメントセレクタ
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

	;    C言語で書かれたキーボードハンドラを呼び出し
	;    【役割】スキャンコード読み取りとASCII変換、バッファ格納
	call keyboard_handler_c

	;   セグメントレジスタを復元
	pop gs
	pop fs
	pop es
	pop ds

	; 汎用レジスタを復元
	popa

	; 割り込み終了
	iret

	;      コンテキストスイッチ関数
	;      switch_context(old_esp_ptr, new_esp)
	;      【役割】あるスレッドから別のスレッドに実行を切り替える
	;      【備考】
	;      今の設計だと「EAX に関数アドレスを pop して jmp eax」という流れになっていますが、
	;      将来的に「ユーザーモードスレッド（ring3）」をやりたい場合は、初期スタックに CS / SS も積んで、iret 戻りを使う必要があります。
	global context_switch

context_switch:
	; 関数の引数：
	; [esp+8] = old_esp_ptr (現在のスレッドのESPを保存するアドレス)
	; [esp+12] = new_esp (切り替え先スレッドのESP値)

	;      現在の実行状態を保存（EFLAGS含む）
	;      【重要】ここで保存される状態は、次回このスレッドが実行される時に
	;      完全に復元される
	pushfd ; EFLAGS保存（割り込み状態含む）
	push   ebp; ベースポインタ保存
	push   edi; 汎用レジスタ保存
	push   esi
	push   edx
	push   ecx
	push   ebx
	push   eax

	;   現在のスタックポインタを old_esp_ptr に保存
	;   これにより、現在のスレッドの実行状態が完全に保存される
	mov eax, [esp+36]; old_esp_ptr のアドレスを取得 (8*4 + 4 = 36)
	mov [eax], esp; 現在の ESP を保存

	;   新しいスレッドのスタックポインタに切り替え
	;   【これがコンテキストスイッチの核心部分】
	mov esp, [esp+40]; new_esp を ESP に設定 (8*4 + 8 = 40)

	;     新しいスレッドの状態を復元
	;     【重要】ここで復元される状態は、以前にこのスレッドが
	;     switch_context で保存した状態、または初期化時に設定された状態
	pop   eax; 汎用レジスタ復元
	pop   ebx
	pop   ecx
	pop   edx
	pop   esi
	pop   edi
	pop   ebp; ベースポインタ復元
	popfd ; EFLAGS復元（割り込み状態含む）
	;     EFLAGSの復帰で割り込みが有効化されているので、stiはここでは不要

	; 関数から戻る（新しいスレッドの続きを実行）
	; 【注意】この ret は元の呼び出し元ではなく、
	; 新しいスレッドの呼び出し元に戻る！
	ret

	;      初期コンテキストスイッチ関数（最初のスレッド専用）
	;      initial_context_switch(new_esp)
	;      【役割】現在の状態を保存せず、直接新しいスレッドに切り替える
	global initial_context_switch

initial_context_switch:
	; 引数: [esp+4] = new_esp (切り替え先スレッドのESP値)

	;   新しいスレッドのスタックポインタに直接切り替え
	mov eax, [esp+4]; new_esp を EAX に取得
	mov esp, eax; ESP を設定

	;     新しいスレッドの状態を復元
	;     （スレッド作成時に初期化された8つのレジスタ値：EAX, EBX, ECX, EDX, ESI, EDI, EBP）
	pop   eax; 汎用レジスタ復元
	pop   ebx
	pop   ecx
	pop   edx
	pop   esi
	pop   edi
	pop   ebp; ベースポインタ復元
	popfd ; EFLAGS復元（割り込み有効化含む）
	;     EFLAGSの復帰で割り込みが有効化されているので、stiはここでは不要

	;   スレッド関数を実行
	;   【重要】retではなくjmpを使用（C呼び出し規約との整合性のため）
	;   スタックトップには関数アドレスがある
	pop eax; 関数アドレスをEAXに取得
	jmp eax; 関数に直接ジャンプ（戻りアドレス不要）
