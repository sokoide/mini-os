# Mini OS 詳細アーキテクチャガイド

> 📎 **参考資料（LLM 比較生成）**: 本ファイルは Gemini により生成されたアーキテクチャ解説の比較検証用資料です。公式の正本は [`../ARCHITECTURE_ja.md`](../ARCHITECTURE_ja.md) を参照してください。内容は生成時点（day99 基準）のものです。

本ドキュメントは、Mini OS の設計、内部構造、および動作原理を、維持管理（メンテナ）向けに詳細に解説するものです。

## 1. システム全体俯瞰

Mini OS は、モノリシックな構造を持つ 32-bit x86 プロテクトモード OS です。ハードウェア抽象化、割り込み管理、スレッドスケジューリング、および基本デバイスドライバ（VGA, キーボード, シリアル）を提供します。

### 1.1 モジュール構成図

```mermaid
graph TB
    subgraph "アプリケーション層 (Application)"
        APP[スレッド A/B/C<br/>ユーザーロジック]
    end

    subgraph "サービス層 (OS Service)"
        LIBC[libc 相当機能<br/>vga_puts, itoa, read_line]
        IO[入出力 API<br/>getchar, sleep]
    end

    subgraph "カーネル層 (Kernel)"
        SCHED[スケジューラ<br/>ラウンドロビン]
        THREAD[スレッド管理<br/>TCB, 状態遷移]
        INT[割り込みシステム<br/>IDT, ISR, PIC]
        CONTEXT[コンテキストスイッチ<br/>レジスタ保存/復元]
    end

    subgraph "ハードウェア抽象化層 (HAL/Drivers)"
        VGA[VGA ドライバ<br/>0xB8000]
        KBD[キーボードドライバ<br/>PS/2]
        TIMER[タイマドライバ<br/>PIT 8254]
        SERIAL[シリアルドライバ<br/>UART 8250]
    end

    subgraph "ハードウェア層 (Hardware)"
        CPU[x86-32 CPU]
        HW_INT[PIC 8259A]
    end

    APP --> LIBC
    LIBC --> IO
    IO --> SCHED
    SCHED --> THREAD
    THREAD --> CONTEXT
    CONTEXT --> INT
    INT --> HW_INT
    IO --> HAL
    HAL --> HW_INT
```

## 2. 起動シーケンス (Boot Sequence)

BIOS からカーネル実行までの流れ。リアルモードから 32bit プロテクトモードへの移行が含まれます。

```mermaid
sequenceDiagram
    participant BIOS as BIOS (ROM)
    participant MBR as MBR (boot.s)
    participant KERN as Kernel Entry (kernel_entry.s)
    participant MAIN as C Kernel (kernel.c)

    BIOS->>MBR: セクタ0を 0x7C00 にロード
    Note over MBR: 16-bit リアルモード
    MBR->>MBR: A20ライン有効化
    MBR->>MBR: GDT ロード
    MBR->>MBR: CR0.PE=1 (プロテクトモード移行)
    MBR->>KERN: セグメントジャンプ (0x08:kernel)
    Note over KERN: 32-bit プロテクトモード
    KERN->>KERN: BSS 領域のゼロクリア
    KERN->>KERN: スタック設定 (0x90000)
    KERN->>MAIN: kernel_main() 呼び出し
    MAIN->>MAIN: システム初期化 & スレッド生成
```

## 3. スレッド管理とスケジューリング

### 3.1 スレッド状態遷移図 (Thread State Transition)

Mini OS は READY, RUNNING, BLOCKED の 3 状態を持ちます。

```mermaid
stateDiagram-v2
    [*] --> READY: create_thread()
    READY --> RUNNING: schedule() (コンテキストスイッチ)
    RUNNING --> READY: タイマ割り込み (先制)
    RUNNING --> BLOCKED: sleep() / getchar() (自発的ブロック)
    BLOCKED --> READY: タイマ満了 / キー入力 (割り込みによる起床)
```

### 3.2 スレッド制御ブロック (TCB)

`thread_t` 構造体はスレッドの全状態を保持します。

```mermaid
classDiagram
    class thread_t {
        +uint32_t stack[1024]
        +thread_state_t state
        +block_reason_t block_reason
        +uint32_t wake_up_tick
        +uint32_t esp
        +uint32_t counter
        +uint32_t delay_ticks
        +int display_row
        +thread_t* next_ready
        +thread_t* next_blocked
    }
```

## 4. 割り込みとコンテキストスイッチ

タイマ割り込み（IRQ0）を契機としたコンテキストスイッチの流れ。

```mermaid
sequenceDiagram
    participant HW as PIT (Hardware)
    participant ISR as Assembly ISR (interrupt.s)
    participant C_HDL as C Handler (kernel.c)
    participant SCHED as Scheduler
    participant TH_OLD as 旧スレッド
    participant TH_NEW as 新スレッド

    HW->>ISR: IRQ0 (10ms)
    ISR->>ISR: pusha (全汎用レジスタ保存)
    ISR->>C_HDL: timer_handler_c()
    C_HDL->>C_HDL: system_ticks++
    C_HDL->>C_HDL: 起床チェック (Blocked List)
    C_HDL->>SCHED: schedule()
    SCHED->>SCHED: 次のスレッドを選択
    SCHED->>ISR: switch_context(&old->esp, new->esp)
    Note over ISR: スタックの切り替え
    ISR->>ISR: popa (新スレッドのレジスタ復元)
    ISR->>TH_NEW: iret (新スレッド実行再開)
```

## 5. キーボード入力システム

PS/2 キーボードからの入力は、割り込み駆動と SPSC リングバッファを使用してスレッドセーフに処理されます。

```mermaid
graph LR
    subgraph "割り込みコンテキスト (Producer)"
        K_HW[キーボード] --> |IRQ1| K_ISR[ISR]
        K_ISR --> |Scancode| K_BUF[Ring Buffer]
        K_ISR --> |Unblock| K_WAKE[Keyboard Wait List]
    end

    subgraph "スレッドコンテキスト (Consumer)"
        K_BUF --> |getchar| APP[アプリスレッド]
        APP --> |Wait| K_WAIT[Blocked]
    end

    K_WAKE -.-> |Move to Ready| K_WAIT
```

## 6. メモリマップ (Memory Layout)

Mini OS はセグメンテーション（フラットモデル）を使用し、ページングは未実装です。

| アドレス範囲   | 用途         | 説明                             |
| :------------- | :----------- | :------------------------------- |
| `0x00007C00`   | MBR          | ブートローダのロード位置         |
| `0x00010000`   | Kernel       | カーネルイメージ実行領域         |
| `0x00090000`   | Kernel Stack | カーネル初期化用スタック         |
| `0x000B8000`   | VGA Buffer   | テキストモード表示メモリ (80x25) |
| `Thread Stack` | 各スレッド   | 4KB 単位の独立したスタック       |

## 7. 維持管理上の注意点 (Maintainer Notes)

1. **スタックアライメント**: スレッド生成時の初期スタックレイアウトは `interrupt.s` の `switch_context` と厳密に一致させる必要があります。
2. **割り込み禁止**: `schedule()` やリスト操作などのクリティカルセクションでは、割り込み状態の管理に注意してください。
3. **IOポート**: ハードウェア制御には `inb`/`outb` 系のインラインアセンブリを使用しています。
4. **テスト**: 変更後は `make test` を実行し、回帰テストをパスすることを確認してください。
