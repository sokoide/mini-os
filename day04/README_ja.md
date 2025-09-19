# Day 04: シリアルポートデバッグ（freestanding C + インラインアセンブリ）🔧

## 本日のゴール

UART シリアルポートを使ってデバッグ環境を構築し、VGA とシリアルの二重出力システムを実装する。

## 背景

Day3 で C 言語移行を達成したが、デバッグ手段が VGA 画面だけでは限界がある。本日はシリアルポートを活用して、ログ保存や継続出力が可能になるデバッグインフラを構築し、より効率的な OS 開発環境を整える。

## 新しい概念

-   **I/O ポート (inb, outb)**: メモリ空間とは別のアドレス空間（I/O 空間）を使ってハードウェアと通信する仕組み。inb 命令でポートからデータ読み込み、outb で書き込み。ハードウェア制御の基本的な方法。
-   **インラインアセンブリ**: C コード内に直接 x86 アセンブリを記述する GCC の機能。ハードウェアアクセスに必須。

## 学習内容

-   **インラインアセンブリ基礎**: C コード内での x86 アセンブリ記述
-   **I/O ポートアクセス**: `in`/`out` 命令を C 関数でラップ
-   **UART プログラミング**: 8250/16550 系 COM1（0x3F8）制御
-   **ハードウェア初期化**: ボーレート、フレーミング、FIFO 設定
-   **ポーリング方式**: LSR レジスタによる送信準備完了待ち
-   **デバッグインフラ**: VGA + シリアルの並行出力システム

【メモ】送信レジスタ空き待ち（LSR 0x20）

-   UART の Line Status Register の 0x20 ビットが 1 なら送信レジスタが空き、1 文字書けます。
-   `while(!(inb(COM1+5) & 0x20)) {}` の待機はこの確認をしています。

## タスクリスト

-   [ ] io.h ヘッダーファイルを作成し、inb/outb 関数をインラインアセンブリで実装する
-   [ ] UART のレジスタを理解し、COM1 ポートの初期化を行う
-   [ ] kernel.c でシリアル出力関数を実装し、ポーリング方式でデータを送信する
-   [ ] VGA とシリアルの並行出力システムを構築する
-   [ ] QEMU で-serial オプションを使ってシリアル出力を確認する
-   [ ] デバッグインフラとして二重出力環境を完成させる

## 前提知識の確認

### 必要な知識

-   **Day 01〜02**: ブート、GDT、プロテクトモード切り替え
-   **Day 03**: freestanding C、VGA 制御、C + アセンブリ連携
-   **C 言語**: 基本的な構文、関数、ヘッダファイル
-   **ハードウェア概念**: I/O ポート、レジスタ、ポーリング

### 新しく学ぶこと

-   **インラインアセンブリ**: C コード内での x86 命令記述
-   **制約記述**: GCC のアセンブリ制約の書き方（`"=a"`, `"Nd"` など）
-   **I/O ポート**: メモリマップと異なるアドレッシング方式
-   **UART プロトコル**: シリアル通信の基礎（ボーレート、フレーミング）

## 進め方と構成

```
├── boot
│   ├── boot.s           # 16bit: A20, GDT（同ファイル内定義）, INT 13h で kernel 読込, PE 切替
│   └── kernel_entry.s   # 32bit: セグメント/スタック設定 → kmain()
├── boot.s               # （補助・比較用の単体版がある場合）
├── io.h                 # inb/outb/io_wait（Cインラインasm）
├── kernel.c             # VGA と COM1 をCで初期化・出力
├── Makefile             # boot と kernel を連結して os.img を生成
├── README.md            # このファイル
└── vga.h                # VGA API（C）
```

完成版は `day04_completed/` を参照してください。`day04_completed` ではカーネルは `kernel.c` をビルドする形式です。

## 実装ガイド

### 1. インラインアセンブリの理解

**なぜインラインアセンブリが必要なのか:**

OS 開発では、ハードウェアを直接制御する必要がありますが、C 言語だけではできない操作があります：

-   I/O ポートへの読み書き（`in`/`out`命令）
-   CPU レジスタの直接操作
-   特殊なメモリアクセス

