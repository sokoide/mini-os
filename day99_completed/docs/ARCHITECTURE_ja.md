# Day99 完成版アーキテクチャガイド

## 概要

Day99完成版は、x86プロテクトモードで動作する教育用マルチスレッドオペレーティングシステムです。タイマベースの先制型スケジューリングとキーボード入力をサポートする、実用的なOS実装です。

## システム構成

```
Day99 システムアーキテクチャ
├── ブート層 (Boot Layer)
│   ├── boot.s           - MBRブートローダ (16bit→32bit移行)
│   ├── kernel_entry.s   - カーネルエントリ (BSS初期化)
│   └── interrupt.s      - 割り込みハンドラ & コンテキストスイッチ
├── ハードウェア制御層 (Hardware Layer)
│   ├── VGA制御          - テキストモード表示
│   ├── PIC/PIT制御      - 割り込みコントローラ/タイマー
│   ├── キーボード制御    - PS/2キーボードドライバ
│   └── シリアル制御     - COM1デバッグ出力
├── カーネル層 (Kernel Layer)
│   ├── kernel_context_t - 統合システム状態管理
│   ├── スケジューラ     - 先制型ラウンドロビン
│   ├── スレッド管理     - TCB、状態遷移、ブロック管理
│   └── メモリ管理       - 固定メモリレイアウト
└── アプリケーション層 (Application Layer)
    ├── threadA/B/C      - サンプルスレッド
    └── キーボード入力    - インタラクティブI/O
```

## メモリレイアウト

| アドレス | 用途 | サイズ | 説明 |
|---------|------|--------|------|
| 0x007C00 | ブートセクタ | 512B | MBRブートローダ実行位置 |
| 0x010000 | カーネル開始 | ~17KB | カーネルコード・データ |
| 0x090000 | カーネルスタック | 4KB | メインカーネルスタック |
| 0x0B8000 | VGAメモリ | 4KB | テキストモード画面バッファ |
| 各スレッド | スレッドスタック | 4KB×N | 各スレッド専用スタック |

## 核心データ構造

### kernel_context_t - カーネル状態統合管理
```c
typedef struct {
    thread_t* current_thread;           // 現在実行中スレッド
    thread_t* ready_thread_list;        // 実行可能スレッドキュー
    thread_t* blocked_thread_list;      // ブロック中スレッドリスト
    uint32_t system_ticks;              // システム経過時間
    volatile int scheduler_lock_count;  // スケジューラロック
} kernel_context_t;
```

### thread_t - スレッド制御ブロック (TCB)
```c
typedef struct thread {
    uint32_t stack[1024];        // 4KBスタック領域
    thread_state_t state;        // READY/RUNNING/BLOCKED
    block_reason_t block_reason; // ブロック理由
    uint32_t wake_up_tick;       // 起床予定時刻
    uint32_t esp;                // スタックポインタ
    // その他フィールド...
} thread_t;
```

## スレッド状態遷移

```
    create_thread()
         │
         ▼
    ┌─────────┐   schedule()   ┌──────────┐
    │ READY   │ ─────────────→ │ RUNNING  │
    │         │                │          │
    └─────────┘                └──────────┘
         ▲                          │
         │                          │ block_current_thread()
         │                          ▼
         │                     ┌──────────┐
         └─────────────────────│ BLOCKED  │
           unblock_*_threads() │          │
                               └──────────┘
```

### 状態遷移の詳細
- **READY → RUNNING**: スケジューラが選択してコンテキストスイッチ
- **RUNNING → BLOCKED**: `sleep()` やキーボード入力待ちで自発的ブロック
- **BLOCKED → READY**: タイマー満了やキーボード入力で自動復帰
- **RUNNING → READY**: タイマー割り込みによる先制的切り替え

## 割り込みシステム

### PIC設定 (8259A)
- **IRQ0 (Timer)**: INT32にマップ → 10ms周期 (100Hz)
- **IRQ1 (Keyboard)**: INT33にマップ → PS/2キーボード
- **マスタPIC**: 0x20-0x21ポート制御

### 割り込み処理フロー
```
ハードウェア割り込み
    │
    ▼
interrupt.s (アセンブリハンドラ)
    │
    ├─ タイマー → timer_interrupt_handler()
    │               │
    │               ├─ system_ticks++
    │               ├─ スリープ中スレッドの起床チェック
    │               └─ schedule() → コンテキストスイッチ
    │
    └─ キーボード → keyboard_interrupt_handler()
                    │
                    ├─ スキャンコード読み取り
                    ├─ ASCIIバッファに格納
                    └─ unblock_keyboard_threads()
```

## スケジューリング

### アルゴリズム: 先制型ラウンドロビン
- **時間量子**: 10ms (100Hzタイマー)
- **優先度**: なし（全スレッド平等）
- **データ構造**: 循環リンクリスト（READY queue）

### スケジューリング条件
1. **タイマー割り込み**: 強制的な時間切り替え
2. **自発的ブロック**: `sleep()`, `getchar()` 呼び出し時
3. **ブロック解除**: 条件成立によるREADY復帰時

