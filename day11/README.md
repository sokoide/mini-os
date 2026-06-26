# Day 11: キーボード入力システム（PS/2 + リングバッファ）⌨️

## 本日のゴール

PS/2 キーボードからの非同期入力を処理し、リングバッファとブロッキング I/O を実装する。

## 背景

Day10 でスリープ機能を追加したが、OS 開発ではユーザー入力も重要な要素。本日は PS/2 キーボードから割り込み駆動で入力を処理し、リングバッファでデータを管理、ブロッキング API で効率的な入力を待機できるシステムを構築する。

## 新しい概念

-   **PS/2**: キーボードと PC を接続するシリアルインターフェース。スキャンコードを送信し、IRQ1 割り込みを発生させる。
-   **リングバッファ**: データを循環的に格納するバッファ。書き込み側と読み出し側が衝突せずに安全にデータをやり取りできる。
-   **ブロッキング I/O**: 入力が得られるまでスレッドを待機状態にする I/O 処理方式。

## 学習内容

-   PS/2 キーボードハードウェアインターフェース
-   スキャンコード → ASCII 文字変換
-   割り込み駆動の非同期入力処理
-   スレッドセーフなリングバッファ実装
-   I/O 待ちのためのブロッキング機構
-   キーボード割り込み（IRQ1）とタイマー割り込み（IRQ0）の統合

## タスクリスト

-   [ ] PS/2 キーボードのハードウェアインターフェースを調査し、スキャンコードを理解する
-   [ ] スキャンコードから ASCII 文字への変換テーブルを実装する
-   [ ] リングバッファを実装し、非同期データの安全な格納を行う
-   [ ] IRQ1 割り込みハンドラを実装し、キーボード入力を処理する
-   [ ] PIC の IRQ1 マスクを解除し、キーボード割り込みを有効化する
-   [ ] キーボード待ちスレッドのブロッキングとウェイクアップを実装する
-   [ ] 警告のないクリーンなコードに修正
-   [ ] QEMU でキーボード入力をテストし、非同期処理を確認する

## 構成

```
boot/boot.s, boot/kernel_entry.s
boot/interrupt.s        # IRQ0(タイマー) + IRQ1(キーボード)
boot/context_switch.s   # コンテキストスイッチ + initial_context_switch
io.h, vga.h
kernel.c                # キーボード入力付きスケジューラ
Makefile
```

## キーボード入力の実装方式

Day 11 では、**割り込み駆動方式**でキーボード入力を実装しています。以下のコンポーネントが連携して動作します：

### アーキテクチャ概要

```
⌨️  キー押下
     ↓
🔌 PS/2コントローラ (スキャンコード生成)
     ↓
⚡ IRQ1割り込み発生
     ↓
🎯 keyboard_handler_c()
     ↓
🔄 スキャンコード → ASCII変換
     ↓
📋 リングバッファに格納
     ↓
🔓 unblock_keyboard_threads()
     ↓
🏃 BLOCKED → READY スレッド復帰
```

### 実装されたコンポーネント

1. **IRQ1 割り込みハンドラ (`keyboard_handler_c`)**

    - PS/2 コントローラからスキャンコードを読み取り
    - ASCII 文字に変換してリングバッファに格納
    - キーボード待ちスレッドを READY 状態に復帰

2. **リングバッファ (`kbuf`)**

    - 128 バイトの循環バッファで非同期入力を管理
    - `kbuf_push`：割り込みハンドラからの文字格納
    - `kbuf_is_empty/full`：バッファ状態の確認

3. **スレッドブロッキング機構**

    - `BLOCK_REASON_KEYBOARD`：キーボード入力待ち
    - `unblock_keyboard_threads`：入力時の一括ウェイクアップ

4. **デモスレッド**
    - `threadA/B`：カウンタ表示とタイマーベースの動作
    - `idle_thread`：全スレッドがブロック時の CPU 休止

## スケジューラとの連携

Day 11 の実装は、Day 10 で導入された堅牢なスケジューリング方式を基盤としています：

-   **`idle_thread`の役割**: 実行可能なスレッドがない場合の CPU 休止
-   **汎用ブロッキング機構**: タイマー待機とキーボード待機の統一的処理
-   **効率的な割り込み処理**: IRQ0（タイマー）と IRQ1（キーボード）の協調動作

## 実装ガイド

### 完全実装コード

**リングバッファ実装**

