# Mini OS アーキテクチャ詳細解説 (Mermaidチャート付き)

> 📎 **参考資料（LLM 比較生成）**: 本ファイルは GLM により生成されたアーキテクチャ解説（Mermaid チャート付き）の比較検証用資料です。公式の正本は [`../ARCHITECTURE_ja.md`](../ARCHITECTURE_ja.md) を参照してください。内容は生成時点（day99 基準）のものです。

## 📋 概要

このドキュメントは、教育用x86 32-bitマルチスレッドオペレーティングシステム「Mini OS」のアーキテクチャを、Software Engineer向けにMermaidチャートを交えて詳細に解説します。

## 🏗️ 全体システムアーキテクチャ

```mermaid
graph TB
    subgraph "ハードウェア層"
        CPU[x86 CPU]
        RAM[物理メモリ]
        PIC[PIC 8259A]
        PIT[PIT 8254]
        KB[PS/2 Keyboard]
        VGA[VGA Display]
    end

    subgraph "ブート層"
        BIOS[BIOS/UEFI]
        BOOT[Boot Sector<br/>boot.s]
        ENTRY[Kernel Entry<br/>kernel_entry.s]
    end

    subgraph "カーネル層"
        IDT[IDT割り込みテーブル]
        GDT[GDTセグメントテーブル]
        KERNEL[kernel.c]
        SCHED[スケジューラ]
        TCB[スレッド管理]
    end

    subgraph "ドライバ層"
        TIMER[タイマードライバ]
        KBD[キーボードドライバ]
        VGA_DRV[VGAドライバ]
        SERIAL[シリアルドライバ]
    end

    subgraph "システムコール層"
        SLEEP[sleep()]
        GETC[getchar()]
        PRINT[printf()]
    end

    BIOS --> BOOT
    BOOT --> ENTRY
    ENTRY --> KERNEL

    CPU -.->|割り込み| PIC
    PIC -.->|IRQ0/PIT| TIMER
    PIC -.->|IRQ1/KB| KBD
    TIMER --> SCHED
    SCHED --> TCB
    KBD --> TCB

    KERNEL --> VGA_DRV
    KERNEL --> SERIAL
    VGA_DRV --> VGA
    SERIAL --> RAM

    TCB --> SLEEP
    TCB --> GETC
    KERNEL --> PRINT
```

## 🚀 ブートプロセス詳細

### フェーズ1: 16-bitリアルモードブート

```mermaid
sequenceDiagram
    participant BIOS
    participant BootLoader as Boot Sector (0x7C00)
    participant Memory as 物理メモリ
    participant CPU as x86 CPU

    BIOS->>BootLoader: ディスク読み込み<br/>512バイトを0x7C00に
    BootLoader->>Memory: カーネル読み込み<br/>0x10000に一時配置
    BootLoader->>CPU: A20ライン有効化
    BootLoader->>CPU: GDT設定
    BootLoader->>CPU: プロテクトモード移行<br/>CR0.PE=1
    BootLoader->>BootLoader: 32bitセグメントジャンプ<br/>CODE_SEG:0x7C00+start32
```

### フェーズ2: 32-bitプロテクトモード移行

```mermaid
graph LR
    subgraph "リアルモード (16-bit)"
        RM1[0x7C00: Boot Code]
        RM2[1MBメモリ制限]
        RM3[セグメント:オフセット<br/>アドレッシング]
    end

    subgraph "プロテクトモード移行"
        PM1[A20ライン有効化]
        PM2[GDT読み込み]
        PM3[CR0レジスタ設定]
        PM4[リモートジャンプ]
    end

    subgraph "プロテクトモード (32-bit)"
        PM5[フラットメモリ<br/>4GBアクセス可能]
        PM6[カーネル再配置<br/>0x100000へ]
        PM7[スタック設定<br/>0x200000]
    end

    RM1 --> PM1
    PM1 --> PM2
    PM2 --> PM3
    PM3 --> PM4
    PM4 --> PM5
    PM5 --> PM6
    PM6 --> PM7
```

### フェーズ3: カーネルエントリー