### コンテキストスイッチ詳細
```assembly
; interrupt.s の context_switch 関数
context_switch:
    ; 現在スレッドのレジスタを保存
    push eax
    push ebx
    push ecx
    push edx
    push esi
    push edi
    push ebp
    pushfd              ; EFLAGS保存

    mov [esp+36], esp   ; 旧ESP保存
    mov esp, [esp+40]   ; 新ESP復元

    ; 新スレッドのレジスタを復元
    popfd               ; EFLAGS復元
    pop ebp
    pop edi
    pop esi
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret
```

## ブロッキング機構

### ブロック理由分類
```c
typedef enum {
    BLOCK_REASON_NONE,      // ブロックなし
    BLOCK_REASON_TIMER,     // sleep() 待機
    BLOCK_REASON_KEYBOARD,  // 入力待機
} block_reason_t;
```

### ブロック管理
- **タイマーブロック**: `wake_up_tick` で絶対時刻管理
- **キーボードブロック**: 入力イベントで一括起床
- **ブロックリスト**: 単方向リンクリストで管理

## キーボード入力システム

### ハードウェアレベル
- **PS/2コントローラ**: ポート0x60(データ), 0x64(ステータス)
- **スキャンコード**: Make/Breakコード対応
- **レイアウト**: US配列、Shift修飾キー対応

### ソフトウェアレベル
```c
// 256バイト循環バッファ
static char keyboard_buffer[256];
static volatile int buffer_start, buffer_end;

// 主要API
char getchar(void);                    // 1文字入力
void read_line(char* buffer, int max); // 文字列入力
```

### 入力処理フロー
```
キー押下 → IRQ1 → スキャンコード取得 → ASCII変換
    ↓
循環バッファに格納 → キーボード待機スレッド起床
    ↓
getchar()/read_line() でバッファから読み取り
```

## デバッグ機能

### デュアル出力システム
- **VGA出力**: リアルタイム画面表示
- **シリアル出力**: 外部ログ取得（COM1ポート）

### デバッグAPI
```c
void debug_print(const char* message);     // 統合デバッグ出力
void serial_write_string(const char* str); // シリアル専用
void print_at(int row, int col, ...);      // VGA座標指定
```

## 起動シーケンス

### 1. ブート段階 (boot.s)
```
1. リアルモード (16bit)
2. A20ライン有効化
3. カーネルセクタ読み込み (セクタ2-128)
4. GDT設定、プロテクトモード移行
5. カーネルエントリにジャンプ (0x10000)
```

### 2. カーネル初期化 (kernel_entry.s)
```
1. セグメントレジスタ設定
2. スタックポインタ設定 (0x90000)
3. BSS領域ゼロクリア
4. kernel_main() 呼び出し
```

### 3. システム初期化 (kernel.c)
```c
void kernel_main(void) {
    init_serial();          // シリアルポート初期化
    clear_screen();         // 画面クリア
    show_system_info();     // システム情報表示

    init_interrupts();      // 割り込みシステム初期化
    init_keyboard();        // キーボードドライバ初期化

    // アプリケーションスレッド作成
    create_thread(threadA, 100, 15);  // 1秒間隔
    create_thread(threadB, 150, 16);  // 1.5秒間隔
    create_thread(threadC, 200, 17);  // キーボード入力

    // アイドルループ（割り込み待機）
    while (1) {
        asm volatile("hlt");  // 割り込みまでCPU停止
    }
}
```

## 実行時動作

### 画面表示
```
Day99 アーキテクチャ - 高度なマルチスレッドOS

Thread Information:
  Thread 1: Counter updates every 1.0 second
  Thread 2: Counter updates every 1.5 seconds
  Thread 3: Keyboard input thread

Live Thread Status:
  Thread A: 123        (1秒毎に更新)
  Thread B: 45         (1.5秒毎に更新)
  Thread C: Keyboard Input Demo - Press keys: _

Input: 'a' (1 time)    (キー入力で更新)
```

### シリアルデバッグ出力例
```
KERNEL: Serial port initialized
KERNEL: Screen cleared
KERNEL: System info displayed
KERNEL: About to initialize interrupts
SUCCESS: Thread created successfully
TIMER_1045 ticks=1045
PERFORM_SWITCH
threadA start
KBD: Key pressed 'a'
```

## 技術的特徴

### 優れた設計点
1. **統合状態管理**: `kernel_context_t`による中央集権的状態管理
2. **柔軟なブロック機構**: 理由別ブロック分類とイベント駆動起床
3. **ロバストな割り込み処理**: リエントラント保護とレジスタ完全保存
4. **教育価値**: 段階的な学習に適した分離された機能設計

### 制約事項
- **シングルコア**: SMP未対応
- **32ビットx86**: 64ビット/他アーキテクチャ未対応
- **固定メモリ**: 仮想メモリ・ページング未実装
- **基本I/O**: DMA・高速I/O未対応

## 拡張の指針

この設計は以下の高度な機能への足がかりとして設計されています：

- **仮想メモリシステム**: ページテーブル管理
- **ファイルシステム**: FAT12/16/32サポート
- **ネットワークスタック**: TCP/IP実装
- **マルチプロセシング**: SMP対応
- **GUIシステム**: 基本ウィンドウマネージャ

Day99完成版は、現代的なOS開発の基礎概念を包括的に実装した、実用的な教育用オペレーティングシステムです。