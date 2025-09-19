# Day 03: VGA テキスト表示（C 移行＋ boot 分割）🎨

## 今日の学習目標

**Day 03 から開発手法が大きく変わります。** Day 01-02 のシンプルなアセンブリファイル構成から、C 言語を中心とした本格的な OS 開発に移行します。

### なぜ C 言語に移行するのか？

**Day 01-02 のアセンブリ中心開発の限界**

-   **複雑性の急増**: VGA 制御、割り込み処理、メモリ管理等が加わると、アセンブリだけでは保守困難
-   **型安全性の欠如**: レジスタやメモリ操作でのバグが発見しにくい
-   **可読性の低下**: コードの意図が分かりにくく、チーム開発に不向き

**C 言語移行の利点**

-   **型システム**: 構造体、ポインタ型による設計明確化
-   **関数分割**: 機能をモジュール化し、テストしやすい構造
-   **保守性**: 高レベル言語による意図の明確な表現
-   **実用性**: Linux、Windows 等、実際の OS は C/C++で開発

Day 03 からは C を中心に実装します。最終形（day99_completed）の構成に近づけるため、ブート関連は `boot/boot.s`（16bit）と `boot/kernel_entry.s`（32bit エントリ）に分割します。GDT は `boot.s` 内へ統合します。表示は `kernel.c` と `vga.h` の C 実装で行います。

## 学習内容

-   VGA テキストバッファ（0xB8000）の構造と C からのアクセス
-   ハードウェアカーソルの制御（CRTC レジスタ）
-   色属性（前景/背景）とテキスト描画 API
-   改行・スクロール・カーソル前進の実装
-   ブート段階の役割分離（16bit ローダ vs 32bit カーネルエントリ）

【メモ】VGA テキスト 1 文字は 2 バイト

-   下位 8bit が文字コード、上位 8bit が属性（下位 4bit=前景色/上位 4bit=背景色）。
-   例: `uint16_t cell = (uint8_t)c | ((attr & 0xFF) << 8);` を `0xB8000 + 2*(row*80+col)` に書き込む。

## タスクリスト

-   [ ] vga.h ヘッダーファイルを作成し、VGA 制御 API を定義する
-   [ ] kernel.c ファイルを作成し、kmain()関数を実装する
-   [ ] boot.s で GDT を統合し、カーネルをメモリに読み込む処理を追加する
-   [ ] kernel_entry.s を作成し、32bit 環境での初期化を行う
-   [ ] Makefile を更新し、C コードのコンパイルとリンクを行う
-   [ ] freestanding C 環境で VGA バッファに直接書き込んで文字列を表示する
-   [ ] QEMU で動作確認し、カラフルなテキスト表示を確認する

## 前提知識の確認

### 必要な知識

-   Day 01〜02（ブート、GDT、プロテクトモード）
-   32 ビット C（ポインタ、配列、ヘッダと実装の分離）

### 今日新しく学ぶこと

-   **freestanding C 環境**（libc なし、-ffreestanding/-nostdlib）
-   低レベル I/O への C からの直接アクセス
-   16bit ローダと 32bit カーネルの役割分離

## freestanding C 環境の理解

**通常の C 環境 vs OS 開発環境の違い:**

通常の C プログラム（例: Hello World）は OS の上に乗って動作します：

-   OS がメモリ管理、ファイル I/O、標準ライブラリを提供
-   `main()`が自動的に呼ばれる
-   `printf()`や`malloc()`が使える

OS 開発では、自分自身が OS になるため：

-   標準ライブラリが存在しない（OS 自体が提供するもの）
-   メモリ、I/O を直接制御する必要がある
-   `kmain()`を手動で呼び出す

**なぜ freestanding C が必要か:**

-   OS がない環境で動作するコードを書くため
-   ハードウェア（VGA、シリアル）を直接操作するため
-   最小限のリソースで動作するように設計するため

Day 03 以降では、通常の C 開発環境とは大きく異なる「freestanding C」環境で開発します。

### 通常の C 環境 vs freestanding C 環境

