# Day 10: スリープ/タイミング 💤

## 本日のゴール
タイマー割り込みを使ってsleep()関数を実装し、スレッドのブロッキングとウェイクアップの仕組みを実現する。

## 背景
Day9でプリエンプティブスケジューリングを実装したが、スレッドが自発的にCPUを明け渡す手段がなかった。本日はタイマーベースのスリープ機能を実装し、スレッドが指定時間待機してCPUを効率的に利用できるようにする。

## 新しい概念
- **ブロッキング**: スレッドが何らかの理由（タイマー、I/O等）で実行を停止し、READYリストからBLOCKEDリストに移行すること。
- **ウェイクアップ**: 待機条件が満たされたスレッドをBLOCKEDリストからREADYリストに戻すこと。

## 学習内容

- スレッド状態管理（READY/RUNNING/BLOCKED）
- タイマーベースの待機機能（sleep）
- ブロック理由の分類（BLOCK_REASON_TIMER等）
- 時刻順ソートされたブロックリスト
- アイドルスレッドによるCPU効率化
- 構造化されたカーネルコンテキスト管理

## タスクリスト
- [ ] スレッド状態をREADY/RUNNING/BLOCKEDに拡張する
- [ ] BLOCKEDリストを時刻順に管理するデータ構造を実装する
- [ ] sleep()関数を実装し、スレッドをBLOCKED状態にする
- [ ] タイマー割り込みハンドラでウェイクアップチェックを行う
- [ ] 起床時刻の来たスレッドをREADYリストに戻す
- [ ] アイドルスレッドを実装してCPUを効率的に休止する
- [ ] 複数のデモスレッドを作成し、異なるスリープ時間で動作を確認する
- [ ] QEMUで動作確認し、スレッドのブロッキングとウェイクアップを確認する

## 構成

```
boot/boot.s, boot/kernel_entry.s
boot/interrupt.s        # タイマー割り込みハンドラ
boot/context_switch.s   # コンテキストスイッチ + initial_context_switch
io.h, vga.h
kernel.c                # スリープ機能付きスケジューラ
Makefile
```

## スケジューラの構造

Day 10では、スリープ機能を支えるために、構造化されたスケジューラを採用しています。

-   **`kernel_context_t`による状態管理**: カーネルの状態（現在実行中のスレッド、実行可能リスト、待機中リストなど）は`kernel_context_t`構造体で一元管理されます。これにより、システム全体の見通しが良くなります。

-   **2つのスレッドリスト**:
    -   **READYリスト**: 実行可能なスレッドが管理される循環リストです。スケジューラはここから次のスレッドを選びます。
    -   **BLOCKEDリスト**: `sleep`などで待機中のスレッドが管理されるリストです。タイマー割り込みのたびに、起床時刻が来たスレッドがいないかチェックされます。

-   **`idle_thread`の役割**: システムには、常に`idle_thread`という特別なスレッドが存在します。これは、他に実行すべきスレッドがない場合にCPUを`hlt`命令で休止させるためのものです。これにより、実行可能なスレッドが一つもない状況でもスケジューラが安全に動作し、CPUの無駄な消費を防ぎます。

-   **汎用的なブロッキング機構**: `block_current_thread`関数は、スレッドを特定の理由（`block_reason_t`）で待機させるための汎用的な仕組みです。Day 10では、タイマー待機（`BLOCK_REASON_TIMER`）のためにこれを使用します。

## 実装ガイド

### スリープ機能のフロー

```
🛌 sleep(ticks) 呼び出し
     ↓
📤 remove_from_ready_list()     # READYリストから削除
     ↓
🔒 THREAD_BLOCKED状態に変更
     ↓
⏰ wake_up_tick = current_ticks + ticks
     ↓
📋 BLOCKEDリストに時刻順で挿入
     ↓
🔄 schedule()                   # 他のスレッドに切替
```

### タイマー割り込み処理フロー

```
🕒 タイマー割り込み (IRQ0)
     ↓
✅ eoi_master()                 # EOI送信
     ↓
📈 system_ticks++               # システム時刻更新
     ↓
🔍 check_and_wake_timer_threads() # 起床時刻チェック
     ↓
📤 BLOCKEDリスト → READYリストに移動
     ↓
🔄 schedule()                   # 必要時にスレッド切替
```

### 完全実装コード

**データ構造定義**