```mermaid
graph TD
    subgraph "カーネルロード完了"
        KERNEL[Kernel at 0x100000]
        STACK[Stack at 0x200000]
    end

    subgraph "エントリーポイント処理"
        ENTRY1[_start]
        ENTRY2[BSSクリア]
        ENTRY3[スタック設定]
        ENTRY4[kernel_main呼び出し]
    end

    subgraph "カーネル初期化"
        INIT1[VGA初期化]
        INIT2[シリアル初期化]
        INIT3[IDT/PIC設定]
        INIT4[スレッド生成]
    end

    KERNEL --> ENTRY1
    ENTRY1 --> ENTRY2
    ENTRY2 --> ENTRY3
    ENTRY3 --> ENTRY4
    ENTRY4 --> INIT1
    INIT1 --> INIT2
    INIT2 --> INIT3
    INIT3 --> INIT4
```

## 💾 メモリアーキテクチャ

### 物理メモリ配置

```mermaid
graph TB
    subgraph "物理メモリマップ (4GB)"
        MEM0[0x00000000<br/>IVT/BIOSデータ]
        MEM1[0x00007C00<br/>ブートセクタ]
        MEM2[0x000A0000<br/>VGAメモリ]
        MEM3[0x000B8000<br/>テキストバッファ]
        MEM4[0x00100000<br/>カーネルコード]
        MEM5[0x00200000<br/>カーネルスタック]
        MEM6[0x00300000<br/>スレッドスタック]
        MEM7[0x00400000<br/>空き領域...]
        MEM8[0xFFFFFFFF<br/>終端]
    end

    subgraph "カーネルメモリ詳細"
        K1[0x100000-0x1FFFFF<br/>Kernel Code (1MB)]
        K2[0x200000-0x2FFFFF<br/>Kernel Stack (1MB)]
        K3[0x300000-<br/>Thread Stacks<br/>4KB × MAX_THREADS]
    end

    MEM4 --> K1
    MEM5 --> K2
    MEM6 --> K3
```

### GDT（グローバルディスクリプタテーブル）

```mermaid
graph LR
    subgraph "GDT構造"
        GDT[GDT Base]
        NULL[NULLセグメント<br/>インデックス0]
        CODE[コードセグメント<br/>インデックス1/セレクタ0x08]
        DATA[データセグメント<br/>インデックス2/セレクタ0x10]
    end

    subgraph "セグメント属性"
        CODE_ATTR[ベース: 0x00000000<br/>リミット: 0xFFFFFFFF<br/>アクセス: 実行/読込可能<br/>DPL: 0<br/>粒度: 4KB]
        DATA_ATTR[ベース: 0x00000000<br/>リミット: 0xFFFFFFFF<br/>アクセス: 読込/書込可能<br/>DPL: 0<br/>粒度: 4KB]
    end

    GDT --> NULL
    GDT --> CODE
    GDT --> DATA
    CODE --> CODE_ATTR
    DATA --> DATA_ATTR
```

## ⚡ 割り込みシステムアーキテクチャ

### 割り込み処理フロー全体

```mermaid
sequenceDiagram
    participant HW as ハードウェア
    participant PIC as PIC 8259A
    participant CPU as x86 CPU
    participant IDT as IDT
    participant Handler as 割り込みハンドラ
    participant Scheduler as スケジューラ
    participant Thread as スレッド

    HW->>PIC: デバイス信号 (IRQ)
    PIC->>CPU: 割り込み要求 (INT)
    CPU->>CPU: 現在状態をスタックに保存
    CPU->>IDT: ハンドラアドレス取得
    IDT->>Handler: ジャンプ
    Handler->>Handler: レジスタ保存
    Handler->>Scheduler: タイマー処理呼び出し
    Scheduler->>Thread: コンテキストスイッチ (必要時)
    Thread->>Handler: 戻り
    Handler->>Handler: レジスタ復元
    Handler->>CPU: IRET
    CPU->>CPU: 状態復元
    CPU->>HW: 実行継続
```

### IDT（割り込みディスクリプタテーブル）

```mermaid
graph TB
    subgraph "IDTエントリ構造"
        IDT_ENT[idt_entry]
        BASE_L[base_low<br/>下位16bit]
        SEL[selector<br/>コードセグメント]
        FLAGS[flags<br/>P/DPL/type]
        BASE_H[base_high<br/>上位16bit]
    end

    subgraph "主要割り込みベクタ"
        INT32[INT 32<br/>IRQ0: タイマー]
        INT33[INT 33<br/>IRQ1: キーボード]
        INT34[INT 34<br/>IRQ2: カスケード]
    end

    subgraph "ハンドラ関数"
        TIMER[timer_interrupt_handler]
        KBD[keyboard_interrupt_handler]
    end

    IDT_ENT --> BASE_L
    IDT_ENT --> SEL
    IDT_ENT --> FLAGS
    IDT_ENT --> BASE_H

    INT32 --> TIMER
    INT33 --> KBD
    INT34 -.->|使用しない| TIMER
```