```c
// キーボードバッファ（リングバッファ）
#define KBUF_SIZE 128
static volatile char kbuf[KBUF_SIZE];
static volatile uint32_t khead = 0, ktail = 0;

// バッファの状態チェック
static inline int kbuf_is_empty(void) {
    return khead == ktail;
}

static inline int kbuf_is_full(void) {
    return ((khead + 1) % KBUF_SIZE) == ktail;
}

// 文字をバッファに追加（割り込みハンドラから呼ばれる）
static void kbuf_push(char c) {
    uint32_t nh = (khead + 1) % KBUF_SIZE;
    if (nh != ktail) {  // バッファが満杯でない場合のみ
        kbuf[khead] = c;
        khead = nh;
    }
}
```

**スキャンコード変換テーブル**

```c
// US配列キーボード用スキャンコード→ASCII変換テーブル
static const char scancode_map[128] = {
    0,   27,   '1',  '2', '3',  '4', '5', '6', '7', '8', '9', '0', '-',
    '=', '', '	', 'q', 'w',  'e', 'r', 't', 'y', 'u', 'i', 'o', 'p',
    '[', ']',  '
', 0,   'a',  's', 'd', 'f', 'g', 'h', 'j', 'k', 'l',
    ';', '\'', '`',  0,   '\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',',
    '.', '/',  0,    '*', 0,    ' ', 0,   0,   0,   0,   0,   0,   0,
    0,   0,    0,    0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,    0,    0,   0,    0,   0,   0,   0,   0,   0,   0,
    // 残りは0で初期化（F1-F10、数字キーパッド等）
    [60 ... 127] = 0
};
```

**キーボード割り込みハンドラ**

```c
// IRQ1: キーボード
void keyboard_handler_c(void) {
    // PIC に割り込み処理完了を通知
    outb(PIC1_CMD, PIC_EOI);

    // キーボードデータの読み取り可能性をチェック
    uint8_t status = inb(PS2_STAT);
    if (!(status & 0x01)) {
        serial_puts("KEYBOARD: Interrupt fired but no data available\n");
        return;
    }

    // スキャンコードを読み取り
    uint8_t scancode = inb(PS2_DATA);

    // キー離す操作は無視（break code）
    if (scancode & 0x80) {
        return;
    }

    // スキャンコードをASCII文字に変換
    char ch = (scancode < 128) ? scancode_map[scancode] : 0;

    if (ch != 0) {
        // バッファに格納
        kbuf_push(ch);

        // 即座にVGAとシリアルの両方に出力
        __asm__ volatile("cli");

        // VGA出力 - "Keyboard Input:" の右側に表示
        vga_move_cursor(16, 6);
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        vga_putc(ch);
        vga_putc(' ');  // 古い文字をクリア

        // シリアル出力
        serial_putc(ch);

        __asm__ volatile("sti");

        // デバッグ出力（day12_completed形式）
        serial_puts("KEY: ");
        serial_putc(ch);
        serial_puts(" (");
        serial_puthex(scancode);
        serial_puts(")\n");

        // 入力待ちスレッドをREADYへ（もしあれば）
        unblock_keyboard_threads();
    }
}
```

**キーボードスレッドのアンブロック**

```c
// キーボード待ちスレッドをすべてREADY状態に戻す
static void unblock_keyboard_threads(void) {
    asm volatile("cli");
    kernel_context_t* ctx = get_kernel_context();
    thread_t* current = ctx->blocked_thread_list;
    thread_t* prev = NULL;

    while (current) {
        thread_t* next = current->next_blocked;
        if (current->block_reason == BLOCK_REASON_KEYBOARD) {
            // BLOCKEDリストから削除
            if (prev) {
                prev->next_blocked = current->next_blocked;
            } else {
                ctx->blocked_thread_list = current->next_blocked;
            }

            // READYリストに復帰
            current->state = THREAD_READY;
            current->block_reason = BLOCK_REASON_NONE;
            current->next_blocked = NULL;
            add_thread_to_ready_list(current);
        } else {
            prev = current;
        }
        current = next;
    }
    asm volatile("sti");
}
```

**デモスレッド実装**

```c
// アイドルスレッド
static void idle_thread(void) {
    for (;;) {
        asm volatile("hlt");  // CPUを休止
    }
}

