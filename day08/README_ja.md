# Day 08: コンテキストスイッチ 🔄

## 本日のゴール

アセンブリで context_switch 関数を実装し、TCB の esp を使ってスレッドの実行状態を切り替える。

## 背景

Day7 で TCB と READY リストを設計したが、まだ実際のスレッド切り替えができていない。本日はコンテキストスイッチの核となるアセンブリ関数を実装し、レジスタ保存・復元、スタック切り替えを行うことで、本格的なマルチスレッドの基盤を完成させる。

## 新しい概念

-   **コンテキストスイッチ**: マルチタスクの核となる概念。「あるスレッドの実行状態（レジスタなど）を保存し、別のスレッドの状態を復元すること」。

## 学習内容

-   pushfd/push と popfd/pop によるレジスタ保存・復元
-   ESP の保存・復元とスタック切り替え
-   初期スタック設計
-   Thread Control Block (TCB) 構造

## タスクリスト

-   [ ] context_switch.s ファイルを作成し、アセンブリ関数を実装する
-   [ ] レジスタ保存・復元（pushfd/push/pop/popfd）の順序を正しく実装する
-   [ ] ESP の保存・復元とスタック切り替えを行う
-   [ ] kernel.c で初期スタックを構築する関数を実装する
-   [ ] TCB 構造体を更新し、esp フィールドを活用する
-   [ ] デモスレッドを作成し、context_switch でスレッド切り替えを行う
-   [ ] QEMU で動作確認し、スレッド A の実行を確認する

## 構成

```
boot/boot.s, boot/kernel_entry.s
boot/context_switch.s   # コンテキストスイッチ本体（32bit）
io.h, vga.h
kernel.c                # TCB/スレッド管理 + 切替呼び出し
Makefile
```

## 実装ガイド

### コンテキストスイッチの詳細図解

**スタック状態の変化過程:**

```
🔄 実行中スレッド（ThreadA）のスタック:
Higher Address
+------------------+
|  ThreadA データ  |
|       ...        |
+------------------+ <- ESP (ThreadA実行中)
|                  |
Lower Address


📥 context_switch呼び出し時（保存処理）:
ThreadA → context_switch(&threadA->esp, threadB->esp);

保存順序: pushfd → push ebp → push edi → ... → push eax

+------------------+
|  ThreadA データ  |
+------------------+
|      EFLAGS      | <- pushfd
|       EBP        | <- push ebp
|       EDI        | <- push edi
|       ESI        | <- push esi
|       EDX        | <- push edx
|       ECX        | <- push ecx
|       EBX        | <- push ebx
|       EAX        | <- push eax
+------------------+ <- ESP (保存完了時)
                     ↑ この位置をthreadA->espに保存

🔄 スタック切り替え:
mov [old_esp], esp   ; threadA->esp = 現在のESP
mov esp, new_esp     ; ESP = threadB->esp


📤 ThreadB復元処理（復元順序）:
pop eax → pop ebx → ... → pop ebp → popfd → ret

ThreadBのスタック（復元前）:
+------------------+
|  ThreadB データ  |
+------------------+
|      EFLAGS      | ← popfdで復元
|       EBP        | ← pop ebpで復元
|       EDI        | ← pop ediで復元
|       ESI        | ← pop esiで復元
|       EDX        | ← pop edxで復元
|       ECX        | ← pop ecxで復元
|       EBX        | ← pop ebxで復元
|       EAX        | ← pop eaxで復元
+------------------+ <- ESP (ThreadB開始時)

復元完了後、ThreadBが実行再開
```

**初期スタック構築の詳細:**

```c
// スレッド初期化時のスタック構築
static void init_stack(thread_t* t, void (*func)(void)) {
    uint32_t* sp = &t->stack[1024];

    // context_switchで復元される順序に合わせて積む
    *--sp = (uint32_t)func;    // 関数アドレス（最後にretで飛ぶ）
    *--sp = 0x00000002;        // EFLAGS（IF=0, reserved bit=1）
    *--sp = 0;                 // EBP
    *--sp = 0;                 // EDI
    *--sp = 0;                 // ESI
    *--sp = 0;                 // EDX
    *--sp = 0;                 // ECX
    *--sp = 0;                 // EBX
    *--sp = 0;                 // EAX

    t->esp = (uint32_t)sp;     // 最終的なESP位置
}
```