| 項目               | 通常の C 環境                             | freestanding C 環境（OS 開発）          |
| ------------------ | ----------------------------------------- | --------------------------------------- |
| **標準ライブラリ** | `printf`, `malloc`, `strlen` 等が利用可能 | **利用不可** - 自分で実装が必要         |
| **メモリ管理**     | `malloc`/`free` でヒープ管理              | **自前実装** - 物理メモリ直接管理       |
| **文字列操作**     | `strlen`, `strcpy` 等が利用可能           | **自前実装** - 1 文字ずつ手動処理       |
| **I/O 操作**       | `printf` でコンソール出力                 | **ハードウェア直接制御** - VGA/シリアル |
| **起動処理**       | `main()` から自動開始                     | **ブートローダーが手動呼び出し**        |
| **リンカ**         | OS がメモリ配置を決定                     | **明示的にメモリ配置を指定**            |

### freestanding C で禁止/利用不可な要素

```c
// ❌ 以下は一切使用できません

// 標準I/O関数
printf("Hello");           // → vga_puts("Hello") で代替
scanf("%d", &num);         // → getchar() で代替

// メモリ管理関数
int* ptr = malloc(100);    // → 配列または物理メモリ直接管理
free(ptr);

// 文字列関数
strlen(str);               // → 手動でループしてカウント
strcpy(dest, src);         // → 手動で1文字ずつコピー

// 数学関数
sin(3.14);                 // → 自前実装または使用回避
```

### freestanding C で自前実装が必要な機能

```c
// ✅ 以下のような関数を自分で実装します

// VGA出力（printf 代替）
void vga_puts(const char* s);
void vga_putc(char c);

// 文字列操作（strlen 代替）
int my_strlen(const char* s) {
    int len = 0;
    while (s[len] != '\0') len++;
    return len;
}

// I/Oポート操作（OS特有）
void outb(uint16_t port, uint8_t value);
uint8_t inb(uint16_t port);
```

### コンパイル時の特別なフラグ

```bash
# freestanding C 用のコンパイラフラグ
-ffreestanding   # 標準ライブラリを仮定しない
-nostdlib       # 標準ライブラリをリンクしない
-fno-stack-protector  # スタック保護機能を無効化
-m32            # 32ビットコード生成
```

### 制約がある理由

**なぜこれらの制約があるのか？**

1. **OS がまだ存在しない**: `printf` や `malloc` は OS 機能に依存している
2. **メモリ管理未実装**: ヒープ管理システムがまだない
3. **ハードウェア直接制御**: VGA、シリアル等のハードウェアを直接操作する必要
4. **最小限の実行環境**: ブートローダーから呼び出される最小環境

## 進め方と全体像

```
boot/boot.s（16bit）:
  - A20有効化、GDT定義と登録、BIOSのINT 0x13でカーネルを読み込み
  - CR0.PE=1 → far jump（32bitオフセット）で kernel_entry へ

boot/kernel_entry.s（32bit）:
  - セグメント設定、スタック初期化、extern kmain を call

kernel.c: kmain() で vga.h のAPIを使って表示
vga.h: VGA制御のC API（クリア/カーソル/色/出力/スクロール）
```

## 実践: C による VGA 層の実装

### ステップ 1: プロジェクト構造

**実際の day03 ディレクトリ構造**（学習用・TODO 付き版）:

```
day03/
├── README.md            # このファイル（学習ガイド）
├── boot/
│   ├── boot.s           # 16bit: A20, GDT統合, INT 0x13 で kernel 読込, PE へ
│   └── kernel_entry.s   # 32bit: セグメント/スタック設定 → kmain()
├── kernel.c             # kmain と VGAデモ（C実装）
├── vga.h                # VGAテキスト表示API定義
└── Makefile             # boot と kernel を連結して os.img を作成
```

**day03_completed の構造**（完成版参考用）:

```
day03_completed/
├── README.md            # 完成版の技術解説
├── boot/
│   ├── boot.s           # 同上（動作する実装）
│   └── kernel_entry.s   # 同上（動作する実装）
├── kernel.c             # 完全実装版
├── vga.h                # 完全なAPI定義
├── io.h                 # I/Oポート操作ヘルパ
└── Makefile             # 完成版ビルドスクリプト
```

**学習の進め方**:

1. `day03/` で自分で実装にチャレンジ
2. 行き詰まったら `day03_completed/` を参考にする
3. 両方を比較して理解を深める

### ステップ 2: vga.h（API 設計）

まずはヘッダーで API を定義します（実装は kernel.c 内、または将来的に vga.c へ分割）。

