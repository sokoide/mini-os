;     Day 08 完成版 - コンテキストスイッチ（32ビット）
[bits 32]

;      void context_switch(uint32_t* old_esp, uint32_t* new_esp)
;      標準的なコンテキストスイッチ - レジスタ状態を保存・復元
	global context_switch

context_switch:
	;      現在のレジスタ状態を保存
	;      注意: ここでは sti は不要。EFLAGS は popfd で復元されるため、
	;      スイッチ中に割り込み再入が起きないよう保存→切替→復元の順を厳守する。
	pushfd ; EFLAGS を保存（IF含む）
	push   ebp
	push   edi
	push   esi
	push   edx
	push   ecx
	push   ebx
	push   eax

	;   現在のESPを old_esp に保存
	mov eax, [esp+36]; old_esp ポインタ (8*4 + 4)
	mov [eax], esp; 現在のESPを保存

	;   新しいESPに切り替え
	mov esp, [esp+40]; new_esp 値 (8*4 + 8)

	;     新しいスレッドのレジスタ状態を復元
	;     初期スタック (EAX..EBP, EFLAGS, 関数アドレス) と整合する順序で復元
	pop   eax
	pop   ebx
	pop   ecx
	pop   edx
	pop   esi
	pop   edi
	pop   ebp
	popfd ; EFLAGS 復元（IF も元通り）
	ret   ; 初回は関数アドレスへ、以降は呼出元へ復帰