### 完全実装コード

**boot/context_switch.s**

```assembly
; Day 08 完成版 - コンテキストスイッチ（32ビット）
[bits 32]

; void context_switch(uint32_t* old_esp, uint32_t* new_esp)
; 標準的なコンテキストスイッチ - レジスタ状態を保存・復元
global context_switch
context_switch:
  ; 現在のレジスタ状態を保存
  ; 注意: ここでは sti は不要。EFLAGS は popfd で復元されるため、
  ;       スイッチ中に割り込み再入が起きないよう保存→切替→復元の順を厳守する。
  pushfd                  ; EFLAGS を保存（IF含む）
  push ebp
  push edi
  push esi
  push edx
  push ecx
  push ebx
  push eax

  ; 現在のESPを old_esp に保存
  mov  eax, [esp+36]      ; old_esp ポインタ (8*4 + 4)
  mov  [eax], esp         ; 現在のESPを保存

  ; 新しいESPに切り替え
  mov  esp, [esp+40]      ; new_esp 値 (8*4 + 8)

  ; 新しいスレッドのレジスタ状態を復元
  ; 初期スタック (EAX..EBP, EFLAGS, 関数アドレス) と整合する順序で復元
  pop eax
  pop ebx
  pop ecx
  pop edx
  pop esi
  pop edi
  pop ebp
  popfd                   ; EFLAGS 復元（IF も元通り）
  ret                     ; 初回は関数アドレスへ、以降は呼出元へ復帰
```

**kernel.c（完全実装）**

```c
// Day 08 完成版 - コンテキストスイッチの最小デモ
#include <stdint.h>

#include "io.h"
#include "vga.h"

// --- VGA 簡易 ---
static volatile uint16_t* const VGA_MEM = (uint16_t*)0xB8000;
static uint16_t cx, cy;
static uint8_t col = 0x0F;
static inline uint16_t ve(char c, uint8_t a) {
    return (uint16_t)c | ((uint16_t)a << 8);
}
void vga_set_color(vga_color_t f, vga_color_t b) {
    col = (uint8_t)f | ((uint8_t)b << 4);
}
void vga_move_cursor(uint16_t x, uint16_t y) {
    cx = x;
    cy = y;
    uint16_t p = y * VGA_WIDTH + x;
    outb(0x3D4, 14);
    outb(0x3D5, (p >> 8) & 0xFF);
    outb(0x3D4, 15);
    outb(0x3D5, p & 0xFF);
}
void vga_clear(void) {
    for (uint16_t y = 0; y < VGA_HEIGHT; y++)
        for (uint16_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEM[y * VGA_WIDTH + x] = ve(' ', col);
    vga_move_cursor(0, 0);
}
void vga_putc(char c) {
    if (c == '\n') {
        cx = 0;
        cy++;
        vga_move_cursor(cx, cy);
        return;
    }
    VGA_MEM[cy * VGA_WIDTH + cx] = ve(c, col);
    if (++cx >= VGA_WIDTH) {
        cx = 0;
        cy++;
    }
    vga_move_cursor(cx, cy);
}
void vga_puts(const char* s) {
    while (*s) vga_putc(*s++);
}
void vga_init(void) {
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_clear();
}

// --- シリアル（COM1）デバッグ ---
#define COM1 0x3F8
static inline void serial_init(void) {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}
static inline void serial_putc(char c) {
    while (!(inb(COM1 + 5) & 0x20)) {
    }
    outb(COM1 + 0, (uint8_t)c);
}
static inline void serial_puts(const char* s) {
    while (*s) {
        if (*s == '\n')
            serial_putc('\r');
        serial_putc(*s++);
    }
}
static inline void serial_puthex(uint32_t v) {
    const char* h = "0123456789ABCDEF";
    for (int i = 7; i >= 0; i--) serial_putc(h[(v >> (i * 4)) & 0xF]);
}

// --- Day08: コンテキストスイッチ ---
typedef struct thread {
    uint32_t stack[1024];
    uint32_t esp;
    int row;
} thread_t;
extern void context_switch(uint32_t** old_esp, uint32_t* new_esp);

static void init_stack(thread_t* t, void (*func)(void)) {
    uint32_t* sp = &t->stack[1024];

    // 正しいスレッド初期化 - switch_contextと互換性のあるスタックレイアウト
    // switch_context で復元される順序に合わせて積む
    *--sp =
        (uint32_t)func;  // 関数アドレス（最初のスレッド実行時にret先になる）
    // Day08 では割り込みインフラ未整備のため IF=0 にする（予約ビットは1）
    // EFLAGS = 0x00000002（IF=0, 必須予約ビット=1）
    *--sp = 0x00000002;  // EFLAGS（IF=0, reserved bit=1）
    *--sp = 0;           // EBP
    *--sp = 0;           // EDI
    *--sp = 0;           // ESI
    *--sp = 0;           // EDX
    *--sp = 0;           // ECX
    *--sp = 0;           // EBX
    *--sp = 0;           // EAX

    t->esp = (uint32_t)sp;
}

static thread_t th1, th2;
static uint32_t* current_esp = 0;

static void threadA(void) {
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    // 一度だけ表示してアイドル（Day08は割り込み未使用のため、連続描画はチラつきやすい）
    vga_move_cursor(0, 10);
    vga_puts("Thread A running...");
    serial_puts("Thread A running\n");
    for (;;) {
        __asm__ volatile("nop");
    }
}
static void threadB(void) {
    // Day08ではここには来ない
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_move_cursor(0, 11);
    vga_puts("Thread B running...");
    serial_puts("[This is NOT printed] Thread B running\n");
    for (;;) {
        __asm__ volatile("nop");
    }
}

void kmain(void) {
    serial_init();
    serial_puts("KMAIN begin\n");
    vga_init();
    vga_puts("Day 08: Context Switch demo\n");
    th1.row = 10;
    th2.row = 11;
    init_stack(&th1, threadA);
    init_stack(&th2, threadB);
    serial_puts("SWITCH to th1 esp=");
    serial_puthex(th1.esp);
    serial_puts("\n");
    // 最初のスレッドへ切替
    context_switch(&current_esp, (uint32_t*)th1.esp);
    serial_puts("RETURNED unexpectedly\n");
    for (;;) {
        __asm__ volatile("hlt");
    }
}
```

