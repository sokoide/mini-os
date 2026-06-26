# Day 04: VGA テキスト表示 🎨

## 本日のゴール

Day 03 で動くようになった freestanding C 環境の上に、**VGA テキストモードの表示ドライバ**を実装する。`vga_putc` / `vga_puts` / 色指定 / スクリーンクリア / スクロール / ハードウェアカーソル制御を備えた、`printf` の代替となる出力 API を完成させる。

## SWE 向けモチベーション

VGA テキストモードは **メモリマップド I/O** の最も古典的で分かりしい例です。デバイスの状態を「ポート I/O（outb/inb）」で、データを「メモリへの書き込み」で扱うという2種類のハードウェア通信方式が1つのデバイスに同居しています。これは Linux の frame buffer ドライバや console サブシステムの原型であり、MMIO（Memory Mapped I/O）の概念を実感を持って理解する最初のステップです。また「1文字 = 2バイト（文字コード+属性）」という固定フォーマットのパース/構築は、バイナリプロトコルを扱う SWE に共通の訓練になります。

## 前提と依存

- **Day 03**（freestanding C、`kmain`、`boot/boot.s` + `boot/kernel_entry.s` の分割構成）が完成していること。
- 32ビット C（ポインタ、配列、ヘッダと実装の分離）の基礎。
- Day 03 の成果物（boot.s / kernel_entry.s / kernel.c / Makefile）を出発点にする。

## 新しい概念

### VGA テキストバッファ（0xB8000）

VGA テキストモードでは、画面の 1 文字が **2 バイト**で構成され、画面全体が `0xB8000` から始まる連続メモリ（フレームバッファ）にマップされています。

- 下位 8bit = 文字コード（ASCII）
- 上位 8bit = 属性（下位 4bit=前景色 / 上位 4bit=背景色）

例: 位置 `(row, col)` のセルは `0xB8000 + 2*(row*80+col)` に `uint16_t` として書き込む。
```c
uint16_t cell = (uint8_t)c | ((attr & 0xFF) << 8);
```

### ハードウェアカーソル（CRTC レジスタ）

画面上の点滅カーソル位置は VGA の CRTC レジスタ（インデックスポート `0x3D4`、データポート `0x3D5`）で制御します。インデックス 14/15 に位置の上位/下位バイトを書き込みます。これが初めての **ポート I/O（outb）** の本格利用です。

## 実装ガイド

完成形は `day04_completed/` を参照。以下が構築するレイヤ。

### ステップ 1: vga.h（API 設計）

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

### ステップ 2: kernel.c（VGA 実装）

`vga.h` の API を実装する。要点:
- `vga_entry(c, attr)` で文字+属性を1ワードに合成。
- `vga_putc`: 改行（`\n`）で次行へ、右端で折り返し、`cursor_y >= VGA_HEIGHT` で1行スクロールアップ。
- `vga_move_cursor`: CRTC レジスタへ `outb` で位置を反映。

```c
void vga_putc(char c) {
    if (c == '\n') { /* 改行 + 必要ならスクロール */ }
    VGA_MEM[cursor_y * VGA_WIDTH + cursor_x] = vga_entry(c, color);
    if (++cursor_x >= VGA_WIDTH) { cursor_x = 0; cursor_y++; /* scroll */ }
    vga_move_cursor(cursor_x, cursor_y);
}
void vga_puts(const char* s) { while (*s) vga_putc(*s++); }
```

`outb` は `io.h` でインラインアセンブリにより提供する（Day 05 でシリアル制御にも再利用）。

### ステップ 3: kmain でデモ

```c
void kmain(void) {
    vga_init();
    vga_puts("Day 04: C-based VGA driver\n");
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_puts("Hello from C!\n");
}
```

## 動作確認

```bash
cd day04_completed
make run    # QEMU が起動し、カラフルなテキストとカーソル移動が確認できる
```

- 画面クリア → "Day 04: C-based VGA driver" → 黄色で "Hello from C!" が表示されれば成功。
- QEMU モニタ（`Ctrl+Alt+2`）で `x/20x 0xB8000` を実行すると、書き込んだセルが見える。

## トラブルシューティング

**🔴 VGA 出力されない**
- 原因1: `0xB8000` への書き込み失敗（アドレス間違い）。
- 原因2: 2バイト構造を1バイトで書いている。
- 解決: `*((uint16_t*)0xB8000) = 'A' | (0x0F << 8);`（`uint16_t` で書く）。

**🔴 文字化け / カーソル異常**
- null 終端の確認、CRTC レジスタへの書き込み順序（インデックス→データ）の確認。

**🔴 `implicit declaration of function 'outb'`**
- `io.h` をインクルード、または `extern void outb(uint16_t,uint8_t);` 宣言。

## 理解度チェック

1. VGA の1文字は何バイトで、どういう内訳か？
2. ハードウェアカーソルの位置はどの I/O ポートで制御する？
3. スクロール時にどのメモリ領域をどう移動する？
4. メモリマップド I/O とポート I/O の違いを、VGA のどの機能がそれぞれ使っているかで説明せよ。

## 次の day へのブリッジ

VGA で「目に見える出力」ができました。次は **Day 05: シリアルポート** でポート I/O（outb/inb）を本格的に使い、ホスト側へログを送れる「見えない出力経路」を確立します。これは以降のデバッグ（割り込み・スケジューラの観察）に不可欠な基盤になります。
