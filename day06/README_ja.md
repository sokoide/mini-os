# Day 06: タイマー割り込み（PIC/PIT） ⏱️

## 本日のゴール

ハードウェアタイマー（PIT）と割り込みコントローラ（PIC）を初期化し、100Hz のタイマー割り込みを発生させて動作を確認する。

## 背景

Day5 で IDT を構築して CPU 例外をハンドリングできるようになったが、OS 開発ではハードウェアからの非同期割り込みも処理する必要がある。本日は PIC を再マッピングし、PIT で定期的なタイマー割り込みを生成して、ハードウェア割り込みの処理環境を整える。

## 新しい概念

-   **PIT (Programmable Interval Timer)**: 8254 チップで、定期的に割り込みを発生させるタイマー装置。基準クロック 1.193182MHz を分周して任意の周期を生成。チャンネル 0 がシステムタイマーとして使用され、スケジューリングの基盤となる。
-   **PIC (Programmable Interrupt Controller)**: 8259A チップで、複数のデバイスからの IRQ を束ねて CPU に効率的に伝達。BIOS 設定では IRQ0-15 を 32-47 番に再マップして使用し、OS の割り込み管理を可能にする。
-   **EOI (End Of Interrupt)**: 割り込み処理完了を PIC に通知するためのコマンド。次の割り込みを許可するために必要。

## 学習内容

-   PIC（プログラマブル割り込みコントローラ）8259A の再マッピング（IRQ0→INT 32 など）
    -   PIC とは: 外部デバイスからの割り込み信号を CPU に伝えるコントローラ
    -   再マッピング: BIOS 初期設定の割り込みベクタ（0-15）を OS 用に 32-47 へ変更
-   PIT（プログラマブルインターバルタイマ）8254 の初期化（Divisor 設定）
    -   PIT とは: 定期的な割り込みを生成するタイマーチップ
    -   Divisor: クロック周波数を分周して割り込み間隔を設定（100Hz=10ms 間隔）
-   IRQ（外部割り込み）ハンドラの登録・EOI（End Of Interrupt）
    -   IRQ: Interrupt Request、ハードウェアからの割り込み要求
    -   EOI: 割り込み処理完了を PIC に通知、次の割り込みを許可
-   CPU 割り込み有効化（`sti`）と割り込みの流れ
    -   sti: Set Interrupt Flag、CPU が割り込みを受け付けるよう設定
    -   割り込み流れ: デバイス →PIC→CPU→ISR ハンドラ →EOI

## タスクリスト

-   [ ] PIC を再マッピングして IRQ0-15 を IDT の 32-47 に割り当てる
-   [ ] PIT を初期化して 100Hz（10ms 周期）のタイマー割り込みを設定する
-   [ ] interrupt.s に IRQ0 スタブを追加し、レジスタ保存/復元を行う
-   [ ] kernel.c でタイマー割り込みハンドラを実装し、tick カウンタを更新する
-   [ ] IDT に IRQ0（ベクタ 32）を登録し、割り込み処理を有効化する
-   [ ] EOI を適切なタイミングで PIC に送信する
-   [ ] sti 命令で CPU 割り込みを有効化し、ハートビート表示を確認する

## 前提知識の確認

### 必要な知識

-   Day 01〜05（ブート、GDT、プロテクトモード、VGA、IDT/例外）
-   I/O ポートアクセス（`inb/outb`）

### 今日新しく学ぶこと

-   PIC の ICW1〜ICW4 による初期化手順とマスク設定
-   PIT のコマンドレジスタ（0x43）とチャンネル 0（0x40）への分周値設定
-   ハードウェア割り込みのハンドラからの EOI 発行

## 進め方と構成

Day 05 と同様、アセンブリは最小限（boot/ 配下）に留め、C で PIC/PIT/IDT の設定を行います。

```
├── boot
│   ├── boot.s            # 16bit: A20, GDT内蔵, INT 13hでkernel読込, PE切替
│   ├── kernel_entry.s    # 32bit: セグメント/スタック設定 → kmain()
│   └── interrupt.s       # 例外 + IRQスタブ（IRQ0）と共通入口
├── io.h                  # inb/outb/io_wait（Cインラインasm）
├── kernel.c              # PIC再マッピング + PIT設定 + IDT登録 + ハンドラ + デモ
├── vga.h                 # VGA API（C）
└── Makefile              # boot + kernel + interrupt を連結して os.img を生成
```

完成版は `day99_completed` を参考にしてください（PIC/PIT/IDT の役割分担とハンドラ構成）。

## 実装ガイド（例）

以降は実装の指針です。実際のコードではソース内コメントを日本語で丁寧に記述しましょう。

### 1. PIC の再マッピング（C）

目的: IRQ0〜IRQ15 を IDT の 32〜47 に再配置（CPU 例外 0〜31 と衝突しないように）