### PIC（プログラマブル割り込みコントローラ）

```mermaid
graph LR
    subgraph "PICマスター"
        MASTER[PIC Master]
        IRQ0[IRQ0<br/>PIT Timer]
        IRQ1[IRQ1<br/>Keyboard]
        IRQ2[IRQ2<br/>Cascade]
        IRQ3[IRQ3<br/>COM2/COM4]
    end

    subgraph "PICスレーブ"
        SLAVE[PIC Slave]
        IRQ8[IRQ8<br/>RTC]
        IRQ12[IRQ12<br/>Mouse]
    end

    subgraph "CPU割り込み"
        INT32[INT 32<br/>元IRQ0]
        INT33[INT 33<br/>元IRQ1]
        INT40[INT 40<br/>元IRQ8]
    end

    IRQ0 --> INT32
    IRQ1 --> INT33
    IRQ2 --> SLAVE
    IRQ8 --> INT40

    MASTER --> CPU
    SLAVE --> MASTER
```

## 🧵 スレッド管理アーキテクチャ

### スレッド制御ブロック（TCB）

```mermaid
classDiagram
    class thread_t {
        +uint32_t stack[1024]
        +thread_state_t state
        +uint32_t counter
        +uint32_t delay_ticks
        +uint32_t last_tick
        +block_reason_t block_reason
        +uint32_t wake_up_tick
        +int display_row
        +thread_t* next_ready
        +thread_t* next_blocked
        +uint32_t esp
    }

    class thread_state_t {
        <<enumeration>>
        THREAD_READY
        THREAD_RUNNING
        THREAD_BLOCKED
    }

    class block_reason_t {
        <<enumeration>>
        BLOCK_REASON_NONE
        BLOCK_REASON_TIMER
        BLOCK_REASON_KEYBOARD
    }

    thread_t --> thread_state_t
    thread_t --> block_reason_t
```

### スレッド状態遷移

```mermaid
stateDiagram-v2
    [*] --> READY: create_thread()
    READY --> RUNNING: schedule()
    RUNNING --> READY: タイムスライス終了
    RUNNING --> BLOCKED: sleep()/getchar()
    BLOCKED --> READY: タイマー満了/キー入力
    RUNNING --> [*]: スレッド終了

    note right of BLOCKED
        block_reasonで分類:
        - TIMER: sleep()待ち
        - KEYBOARD: getchar()待ち
    end note
```

### スケジューラロジック

```mermaid
flowchart TD
    START[timer_interrupt_handler] --> SAVE[レジスタ保存]
    SAVE --> LOCK[スケジューラロック取得]
    LOCK --> CHECK{scheduler_lock_count > 0?}
    CHECK -->|はい| SKIP[スケジューリングスキップ]
    CHECK -->|いいえ| TIMER_CHECK[タイムアウトスレッド確認]

    TIMER_CHECK --> WAKE[ブロック解除処理]
    WAKE --> CURRENT{current_threadがREADY?}
    CURRENT -->|はい| MOVE[readyリスト末尾へ移動]
    CURRENT -->|いいえ| REMOVE[readyリストから削除]

    MOVE --> SELECT[next_thread選択]
    REMOVE --> SELECT

    SELECT --> SWITCH{current_thread != next_thread?}
    SWITCH -->|はい| CTX[コンテキストスイッチ]
    SWITCH -->|いいえ| SKIP

    CTX --> UNLOCK[スケジューラロック解除]
    SKIP --> UNLOCK
    UNLOCK --> EOI[PICにEOI送信]
    EOI --> RESTORE[レジスタ復元]
    RESTORE --> IRET[IRETで戻る]
```

### コンテキストスイッチ詳細