インラインアセンブリを使うことで、C コード内でこれらの低レベル操作を実現できます。

**インラインアセンブリ**とは、C コード内に直接 x86 アセンブリを記述する GCC の機能です。freestanding C 環境では、ハードウェアアクセスに必須の技術です。

#### GCC インラインアセンブリの構文

```c
__asm__("命令" : 出力制約 : 入力制約 : 変更レジスタ);
```

#### 制約記述の意味

-   `"=a"`: 出力を EAX レジスタに格納
-   `"a"`: 入力を EAX レジスタから取得
-   `"Nd"`: 入力を EDX レジスタまたは即値として使用
-   `%%al`: レジスタ名をエスケープ（% → %%）

### 2. io.h（I/O ポートアクセス関数）

```c
// io.h — freestanding C 用 I/O ポートヘルパ
#pragma once
#include <stdint.h>

// I/O ポートから 1 バイト読み取り
static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

// I/O ポートに 1 バイト書き込み
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

// I/O 遅延（古い PC との互換性確保）
static inline void io_wait(void) {
    __asm__ volatile ("outb %%al, $0x80" : : "a"(0));
}
```

**重要な点**:

-   `volatile`: コンパイラ最適化を防ぐ（I/O は副作用があるため）
-   `static inline`: ヘッダファイル内関数の重複定義を避ける
-   制約 `"Nd"`: ポート番号を EDX または即値で指定

### 2. kernel.c（COM1 初期化と送信）

```c
#include <stdint.h>
#include "io.h"
#include "vga.h"

#define COM1 0x3F8

static void serial_init(void){
    outb(COM1+1, 0x00);      // IER=0（割り込み無効）
    outb(COM1+3, 0x80);      // LCR: DLAB=1
    outb(COM1+0, 0x03);      // DLL=3（115200/38400）
    outb(COM1+1, 0x00);      // DLM=0
    outb(COM1+3, 0x03);      // LCR: 8N1, DLAB=0
    outb(COM1+2, 0xC7);      // FCR: FIFO有効/クリア/14B閾値
    outb(COM1+4, 0x0B);      // MCR: RTS/DTR/OUT2
}

static void serial_putc(char c){
    while(!(inb(COM1+5) & 0x20)){} // LSR bit5 (THR empty)
    outb(COM1+0, (uint8_t)c);
}

static void serial_puts(const char* s){
    while(*s){
        if(*s=='\n') serial_putc('\r');
        serial_putc(*s++);
    }
}

void kmain(void){
    vga_init();
    vga_puts("Day 04: Serial debug (C)\n");
    serial_init();
    serial_puts("COM1: Hello from C!\\r\\n");
}
```

**実装のポイント**:

-   **ポーリング方式**: LSR の THR Empty ビット監視
-   **CRLF 変換**: Unix の `
` を Windows 互換 `
` に変換
-   **型安全性**: `char` を `uint8_t` にキャストしてポート出力

### 3. UART レジスタとシリアル通信の理論

#### COM1 ポートマッピング

| オフセット | レジスタ    | 説明                                   |
| ---------- | ----------- | -------------------------------------- |
| COM1+0     | THR/RBR/DLL | 送信/受信バッファ、DLAB 時は分周器下位 |
| COM1+1     | IER/DLM     | 割り込み許可、DLAB 時は分周器上位      |
| COM1+2     | FCR/IIR     | FIFO 制御/割り込み識別                 |
| COM1+3     | LCR         | 回線制御（データ長、パリティ、DLAB）   |
| COM1+4     | MCR         | モデム制御（RTS、DTR、OUT）            |
| COM1+5     | LSR         | 回線状態（送受信準備、エラー）         |

#### 初期化手順の理論

1. **割り込み無効化**: OS がまだ割り込みハンドラを用意していない
2. **DLAB 設定**: 分周器レジスタアクセスモードに切り替え
3. **ボーレート設定**: 38400bps = 115200 ÷ 3
4. **フレーミング設定**: 8 ビットデータ、パリティなし、1 ストップビット
5. **FIFO 有効化**: バッファリングで効率向上
6. **モデム制御**: ハンドシェイク信号の基本設定