### 実装のポイント

**アセンブリ実装の要点**

-   **C シンボル**: `void context_switch(uint32_t** old_esp, uint32_t* new_esp);`
-   **退避順序（保存）**: `pushfd` → `push ebp` → `push edi` → `push esi` → `push edx` → `push ecx` → `push ebx` → `push eax`
-   **ESP 切替**: `mov [old_esp], esp` → `mov esp, new_esp`
-   **復元順序**: `pop eax` → `pop ebx` → `pop ecx` → `pop edx` → `pop esi` → `pop edi` → `pop ebp` → `popfd` → `ret`
-   **重要**: 保存と復元の順序が逆順になっている（スタックの特性）

**初期スタック設計**

-   **初期化関数**: `init_stack(thread_t* t, void (*func)(void))`
-   **積む順序**: 関数アドレス → EFLAGS → EBP → EDI → ESI → EDX → ECX → EBX → EAX
-   **EFLAGS 設定**: Day 08 では割り込み未整備のため`0x00000002`（IF=0）
-   **ESP 設定**: 初期化完了後、最後に積んだ値のアドレスを`t->esp`に保存

**呼び出し方法**

-   **関数呼び出し**: `context_switch(&prev->esp, (uint32_t*)next->esp);`
-   **引数説明**:
-   `old_esp`: 現在の ESP を保存する場所のポインタ
-   `new_esp`: 切り替え先スレッドの ESP 値
-   **動作確認**: スレッド開始後は表示＋`nop`ループ（プリエンプション無し）

## トラブルシューティング

-   **切替後に戻らない**: 初期スタックの順序/値を見直す。特に EFLAGS と関数アドレスの位置
-   **システムが破綻する**: pushfd/push と popfd/pop の順序を一致させる
-   **割り込み関連の問題**: Day08 では IF=0（割り込み無効）で動作させる

## 理解度チェック

1.  `old_esp` へ「退避後」の ESP を書き戻す理由は？
2.  初期スタックで何をどの順に積む？
3.  なぜ保存順序と復元順序が逆になる？

## 次のステップ

Day 09 でタイマ割り込みと連動させ、ラウンドロビンのプリエンプティブスケジューラを実装します。