```c
#include "io.h"

#define PIC1_CMD  0x20  // マスタPIC コマンド
#define PIC1_DATA 0x21  // マスタPIC データ
#define PIC2_CMD  0xA0  // スレーブPIC コマンド
#define PIC2_DATA 0xA1  // スレーブPIC データ

#define PIC_EOI   0x20  // End Of Interrupt

static void remap_pic(void) {
    uint8_t a1 = inb(PIC1_DATA); // 現マスク保存
    uint8_t a2 = inb(PIC2_DATA);

    // ICW1: 初期化, エッジトリガ, ICW4必要
    outb(PIC1_CMD, 0x11);
    outb(PIC2_CMD, 0x11);

    // ICW2: 割り込みベクタオフセット
    outb(PIC1_DATA, 0x20); // マスタ: 0x20 (32)
    outb(PIC2_DATA, 0x28); // スレーブ: 0x28 (40)

    // ICW3: カスケード配線
    outb(PIC1_DATA, 0x04); // マスタ: IRQ2 にスレーブ接続
    outb(PIC2_DATA, 0x02); // スレーブ: ID=2

    // ICW4: 8086/88 モード
    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);

    // マスク復元（最初はIRQ0だけ有効にするなら 0xFE/0xFF を書く）
    outb(PIC1_DATA, a1);
    outb(PIC2_DATA, a2);
}

static void set_irq_masks(uint8_t master_mask, uint8_t slave_mask) {
    outb(PIC1_DATA, master_mask); // 0=有効, 1=無効
    outb(PIC2_DATA, slave_mask);
}

static inline void send_eoi_master(void) { outb(PIC1_CMD, PIC_EOI); }
static inline void send_eoi_slave(void)  { outb(PIC2_CMD, PIC_EOI); }
```

例: 最初は IRQ0（タイマ）だけ有効にしたい → `set_irq_masks(0xFE, 0xFF);`

### 2. PIT の初期化（C）

目的: 周波数 100Hz（10ms）で割り込みを発生させる。
PIT クロックは 1,193,182 Hz → 分周値 `div = 1193182 / 100 = 11931(余り)`（おおよそ 11932 で OK）。

```c
#define PIT_CH0   0x40
#define PIT_CMD   0x43

static void init_pit_100hz(void) {
    uint16_t div = 11932; // 100Hz 近似
    outb(PIT_CMD, 0x36);  // チャンネル0, ローハイ, モード3(方形波), バイナリ
    outb(PIT_CH0, div & 0xFF);       // LSB
    outb(PIT_CH0, (div >> 8) & 0xFF); // MSB
}

【メモ】0x36 コマンドの内訳
- ch0 / LSB→MSB / モード3（方形波）/ バイナリモード を意味します。
```

### 3. IRQ0（タイマ）スタブ（interrupt.s）

Day 05 の `interrupt.s` に IRQ0 のスタブを追加します（例外と同じ共通入口を使う）。

```assembly
[bits 32]

global irq0
irq0:
  push dword 0          ; ダミーerr
  push dword 32         ; ベクタ番号（再マップ後のIRQ0）
  jmp isr_common        ; 例外と同じ共通入口へ
```

IDT への登録時は `set_idt_gate(32, (uint32_t)irq0);` を呼びます。

### 4. IDT 登録とハンドラ（C）

IDT は Day 05 と同様に構築し、IRQ0（ベクタ 32）を登録します。ハンドラでは `tick` カウンタを増やし、EOI を PIC に通知します（スレーブ側ではないのでマスタ EOI で OK）。

````c
extern void irq0(void);

static volatile uint32_t tick = 0;

void irq_handler_timer(void) {
    tick++;
    if ((tick % 100) == 0) { // 約1秒ごと（100Hz）
        vga_puts(".");
    }
    send_eoi_master();
}

// isr_common から C に来る共通ハンドラ（例）
struct isr_stack { uint32_t regs[8]; uint32_t int_no; uint32_t err_code; };
void isr_handler_c(struct isr_stack* f) {
    if (f->int_no == 32) { irq_handler_timer(); return; }
    // それ以外（例外など）は Day 05 の処理に委ねる
}