#### I/O ポート vs メモリマップ

| 方式             | アクセス方法         | 例             | 特徴                   |
| ---------------- | -------------------- | -------------- | ---------------------- |
| **I/O ポート**   | `in`/`out` 命令      | UART, PIC, PIT | 専用アドレス空間、高速 |
| **メモリマップ** | 通常のメモリ読み書き | VGA (0xB8000)  | メモリ空間の一部使用   |

### 5. boot/boot.s（アーキテクチャの継続）

Day 03 で確立したアーキテクチャを継続します：

-   **カーネル読み込み**: `INT 13h` で第 2 セクタから `0x00100000` へ
-   **プロテクトモード**: `CR0.PE=1` → `jmp 0x08:kernel_entry`（`dword` なし）

### 6. boot/kernel_entry.s（C への橋渡し）

-   **セグメント設定**: DS/ES/FS/GS/SS を `0x10` に設定
-   **スタック初期化**: `ESP` を適切な位置に設定
-   **C 関数呼び出し**: `extern kmain` を `call` で実行

### 7. Makefile（C コンパイル対応）

Day 03 と同様の C コンパイルに対応：

```makefile
OBJECTS = boot/boot.o boot/kernel_entry.o kernel.o
TARGET = os.img

$(TARGET): $(OBJECTS)
	ld -T linker.ld -o kernel.elf $(OBJECTS)
	objcopy -O binary kernel.elf kernel.bin
	cat boot.bin kernel.bin > $(TARGET)

kernel.o: kernel.c io.h vga.h
	i686-elf-gcc -ffreestanding -c kernel.c -o kernel.o
```

## ビルドと実行

```bash
cd day04
make clean
make all
make run        # -serial stdio で端末にシリアル出力
```

期待される動作:

-   画面(VGA)に「Day 04: Serial debug (C)」
-   ターミナル(シリアル)に「COM1: Hello from C!」

## コード解説：UART レジスタ

-   `COM1+0 (THR/DLL)` 送信ホールド/DLAB 時は分周下位
-   `COM1+1 (IER/DLM)` 割り込み許可/DLAB 時は分周上位
-   `COM1+2 (FCR)` FIFO 制御（enable/clear/threshold）
-   `COM1+3 (LCR)` データ長/ストップ/パリティ/DLAB
-   `COM1+4 (MCR)` RTS/DTR/OUT/ループバック
-   `COM1+5 (LSR)` 送受信状態（bit5: THR empty）

## トラブルシューティング

1. シリアル出力が表示されない
    - `-serial stdio` を付けて起動しているか
    - LSR bit5 を待ってから送信しているか
    - QEMU が COM1 を有効化しているか（既定で有効）
2. 文字化け
    - 分周比（DLL/DLM）が正しいか（38400→3）
    - 8N1 設定（LCR=0x03）か
3. 画面が真っ黒
    - GDT セレクタ（CS=0x08, DS=0x10）と far jump の順序

## 理解度チェック

### インラインアセンブリの理解

1. `__asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port))` の各部分の意味は？
2. なぜ I/O 操作に `volatile` を付ける必要があるのか？
3. GCC 制約 `"=a"` と `"Nd"` の違いを説明できる？

### UART プログラミングの理解

1. なぜ LSR bit5（THR empty）を待つ必要があるのか？
    - **回答**: 送信バッファが空になってから次のデータを送信しないと、データが上書きされて失われる
2. DLAB ビットを立てる/下ろすタイミングの意味は？
    - **回答**: DLAB=1 で分周器設定モード、DLAB=0 で通常の送受信モードに切り替える
3. VGA とシリアルの違い（メモリマップ vs I/O ポート）を説明できる？
    - **回答**: VGA は 0xB8000 番地にメモリアクセス、シリアルは 0x3F8 ポートに `in`/`out` 命令

## 次のステップ

-   ✅ UART 初期化と送信
-   ✅ I/O ポートアクセスの基礎
-   ✅ 二重デバッグ（VGA+シリアル）

次は割り込み（IDT）やタイマ（PIT）を使い、非同期イベントの扱いを学びます。