```c
// スレッド状態
typedef enum {
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
} thread_state_t;

// ブロック理由
typedef enum {
    BLOCK_REASON_NONE,
    BLOCK_REASON_TIMER,  // スリープ待機
} block_reason_t;

// スレッド制御ブロック（TCB）
typedef struct thread {
    uint32_t stack[1024];        // 4KBスタック
    uint32_t esp;                // スタックポインタ

    thread_state_t state;        // スレッド状態
    block_reason_t block_reason; // ブロック理由

    uint32_t counter;            // カウンタ（デモ用）
    uint32_t delay_ticks;        // スリープ間隔
    uint32_t last_tick;          // 最終実行時刻
    uint32_t wake_up_tick;       // 起床予定時刻

    int display_row;             // 表示行

    struct thread* next_ready;   // READYリスト用
    struct thread* next_blocked; // BLOCKEDリスト用
} thread_t;

// カーネルコンテキスト
typedef struct {
    thread_t* current_thread;     // 現在実行中のスレッド
    thread_t* ready_thread_list;  // READY循環リスト
    thread_t* blocked_thread_list; // BLOCKED時刻順リスト
    volatile uint32_t system_ticks; // システム時刻
    uint32_t scheduler_lock_count;  // スケジューラロック
} kernel_context_t;

static kernel_context_t k_context = {0};
```

**sleep機能の実装**

```c
// スリープ関数
static void sleep(uint32_t ticks) {
    if (ticks == 0) {
        return;
    }

    if (ticks > MAX_COUNTER_VALUE) {
        ticks = MAX_COUNTER_VALUE;
    }

    if (!get_current_thread()) {
        return;
    }

    // 起床時刻を計算してブロック
    uint32_t wake_up_time = get_system_ticks() + ticks;
    block_current_thread(BLOCK_REASON_TIMER, wake_up_time);

    // 他のスレッドに制御を移す
    schedule();
}

// 汎用ブロック関数
void block_current_thread(block_reason_t reason, uint32_t data) {
    asm volatile("cli");

    thread_t* thread = get_current_thread();
    if (!thread) {
        asm volatile("sti");
        return;
    }

    // READYリストから削除
    remove_from_ready_list(thread);

    // ブロック状態に設定
    thread->state = THREAD_BLOCKED;
    thread->block_reason = reason;
    thread->next_blocked = NULL;

    kernel_context_t* ctx = get_kernel_context();

    if (reason == BLOCK_REASON_TIMER) {
        thread->wake_up_tick = data;

        // BLOCKEDリストに時刻順で挿入
        if (!ctx->blocked_thread_list ||
            thread->wake_up_tick < ctx->blocked_thread_list->wake_up_tick) {
            // リストの先頭に挿入
            thread->next_blocked = ctx->blocked_thread_list;
            ctx->blocked_thread_list = thread;
        } else {
            // 適切な位置に挿入（時刻順ソート）
            thread_t* current = ctx->blocked_thread_list;
            while (current->next_blocked &&
                   current->next_blocked->wake_up_tick <= thread->wake_up_tick) {
                current = current->next_blocked;
            }
            thread->next_blocked = current->next_blocked;
            current->next_blocked = thread;
        }
    }

    asm volatile("sti");
}
```

**タイマー割り込み処理**

```c
// タイマー割り込みハンドラ
void timer_handler_c(void) {
    // 先にEOI送信（重要）
    eoi_master();

    kernel_context_t* ctx = get_kernel_context();
    ctx->system_ticks++;

    // ブロック中スレッドのウェイクアップチェック
    check_and_wake_timer_threads();

    // タイムスライス判定
    if ((ctx->system_ticks - last_slice_tick) >= slice_ticks) {
        last_slice_tick = ctx->system_ticks;
        schedule();
    }
}

// 起床時刻チェック＆ウェイクアップ
static void check_and_wake_timer_threads(void) {
    kernel_context_t* ctx = get_kernel_context();
    uint32_t current_ticks = ctx->system_ticks;

    while (ctx->blocked_thread_list &&
           ctx->blocked_thread_list->wake_up_tick <= current_ticks) {

        thread_t* thread_to_wake = ctx->blocked_thread_list;
        ctx->blocked_thread_list = thread_to_wake->next_blocked;

        // READYリストに復帰
        unblock_and_requeue_thread(thread_to_wake);
    }
}

// スレッドをREADY状態に復帰
static void unblock_and_requeue_thread(thread_t* thread) {
    if (!thread) return;

    thread->state = THREAD_READY;
    thread->block_reason = BLOCK_REASON_NONE;
    thread->next_blocked = NULL;

    // READYリストに追加
    add_thread_to_ready_list(thread);
}
```

**スケジューラの実装**

