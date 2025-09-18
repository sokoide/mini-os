;     Day 12 完成版 - コンテキストスイッチ（32ビット）- day11準拠
[bits 32]

;      void context_switch(uint32_t* old_esp, uint32_t* new_esp)
;      標準的なコンテキストスイッチ - レジスタ状態を保存・復元
	global context_switch

context_switch:
	;    現在のレジスタ状態を保存（EFLAGS含む）
	pushfd
	push ebp
	push edi
	push esi
	push edx
	push ecx
	push ebx
	push eax

	;   現在のESPを old_esp に保存
	mov eax, [esp+36]; old_esp ポインタ (8*4 + 4)
	mov [eax], esp; 現在のESPを保存

	;   新しいESPに切り替え
	mov esp, [esp+40]; new_esp 値 (8*4 + 8)

	;   新しいスレッドのレジスタ状態を復元
	pop eax
	pop ebx
	pop ecx
	pop edx
	pop esi
	pop edi
	pop ebp
	popfd
	ret

	;      初回コンテキストスイッチ（最初のスレッド専用）
	;      initial_context_switch(new_esp)
	;      役割: 現在の状態を保存せず、スレッド初期スタックへ切り替えて直ちに実行を開始する
	global initial_context_switch

initial_context_switch:
	;   引数: [esp+4] = new_esp（切替先スレッドのESP）
	mov eax, [esp+4]
	mov esp, eax

	;     スレッド初期スタックに積んだレジスタ/EFLAGSを復元
	pop   eax
	pop   ebx
	pop   ecx
	pop   edx
	pop   esi
	pop   edi
	pop   ebp
	popfd ; 割り込み状態を含めて復元

	;   スタックトップの関数アドレスへ遷移
	pop eax; 関数アドレスを取得
	jmp eax; 直接ジャンプ（戻り先は不要）