```c
// vga.h — VGAテキストモード制御（C, freestanding）
#pragma once
#include <stdint.h>

#define VGA_WIDTH  80
#define VGA_HEIGHT 25

typedef enum {
    VGA_BLACK=0, VGA_BLUE, VGA_GREEN, VGA_CYAN, VGA_RED, VGA_MAGENTA, VGA_BROWN, VGA_LIGHT_GRAY,
    VGA_DARK_GRAY, VGA_LIGHT_BLUE, VGA_LIGHT_GREEN, VGA_LIGHT_CYAN, VGA_LIGHT_RED, VGA_PINK, VGA_YELLOW, VGA_WHITE
} vga_color_t;

void vga_init(void);
void vga_clear(void);
void vga_set_color(vga_color_t fg, vga_color_t bg);
void vga_move_cursor(uint16_t x, uint16_t y);
void vga_putc(char c);
void vga_puts(const char* s);
```

### ステップ 3: kernel.c（実装とエントリ）

`kmain()` をエントリにして、vga.h の API を実装・使用します（libc なし）。

```c
// kernel.c — kmain と VGA実装（最小）
#include <stdint.h>
#include "vga.h"

static volatile uint16_t* const VGA_MEM = (uint16_t*)0xB8000;
static uint16_t cursor_x = 0, cursor_y = 0;
static uint8_t color = 0x0F; // white on black

static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

void vga_set_color(vga_color_t fg, vga_color_t bg) {
    color = (uint8_t)fg | ((uint8_t)bg << 4);
}

void vga_move_cursor(uint16_t x, uint16_t y) {
    cursor_x = x; cursor_y = y;
    uint16_t pos = y * VGA_WIDTH + x;
    outb(0x3D4, 14);
    outb(0x3D5, (pos >> 8) & 0xFF);
    outb(0x3D4, 15);
    outb(0x3D5, pos & 0xFF);
}

void vga_clear(void) {
    for (uint16_t y=0; y<VGA_HEIGHT; ++y) {
        for (uint16_t x=0; x<VGA_WIDTH; ++x) {
            VGA_MEM[y*VGA_WIDTH + x] = vga_entry(' ', color);
        }
    }
    vga_move_cursor(0,0);
}

static void vga_newline(void) {
    cursor_x = 0;
    if (++cursor_y >= VGA_HEIGHT) {
        // simple scroll up
        for (uint16_t y=1; y<VGA_HEIGHT; ++y)
            for (uint16_t x=0; x<VGA_WIDTH; ++x)
                VGA_MEM[(y-1)*VGA_WIDTH + x] = VGA_MEM[y*VGA_WIDTH + x];
        for (uint16_t x=0; x<VGA_WIDTH; ++x)
            VGA_MEM[(VGA_HEIGHT-1)*VGA_WIDTH + x] = vga_entry(' ', color);
        cursor_y = VGA_HEIGHT-1;
    }
}

void vga_putc(char c) {
    if (c=='\n') { vga_newline(); vga_move_cursor(cursor_x, cursor_y); return; }
    VGA_MEM[cursor_y*VGA_WIDTH + cursor_x] = vga_entry(c, color);
    if (++cursor_x >= VGA_WIDTH) vga_newline();
    vga_move_cursor(cursor_x, cursor_y);
}

void vga_puts(const char* s) { while (*s) vga_putc(*s++); }

void vga_init(void) { vga_set_color(VGA_WHITE, VGA_BLACK); vga_clear(); }

// I/O port helpers（後続回で io.h として提供予定/ここでは boot 側で提供可）
extern void outb(uint16_t port, uint8_t val);

void kmain(void) {
    vga_init();
    vga_puts("Day 03: C-based VGA driver\n");
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_puts("Hello from C!\n");
}
```

注: `outb` はブート側で提供（または後の回で io.h として C 内に実装）。

### ステップ 4: boot/boot.s（16bit, GDT 統合, kernel 読込, PE 切替）

`boot.s` は次を実施します：

-   割り込み無効化、A20 有効化（0x92）
-   GDT を「ファイル内に定義」し、`lgdt` で登録
-   BIOS `INT 0x13` でディスク第 2 セクタ以降を `0x00100000` へ読み込み（固定セクタ数で簡易）
-   KERNEL_SECTORS セクタ分をメモリに読み込むようになっています。127 セクタの場合、127 x 512 バイト = 63.5 キロバイトが読み込まれます
-   `CR0.PE=1` でプロテクトモードへ → `jmp dword 0x08:kernel_entry` で 32bit へ