```mermaid
sequenceDiagram
    participant Current as 現在スレッド
    participant ASM as interrupt.s
    participant Scheduler as scheduler()
    participant Switch as context_switch()
    participant Next as 次スレッド

    Note over Current,Next: タイマー割り込み発生
    Current->>ASM: 実行中断
    ASM->>ASM: pusha (レジスタ保存)
    ASM->>Scheduler: timer_handler_c呼び出し
    Scheduler->>Scheduler: schedule()実行
    Scheduler->>Switch: context_switch呼び出し

    Switch->>Switch: pushfd (EFLAGS保存)
    Switch->>Switch: ESP保存
    Switch->>Switch: ESP切り替え
    Switch->>Switch: popfd (EFLAGS復元)

    Switch->>Next: 実行再開
    Next->>ASM: ret
    ASM->>ASM: popa (レジスタ復元)
    ASM->>Next: IRET
    Next->>Next: スレッド処理継続
```

## 🎯 タイマーシステム詳細

### PIT（プログラマブル間隔タイマー）

```mermaid
graph TB
    subgraph "PIT 8254チップ"
        PIT_CHIP[PIT]
        CLK[1.193182MHz<br/>基準クロック]
        CH0[Channel 0<br/>システムタイマー]
        CH1[Channel 1<br/>RAMリフレッシュ]
        CH2[Channel 2<br/>スピーカー]
    end

    subgraph "設定レジスタ"
        CMD[0x43<br/>Command Register]
        DATA0[0x40<br/>Channel 0 Data]
        CTRL[0x36<br/>Mode 3<br/>Square Wave]
    end

    subgraph "タイマー計算"
        FREQ[目標周波数: 100Hz]
        DIV[分周比: 11931]
        TICK[1ティック = 10ms]
    end

    CLK --> PIT_CHIP
    PIT_CHIP --> CH0
    CH0 --> DIV
    DIV --> TICK
    CMD --> CTRL
    CTRL --> DATA0
```

### タイマー割り込み処理

```mermaid
flowchart TD
    IRQ[timer_interrupt_handler] --> CALL_ASM[アセンブリ処理]
    CALL_ASM --> CALL_C[timer_handler_c]

    CALL_C --> INC_TICK[system_ticks++]
    INC_TICK --> CHECK_TIMER[タイムアウトスレッド確認]

    CHECK_TIMER --> LOOP{blockedリスト巡回}
    LOOP -->|スレッドあり| CHECK_WAKE{wake_up_tick <= system_ticks?}
    CHECK_WAKE -->|はい| UNBLOCK[ブロック解除]
    CHECK_WAKE -->|いいえ| CONTINUE[次スレッドへ]
    UNBLOCK --> READY[READYリストへ追加]

    CONTINUE --> LOOP
    LOOP -->|リスト終了| SCHEDULE[schedule()呼び出し]
    READY --> SCHEDULE

    SCHEDULE --> EOI[PIC_EOI送信]
    EOI --> RETURN[ハンドラ終了]
```

## ⌨️ キーボードシステム詳細

### PS/2キーボードアーキテクチャ

```mermaid
graph TB
    subgraph "PS/2キーボード"
        KB[キーボードデバイス]
        CTRL[PS/2 Controller]
    end

    subgraph "I/Oポート"
        DATA[0x60<br/>Data Port]
        STATUS[0x64<br/>Status Port]
    end

    subgraph "割り込み処理"
        IRQ1[IRQ1割り込み]
        HANDLER[keyboard_handler_c]
    end

    subgraph "バッファシステム"
        RING[リングバッファ<br/>256バイト]
        HEAD[headポインタ]
        TAIL[tailポインタ]
    end

    subgraph "変換テーブル"
        SCAN[スキャンコード]
        ASCII[ASCII変換テーブル]
        SHIFT[Shift押下時テーブル]
    end

    KB --> CTRL
    CTRL --> IRQ1
    IRQ1 --> HANDLER
    HANDLER --> DATA
    HANDLER --> STATUS
    HANDLER --> RING
    SCAN --> ASCII
    SCAN --> SHIFT
    ASCII --> RING
```

### キーボードバッファ詳細

```mermaid
sequenceDiagram
    participant HW as キーボード
    participant ISR as keyboard_handler
    participant Buffer as リングバッファ
    participant Thread1 as 待機スレッド
    participant Thread2 as 実行スレッド

    Note over HW,Buffer: キー押下
    HW->>ISR: IRQ1割り込み
    ISR->>Buffer: keyboard_buffer_put()
    Note over Buffer: スキャンコード→ASCII変換
    ISR->>Thread1: unblock_keyboard_threads()

    Note over Thread1,Thread2: スレッド切り替え
    Thread1->>Buffer: keyboard_buffer_get()
    Buffer->>Thread1: 文字返却
    Thread1->>Thread1: 文字処理
    Thread2->>Thread2: 実行継続
```