// スレッドA（高速カウンタ）
static void threadA(void) {
    serial_puts("threadA start
");
    for (;;) {
        thread_t* self = get_current_thread();
        self->counter++;
        __asm__ volatile("cli");
        vga_move_cursor(0, 2);
        vga_set_color(VGA_YELLOW, VGA_BLACK);
        vga_puts("A: ");
        vga_putnum(self->counter);
        vga_puts("        ");  // 前の数値をクリア
        __asm__ volatile("sti");

        if ((self->counter & 0x1FF) == 0)
            serial_puts("threadA alive
");

        sleep(50);  // 500ms待機
    }
}

// スレッドB（中速カウンタ）
static void threadB(void) {
    serial_puts("threadB start
");
    for (;;) {
        thread_t* self = get_current_thread();
        self->counter++;
        __asm__ volatile("cli");
        vga_move_cursor(0, 3);
        vga_set_color(VGA_CYAN, VGA_BLACK);
        vga_puts("B: ");
        vga_putnum(self->counter);
        vga_puts("        ");  // 前の数値をクリア
        __asm__ volatile("sti");

        if ((self->counter & 0x1FF) == 0)
            serial_puts("threadB alive
");

        sleep(75);  // 750ms待機
    }
}
```

**PIC 設定と IDT 初期化**

```c
// PICの再マッピング（IRQ0-15 → 32-47）
static void remap_pic(void) {
    uint8_t a1 = inb(PIC1_DATA), a2 = inb(PIC2_DATA);

    outb(PIC1_CMD, 0x11);   // ICW1: 8086モード、カスケード
    outb(PIC2_CMD, 0x11);
    outb(PIC1_DATA, 0x20);  // ICW2: マスタPIC → 32-39
    outb(PIC2_DATA, 0x28);  // ICW2: スレーブPIC → 40-47
    outb(PIC1_DATA, 0x04);  // ICW3: スレーブはIRQ2
    outb(PIC2_DATA, 0x02);  // ICW3: カスケードID=2
    outb(PIC1_DATA, 0x01);  // ICW4: 8086モード
    outb(PIC2_DATA, 0x01);

    outb(PIC1_DATA, a1);    // 元のマスクに戻す
    outb(PIC2_DATA, a2);
}

// IDT設定
static void idt_init(void) {
    // 全エントリを0で初期化
    for (int i = 0; i < 256; i++)
        set_gate(i, 0);

    // 例外ハンドラ
    set_gate(0, (uint32_t)isr0);   // Divide by zero
    set_gate(3, (uint32_t)isr3);   // Breakpoint
    set_gate(6, (uint32_t)isr6);   // Invalid opcode
    set_gate(13, (uint32_t)isr13); // General protection fault
    set_gate(14, (uint32_t)isr14); // Page fault

    // 割り込みハンドラ
    set_gate(32, (uint32_t)timer_interrupt_handler);    // IRQ0
    set_gate(33, (uint32_t)keyboard_interrupt_handler); // IRQ1

    // IDTRレジスタに設定
    idtr.limit = sizeof(idt) - 1;
    idtr.base = (uint32_t)&idt[0];
    lidt(&idtr);
}

// メイン初期化
void kmain(void) {
    // 基本初期化
    serial_init();
    vga_init();
    init_kernel_context();

    // 割り込みシステム初期化
    remap_pic();
    set_masks(0xFC, 0xFF);  // IRQ0,1のみ有効（0xFC = 11111100）
    idt_init();
    init_pit_100hz();

    // PS/2キーボード初期化
    ps2_keyboard_init();

    serial_puts("PIC/IDT/PIT/PS2 ready, enabling interrupts
");
    __asm__ volatile("sti");

    // スレッド作成
    thread_t *th1, *th2, *tidle;
    create_thread(threadA, 50, 2, &th1);
    create_thread(threadB, 75, 3, &th2);
    create_thread(idle_thread, 1, 0, &tidle);

    serial_puts("Starting scheduler...");
    schedule();
}
```

## 実装の要点

**リングバッファ設計:**

-   `head`（書き込み）と`tail`（読み取り）の 2 ポインタ管理
-   割り込みハンドラとアプリケーション間の安全な非同期転送

**IRQ1 割り込みハンドラの処理順序:**

1. PIC に EOI 送信（最重要：次の割り込み受信可能化）
2. PS/2 ステータス確認（データ準備完了チェック）
3. スキャンコード読み取り（ポート 0x60）
4. ASCII 文字変換（スキャンコードテーブル使用）
5. リングバッファに格納
6. デバッグ出力（VGA + シリアル）
7. ブロック中スレッドのウェイクアップ

## トラブルシューティング

-   **入力が全く来ない**

    -   PIC の IRQ1 マスク（bit 1）解除確認
    -   IDT の 33 番エントリ（IRQ1）設定確認

-   **システムハング**
    -   EOI 送信タイミング確認（ハンドラ最初で実行）
    -   割り込み中のスケジューラ再入防止

## 理解度チェック

1. なぜ`threadKpoll`を削除したのか？
2. 割り込み駆動方式とポーリング方式の違いは？
3. リングバッファが必要な理由は？
4. `unblock_keyboard_threads`の役割は？

## 次のステップ

Day 12 では、これまでの機能を統合し、モジュール化された構造に改良します。また、より高度なキーボード機能（文字列入力、バックスペース処理）も実装します。