擬似コード（主要部分）：

```assembly
[org 0x7C00]
[bits 16]
start:
  cli
  ; A20 enable (0x92)
  in al, 0x92
  or al, 0x02
  out 0x92, al

  ; load GDT (defined below in this file)
  lgdt [gdt_descriptor]

  ; read kernel: INT 0x13 (CHS/LBA簡易、学習用に固定セクタ数)
  ; 目的地 ES:BX = 0x1000:0x0000 => 0x00100000
  ; ...（詳細は後続回で拡張）

  ; enter protected mode
  mov eax, cr0
  or eax, 1
  mov cr0, eax
  jmp dword 0x08:kernel_entry   ; 32bit far jump

; --- GDT ---
align 8
gdt_start:
  dq 0x0000000000000000
  dq 0x00CF9A000000FFFF  ; code
  dq 0x00CF92000000FFFF  ; data
gdt_end:
gdt_descriptor:
  dw gdt_end - gdt_start - 1
  dd gdt_start

times 510 - ($ - $$) db 0
dw 0xAA55
```

### ステップ 5: boot/kernel_entry.s（32bit, kmain 呼び出し）

`kernel_entry.s` は 32bit コードで、セグメント設定/スタック初期化後 `extern kmain` を呼び出します。

```assembly
[bits 32]
extern kmain
global kernel_entry

kernel_entry:
  mov ax, 0x10
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax
  mov ss, ax
  mov esp, 0x00300000
  call kmain
.halt:
  hlt
  jmp .halt
```

### ステップ 6: Makefile

-   `day03_complete/Makefile`を参照してください。

## トラブルシューティング

### Day 03 特有の問題

**🔴 kmain() が呼び出されない**

-   **原因**: カーネルのリンクアドレスと読み込み先アドレスの不一致
-   **確認方法**:
    ```bash
    objdump -h kernel.elf | grep text  # リンクアドレス確認
    hexdump -C os.img | head -20       # 読み込み内容確認
    ```
-   **解決策**: `ld -Ttext 0x00010000` と boot.s の読み込み先を一致させる

**🔴 C コンパイル エラー**

-   **問題**: `printf` や `strlen` を使用している
-   **解決策**: freestanding C 用の自作関数に置き換え
    ```c
    // ❌ printf("Hello");
    // ✅ vga_puts("Hello");
    ```

**🔴 リンク エラー**

-   **問題**: `_start` シンボルが見つからない
-   **解決策**: kernel_entry.s で `global kernel_entry` を宣言し、`-e kernel_entry` でリンク

**🔴 VGA 出力されない**

-   **原因 1**: VGA バッファアドレス（0xB8000）への書き込み失敗
-   **原因 2**: 2 バイト構造（文字+属性）を 1 バイトで書き込み
-   **確認方法**: QEMU モニタで `x/20x 0xB8000` を実行
-   **解決策**:
    ```c
    // ❌ *((char*)0xB8000) = 'A';
    // ✅ *((uint16_t*)0xB8000) = 'A' | (0x0F << 8);
    ```

### freestanding C 環境でのよくあるエラー

**コンパイル時エラー**:

```bash
# ❌ undefined reference to `printf`
# → vga_puts() を使用

# ❌ undefined reference to `strlen`
# → 手動実装または既存実装を使用

# ❌ implicit declaration of function 'outb'
# → io.h をインクルードまたは extern 宣言
```

**実行時エラー**:

-   **ハング**: 無限ループの末尾に `hlt` 命令があるか確認
-   **文字化け**: null 終端文字列の確認
-   **カーソル異常**: CRTC レジスタ書き込み順序の確認

## 理解度チェック

1. freestanding C と通常の C 実行環境の違いは？
2. VGA の 1 文字は何バイトで、どういう内訳か？
3. ハードウェアカーソルの位置はどの I/O ポートで制御する？
4. スクロール時にどのメモリ領域をどう移動する？

## 次のステップ

-   ✅ C ベースの VGA 描画 API
-   ✅ kmain() からの初期化・出力
-   ✅ boot.s は最小限（GDT/モード切替/呼び出し）

次回以降、カーネルの読み込み・配置を整理し、I/O 補助（outb/inb）の提供やビルド/リンクの改善（リンカスクリプト導入）、さらに割り込みやタイマへ進みます。