### キーボード入力API

```mermaid
graph LR
    subgraph "ユーザーAPI"
        GETC[getchar()]
        READS[read_line()]
        SCANF[scanf_char()]
    end

    subgraph "内部処理"
        BLOCK[block_current_thread<br/>BLOCK_REASON_KEYBOARD]
        CHECK[keyboard_buffer_is_empty()]
        UNBLOCK[unblock_keyboard_threads()]
    end

    subgraph "バッファ操作"
        PUT[keyboard_buffer_put]
        GET[keyboard_buffer_get]
    end

    GETC --> BLOCK
    READS --> BLOCK
    SCANF --> BLOCK

    BLOCK --> CHECK
    CHECK -->|空| UNBLOCK
    UNBLOCK --> GET

    ISR割り込み --> PUT
```

## 🖥️ VGA表示システム

### VGAテキストモード詳細

```mermaid
graph TB
    subgraph "VGAテキストバッファ"
        MEM[0xB8000]
        OFFSET[オフセット計算<br/>y * 80 + x]
        CHAR[文字コード]
        ATTR[色属性]
    end

    subgraph "色属性フォーマット"
        FG[前景色: 4bit]
        BG[背景色: 4bit]
        COMBINE[8bit属性値]
    end

    subgraph "VGA関数群"
        PUTC[vga_putc]
        PUTS[vga_puts]
        MOVE[vga_move_cursor]
        CLEAR[vga_clear]
    end

    OFFSET --> MEM
    CHAR --> MEM
    ATTR --> MEM
    FG --> COMBINE
    BG --> COMBINE
    COMBINE --> ATTR

    PUTC --> MEM
    PUTS --> MEM
    MOVE --> MEM
    CLEAR --> MEM
```

### デバッグ表示システム

```mermaid
graph LR
    subgraph "デバッグ出力"
        DEBUG[debug_print]
        VARG[va_list処理]
        FORMAT[フォーマット処理]
    end

    subgraph "出力先"
        VGA_OUT[VGAテキストバッファ]
        SERIAL_OUT[シリアルポートCOM1]
    end

    subgraph "シリアル設定"
        COM1[0x3F8ポート]
        BAUD[ボーレート38400]
        CONFIG[8N1設定]
    end

    DEBUG --> VARG
    VARG --> FORMAT
    FORMAT --> VGA_OUT
    FORMAT --> SERIAL_OUT

    SERIAL_OUT --> COM1
    COM1 --> BAUD
    BAUD --> CONFIG
```

## 🔧 ビルドシステム

### Makefileターゲット依存関係

```mermaid
graph TD
    ALL[all]
    RUN[run]
    TEST[test]
    CLEAN[clean]

    subgraph "ビルドターゲット"
        KERNEL[kernel.bin]
        BOOT[boot.bin]
        IMAGE[os.img]
    end

    subgraph "ソースファイル"
        BOOT_S[src/boot/boot.s]
        ENTRY_S[src/boot/kernel_entry.s]
        INT_S[src/boot/interrupt.s]
        KERNEL_C[src/kernel.c]
        KBD_C[src/keyboard.c]
    end

    ALL --> KERNEL
    KERNEL --> BOOT
    KERNEL --> IMAGE

    BOOT --> BOOT_S
    KERNEL --> ENTRY_S
    KERNEL --> INT_S
    KERNEL --> KERNEL_C
    KERNEL --> KBD_C

    RUN --> ALL
    TEST --> ALL
    CLEAN --> -.->|削除| KERNEL
    CLEAN --> -.->|削除| IMAGE
```

### ツールチェーン構成

```mermaid
graph TB
    subgraph "開発ツール"
        GCC[i686-elf-gcc<br/>クロスコンパイラ]
        NASM[nasm<br/>アセンブラ]
        LD[i686-elf-ld<br/>リンカ]
        QEMU[qemu-system-i386<br/>エミュレータ]
    end

    subgraph "ビルドプロセス"
        SRC[ソースコード]
        OBJ[オブジェクトファイル]
        BIN[バイナリイメージ]
        IMG[ディスクイメージ]
    end

    subgraph "実行環境"
        BOOT[ブートローダ]
        KERNEL[カーネル]
        OS[Mini OS]
    end

    SRC --> GCC
    SRC --> NASM
    GCC --> OBJ
    NASM --> OBJ
    OBJ --> LD
    LD --> BIN
    BIN --> IMG

    IMG --> QEMU
    QEMU --> BOOT
    BOOT --> KERNEL
    KERNEL --> OS
```