static void idt_init_with_timer(void) {
    // 省略: Day 05 と同様に idt[] を0クリアし、例外・必要なIRQを登録
    set_idt_gate(32, (uint32_t)irq0);
    load_idt();
}
```

### 補足: EOI（End Of Interrupt）とは？どう使う？

EOI は「割り込み処理が完了した」ことを PIC（8259A）に通知するためのコマンドです。PIC は各 IRQ ラインに対して「サービス中（In-Service）」ビットを持っており、EOI を受け取るまで同じ IRQ を再度 CPU に通しません。したがって、EOI を出し忘れると次回以降の同一 IRQ が届かず、タイマーが止まったように見える、キーボードが反応しなくなる、といった症状が発生します。

- 基本形
  - マスタ PIC（IRQ0–7）: `outb(PIC1_CMD, 0x20)`
  - スレーブ PIC（IRQ8–15）: まずスレーブへ EOI、続けてマスタへ EOI（カスケード解放）

- 送るタイミング
  - 短いハンドラ（カウント増加・軽いログ等）なら、処理の最後で送っても良い。
  - ただし、ハンドラ内で重い処理やスレッド切替（スケジューリング）を行う可能性がある場合は、先に EOI を送るのが実用的です。先に EOI を送っておけば、切替後も次の IRQ が失われず、タイマー等が継続して動作します。
  - 本カリキュラムでは Day 09 でプリエンプションを導入するため、IRQ0（PIT）については「先に EOI → その後に `tick++` や `schedule()`」という順序を推奨しています。

- よくある落とし穴
  - EOI を送らない（またはスレーブ割り込みでマスタへの EOI を忘れる）
  - ハンドラ内で無限ループ等があり、EOI に到達しない
  - Auto-EOI モード（自動EOI）を有効にせずに EOI を省略する（本教材では扱いません）

- コード例（PIT/IRQ0: マスタのみ）
  ```c
  void timer_handler_c(void) {
      send_eoi_master();       // 先にEOIを送って次回IRQを保証
      tick++;
      if (tick - last >= slice) { /* schedule(); */ }
  }
````

### 5. kmain の初期化順序

```c
void kmain(void) {
    vga_init();
    vga_puts("Day 06: Timer IRQ (100Hz)\n");

    remap_pic();
    set_irq_masks(0xFE, 0xFF);  // IRQ0のみ許可
    idt_init_with_timer();
    init_pit_100hz();

    __asm__ volatile ("sti");   // CPU割り込み許可

    // メインループ（何もしない）
    for(;;) { __asm__ volatile ("hlt"); }
}
```

### 6. Makefile（例）

-   Day 05 と同様に、boot 分割＋ C カーネル構成でビルドします（`interrupt.s` を含める）。
-   `day06_complete/Makefile`を参照してください。

```makefile
AS   = nasm
CC   = i686-elf-gcc
LD   = i686-elf-ld
OBJCOPY = i686-elf-objcopy
QEMU = qemu-system-i386

CFLAGS = -ffreestanding -m32 -nostdlib -fno-stack-protector -fno-pic -O2 -Wall -Wextra

BOOT    = boot/boot.s
ENTRY   = boot/kernel_entry.s
INTRASM = boot/interrupt.s   # IRQ0スタブを追加
KERNELC = kernel.c

BOOTBIN = boot.bin
KELF    = kernel.elf
KBIN    = kernel.bin
OS_IMG  = os.img

all: $(OS_IMG)
	@echo "✅ Day 06: タイマー割り込み ビルド完了"
	@echo "🚀 make run で起動"

$(BOOTBIN): $(BOOT)
	$(AS) -f bin $(BOOT) -o $(BOOTBIN)

kernel_entry.o: $(ENTRY)
	$(AS) -f elf32 $(ENTRY) -o $@

interrupt.o: $(INTRASM)
	$(AS) -f elf32 $(INTRASM) -o $@

kernel.o: $(KERNELC) vga.h io.h
	$(CC) $(CFLAGS) -c $(KERNELC) -o $@

$(KELF): kernel_entry.o interrupt.o kernel.o
	$(LD) -m elf_i386 -Ttext 0x00010000 -e kernel_entry -o $(KELF) kernel_entry.o interrupt.o kernel.o

$(KBIN): $(KELF)
	$(OBJCOPY) -O binary $(KELF) $(KBIN)

$(OS_IMG): $(BOOTBIN) $(KBIN)
	cat $(BOOTBIN) $(KBIN) > $(OS_IMG)

run: $(OS_IMG)
	$(QEMU) -fda $(OS_IMG) -boot a -serial stdio

clean:
	rm -f $(BOOTBIN) kernel_entry.o interrupt.o kernel.o $(KELF) $(KBIN) $(OS_IMG)

.PHONY: all run clean
```

## トラブルシューティング

-   ピリオドが出ない（ハートビートが見えない）
    -   `sti` を呼んでいるか、`hlt` で CPU をアイドルさせているか
    -   PIC のマスクが正しいか（`0xFE` / `0xFF`）
    -   IDT のベクタ 32 に `irq0` を登録済みか
-   例外で止まる
    -   `isr_common` のスタック処理と C 側の解析が一致しているか
    -   例外と IRQ のベクタ番号（0〜31 / 32〜47）が混ざっていないか

## 理解度チェック

1. なぜ PIC の再マッピングが必要なのか？
2. 100Hz の分周値をどのように計算するか？
3. EOI はどこに、いつ送るのか？
4. 例外と IRQ のハンドラはどのように区別しているか？

## 次のステップ

-   ✅ PIC の再マッピングとマスク設定
-   ✅ PIT の初期化（100Hz）
-   ✅ IRQ0（タイマ）ハンドラでの心拍表示

Day 07 ではスレッドやタスクの土台へ繋げるため、タイマティックを用いた時間管理（チックカウンタ）を応用していきます。