```c
// メインスケジューラ
static void schedule(void) {
    if (is_scheduler_locked()) {
        return;
    }

    acquire_scheduler_lock();

    kernel_context_t* ctx = get_kernel_context();

    if (!ctx->ready_thread_list) {
        handle_blocked_thread_scheduling();
        release_scheduler_lock();
        return;
    }

    if (!ctx->current_thread) {
        handle_initial_thread_selection();
        release_scheduler_lock();
        return;
    }

    perform_thread_switch();
    release_scheduler_lock();
}

// スレッド切替の実行
static void perform_thread_switch(void) {
    kernel_context_t* ctx = get_kernel_context();
    thread_t* current = ctx->current_thread;
    thread_t* next = ctx->ready_thread_list;

    if (current == next) {
        return; // 同じスレッド、切替不要
    }

    // READYリストをローテーション
    ctx->ready_thread_list = next->next_ready;
    ctx->current_thread = next;
    next->state = THREAD_RUNNING;

    if (current && current->state == THREAD_RUNNING) {
        current->state = THREAD_READY;
    }

    // コンテキストスイッチ実行
    context_switch(&current->esp, next->esp);
}
```

**デモスレッド**

```c
// アイドルスレッド
static void idle_thread(void) {
    for (;;) {
        asm volatile("hlt");  // CPUを休止
    }
}

// デモスレッドA（50ms間隔）
static void threadA(void) {
    for (;;) {
        self->counter++;
        vga_move_cursor(0, self->display_row);
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        vga_puts("Fast Thread A: ");
        vga_putnum(self->counter);
        sleep(5);  // 50ms待機
    }
}

// デモスレッドB（100ms間隔）
static void threadB(void) {
    for (;;) {
        self->counter++;
        vga_move_cursor(0, self->display_row);
        vga_set_color(VGA_LIGHT_BLUE, VGA_BLACK);
        vga_puts("Med Thread B: ");
        vga_putnum(self->counter);
        sleep(10); // 100ms待機
    }
}

// デモスレッドC（200ms間隔）
static void threadC(void) {
    for (;;) {
        self->counter++;
        vga_move_cursor(0, self->display_row);
        vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
        vga_puts("Slow Thread C: ");
        vga_putnum(self->counter);
        sleep(20); // 200ms待機
    }
}

// デモスレッドD（500ms間隔）
static void threadD(void) {
    for (;;) {
        self->counter++;
        vga_move_cursor(0, self->display_row);
        vga_set_color(VGA_LIGHT_MAGENTA, VGA_BLACK);
        vga_puts("Very Slow D: ");
        vga_putnum(self->counter);
        sleep(50); // 500ms待機
    }
}
```

## 実装の要点

**sleep(ticks) 関数の流れ:**
1.  現在のスレッドをREADYリストから削除する。
2.  スレッドの状態を`THREAD_BLOCKED`に、理由を`BLOCK_REASON_TIMER`に設定する。
3.  起床時刻（`wake_up_tick`）を計算し、BLOCKEDリストに時刻順で挿入する。
4.  `schedule()`を呼び出して、次の実行可能なスレッドに切り替える。

**タイマー割り込みハンドラ (timer_handler_c) の役割:**
1.  システムティック (`system_ticks`) をインクリメントする。
2.  BLOCKEDリストをチェックし、起床時刻を迎えたスレッドをREADYリストに戻す。
3.  タイムスライスが経過していれば、`schedule()`を呼び出してプリエンプティブな切り替えを行う。

**スケジューラ (schedule) の動作:**
-   常にREADYリストから次のスレッドを選択します。
-   READYリストが空の場合（`idle_thread`のみの場合など）でも、`idle_thread`が選択されるため、システムは停止しません。

**重要な設計パターン:**
- **時刻順ソート**: BLOCKEDリストは起床時刻の早い順にソート
- **アイドルスレッド**: CPUを効率的に休止させる専用スレッド
- **スケジューラロック**: 割り込み中の再入を防ぐロック機構
- **汎用ブロック機構**: 将来のI/O待ちにも対応可能な設計

## トラブルシューティング

-   **「スリープ後に切り替わらない」**
    -   READYリストからの削除、BLOCKEDリストへの挿入ロジックが正しいか確認する。特にリストの先頭や末尾を操作する場合に注意が必要です。
-   **「タイマーが止まる」**
    -   割り込みハンドラの最初でPICにEOI（End of Interrupt）を送信しているか確認する。
-   **「すべてのスレッドがブロックされてハング」**
    -   idle_threadが正しく動作しているか、READYリストに残っているか確認する。
-   **「起床時刻が正しくない」**
    -   BLOCKEDリストの時刻順ソートが正しく動作しているか確認する。

## 理解度チェック

1. なぜBLOCKEDリストを時刻順にソートする必要がある？
2. idle_threadがない場合、システムはどうなる？
3. スケジューラロックが必要な理由は？

## 次のステップ

Day 11 ではキーボード入力を導入し、このブロッキング機構をI/O待ちにも応用します。