## 🎓 教育的価値と学習ポイント

### OS開発の主要概念

このMini OSは、以下の重要なOS概念を実践的に学習できます：

1. **ブートプロセス**: BIOS→ブートローダ→カーネルの連携
2. **メモリ管理**: フラットメモリモデル、スタック管理
3. **割り込み処理**: ハードウェア割り込み、IDT、PIC
4. **マルチスレッド**: スケジューリング、コンテキストスイッチ
5. **デバイスドライバ**: タイマー、キーボード、VGA
6. **システムコール**: ユーザーAPIの実装

### Software Engineerへの応用

```mermaid
mindmap
  root((Mini OS学習))
    低レベル理解
      CPUアーキテクチャ
      メモリ管理
      割り込み機構
    システム設計
      レイヤ化アーキテクチャ
      モジュール化
      並行処理
    デバッグスキル
      ハードウェアデバッグ
      シリアル出力活用
      QEMU活用
    パフォーマンス理解
      コンテキストスイッチ
      割り込みレイテンシ
      メモリアクセス
```

### 実践的な開発スキル

1. **クロスコンパイル環境**: ターゲットとホストの分離
2. **アセンブリ言語**: Cとの連携、インラインアセンブリ
3. **メモリレイアウト設計**: リンカスクリプト、アドレス配置
4. **割り込み駆動設計**: イベントベースプログラミング
5. **状態機械設計**: スレッド状態遷移
6. **リングバッファ**: 生産者-消費者パターン

## 📊 パフォーマンス特性

### タイミング分析

```mermaid
gantt
    title タイムライン分析（1秒間）
    dateFormat X
    axisFormat %s

    section スレッド実行
    ThreadA : 0, 100ms
    ThreadB : 100ms, 100ms
    ThreadC : 200ms, 100ms
    ThreadA : 300ms, 100ms

    section 割り込み
    Timer IRQ : 0, 10ms
    Timer IRQ : 10ms, 10ms
    Timer IRQ : 20ms, 10ms
    Timer IRQ : 30ms, 10ms

    section コンテキストスイッチ
    Switch A→B : active, 50ms
    Switch B→C : 100ms, 150ms
    Switch C→A : 200ms, 250ms
```

### リソース使用状況

- **メモリ使用**: 約2MB（カーネル + スタック）
- **CPU使用率**: 100%（バックグラウンドタスク含む）
- **割り込み頻度**: 100Hz（10ms間隔）
- **コンテキストスイッチ時間**: 約50-100クロックサイクル

## 🚀 拡張可能性

### 将来的な機能拡張

```mermaid
graph TB
    subgraph "現在の機能"
        CURRENT[マルチスレッド]
        TIMER[タイマー]
        KBD[キーボード]
        VGA[VGA表示]
    end

    subgraph "拡張機能候補"
        MEM[メモリ管理<br/>ページング]
        FS[ファイルシステム<br/>FAT32]
        NET[ネットワーク<br/>TCP/IP]
        USER[ユーザーモード<br/>システムコール]
        SMP[マルチコア<br/>SMP対応]
    end

    CURRENT --> MEM
    CURRENT --> FS
    CURRENT --> NET
    CURRENT --> USER
    CURRENT --> SMP
```

## 📝 まとめ

このMini OSは、教育用として設計されたx86 32-bitオペレーティングシステムであり、以下の特徴を持っています：

1. **完全な実装**: ブートからアプリケーション層まで
2. **実践的アーキテクチャ**: 現代OSの基本概念を網羅
3. **クリーンな設計**: モジュール化された構造
4. **豊富なドキュメント**: 学習者が理解しやすい解説
5. **拡張性**: 将来の機能追加が容易

Software Engineerがこのコードベースを学ぶことで、コンピュータシステムの根本的な理解を深め、より高度なシステム開発に必要な知識を習得できます。

---

*このドキュメントは、Mini OSのアーキテクチャをMermaidチャートと共に詳細に解説しました。コードの具体的な実装については、ソースコードと各種READMEを参照してください。*