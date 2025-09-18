# Day 05: 割り込みインフラストラクチャ ⚙️

## 本日のゴール

x86 割り込みシステムの基礎を実装し、IDT を構築して CPU 例外をハンドリングできる環境を整備する。

## 背景

Day4 までで freestanding C 環境が構築できたが、OS 開発ではプログラムエラーやハードウェアイベントを適切に処理する必要がある。本日は割り込みシステムの基礎として IDT を構築し、CPU 例外のハンドリング環境を整える。

## 新しい概念

-   **割り込み (Interrupt)**: CPU に対する「イベント通知」の仕組み。ハードウェアやソフトウェアからの非同期要求に応答し、現在の処理を一時中断してハンドラを実行。OS の応答性を高める核となる概念。
-   **IRQ (Interrupt ReQuest)**: 割り込み要求信号が通る物理的な線（割り込み線）。PIC が複数の IRQ を管理し、CPU に伝達。IRQ 0-15 が標準的に割り当てられており、それぞれキーボード、タイマー等のデバイスに対応。
-   **PIC (Programmable Interrupt Controller)**: 8259A チップで、複数のデバイスからの IRQ を束ねて CPU に効率的に伝達。BIOS 設定では IRQ0-15 を 32-47 番に再マップして使用し、OS の割り込み管理を可能にする。
-   **IDT (Interrupt Descriptor Table)**: 割り込みハンドラのアドレステーブル。CPU 例外やハードウェア割り込みが発生した際に、どの関数を呼び出すかを定義。

## 学習内容

-   **IDT 基礎**: Interrupt Descriptor Table の構造と役割の理解
-   **例外システム**: CPU 例外（0〜31）の分類とハンドリング方法
-   **アセンブリ連携**: ISR スタブと C ハンドラの協調動作
-   **テーブル管理**: `lidt` 命令による IDT 登録の実装
-   **デバッグ技術**: ソフト割り込み（`int 0x03`）によるデバッグ手法
-   **エラー処理**: CPU 例外からの情報表示と復旧方法

【用語整理】例外と IRQ の違い

-   例外（Exception）: CPU 内部の同期イベント（0 除算、無効オペコード、ページフォルト等）。命令実行の“その時点”で起きます。
-   IRQ（Interrupt Request）: デバイス由来の非同期イベント（タイマ、キーボード等）。外部からの信号で発生します。

【IDT ゲートの実体】

-   「何番の割り込みでどの関数に飛ぶか」を記すテーブルの 1 エントリ。32bit OS では“割り込みゲート(0x8E)”を使うのが基本です。

## タスクリスト

-   [ ] IDT の構造体を定義し、エントリを設定する関数を実装する
-   [ ] interrupt.s で ISR スタブ（0〜31）を作成し、レジスタ保存/復元を行う
-   [ ] kernel.c で IDT を初期化し、代表的な例外ハンドラを実装する
-   [ ] lidt 命令で IDT をプロセッサに登録する
-   [ ] ソフト割り込み（int 0x03）で例外処理をテストする
-   [ ] ゼロ除算などの CPU 例外でハンドラが動作することを確認する

## 前提知識の確認

### 必要な知識

-   **Day 01〜02**: ブートローダ、GDT、プロテクトモード切り替え
-   **Day 03〜04**: freestanding C、VGA/シリアル出力、インラインアセンブリ
-   **C 言語**: 構造体、関数ポインタ、ビット操作、ヘッダファイル管理
-   **アセンブリ基礎**: レジスタ、スタック操作、関数呼び出し規約

### 今日新しく学ぶこと

-   **IDT 構造**: エントリレイアウト（オフセット、セレクタ、属性フィールド）
-   **例外分類**: CPU 例外とハードウェア割り込みの違い
-   **エラーコード**: 例外によるエラー情報の有無と処理方法
    -   すべての例外がエラーコードを持つわけではない（ゼロ除算#DE などはなし）
    -   エラーコードあり: ページフォルト#PF（違反アドレス）、一般保護違反#GP など
    -   エラーコードなし: ゼロ除算#DE、無効オペコード#UD など
    -   ISR ではスタックを揃えるため、エラーコードなし例外でもダミー値を push
-   **レジスタ保存**: 割り込み時の CPU 状態保存・復旧メカニズム
-   **呼び出し規約**: アセンブリから C 関数への安全な移行方法

## 進め方と構成

Day 03/04 と同じく、アセンブリは最小限（boot/ 配下）に留め、C で IDT を管理します。

```
├── boot
│   ├── boot.s            # 16bit: A20, GDT内蔵, INT 13hでkernel読込, PE切替
│   ├── kernel_entry.s    # 32bit: セグメント/スタック設定 → kmain()
│   └── interrupt.s       # 例外ISRスタブ（0〜31）と共通入口
├── io.h                  # inb/outb/io_wait（Cインラインasm）
├── kernel.c              # IDT構築・例外ハンドラ・動作デモ
├── vga.h                 # VGA API（C）
└── Makefile              # boot + kernel + interrupt を連結して os.img を生成
```

### アーキテクチャ設計のポイント

#### C 中心開発の継続

Day 03/04 で確立したパターンを継続：

-   **最小アセンブリ**: 割り込みハンドラスタブのみアセンブリで記述
-   **C メイン実装**: IDT 管理、例外処理ロジックは C で実装
-   **段階的構築**: 基本例外から始めて、徐々にハードウェア割り込みへ拡張

#### 新規追加要素

-   **interrupt.s**: CPU 例外のアセンブリスタブ（ISR0〜ISR31）
-   **IDT 管理**: kernel.c 内の IDT 構造体と設定関数
-   **例外ハンドラ**: C による例外情報の解析と表示

完成版の参考は `day05_completed/` を確認してください。

## 実装ガイド

### 1. 割り込み処理の基本概念

#### x86 割り込みシステムの構造

x86 プロセッサの割り込みシステムは以下の要素で構成されます：

1. **IDT (Interrupt Descriptor Table)**: 割り込みハンドラのアドレステーブル
2. **IDT エントリ**: 各割り込み/例外のハンドラ情報を格納
3. **ISR (Interrupt Service Routine)**: 実際の割り込みハンドラコード
4. **IDTR**: IDT の場所とサイズを示すレジスタ

#### 処理の流れ

1. **例外発生** → CPU が自動的に IDT を参照
2. **ISR スタブ** → レジスタ保存、C ハンドラ呼び出し
3. **C ハンドラ** → 例外情報の解析と適切な処理
4. **復帰** → レジスタ復元、元の処理に戻る

### 2. IDT 構造体の実装

#### IDT エントリの構造

各 IDT エントリは 8 バイトの構造体で、以下の情報を含みます：

```c
// IDT エントリ（32ビット割り込みゲート）
struct idt_entry {
    uint16_t base_low;   // ハンドラアドレス下位16ビット
    uint16_t sel;        // セグメントセレクタ（通常は0x08）
    uint8_t  always0;    // 常に0（予約領域）
    uint8_t  flags;      // 属性フラグ（Present、DPL、Type）
    uint16_t base_high;  // ハンドラアドレス上位16ビット
} __attribute__((packed));
```

#### フラグフィールドの詳細

| ビット | 意味    | 説明                                     |
| ------ | ------- | ---------------------------------------- |
| 7      | Present | エントリが有効（1=有効、0=無効）         |
| 6-5    | DPL     | 特権レベル（0=カーネル、3=ユーザ）       |
| 4      | Storage | 常に 0（システム記述子）                 |
| 3-0    | Type    | ゲートタイプ（0xE=32bit 割り込みゲート） |

### 3. IDT 管理システム

```c
// ----- IDT 構築 -----
struct idt_entry {
    uint16_t base_low;   // ハンドラ下位16ビット
    uint16_t sel;        // セグメントセレクタ（通常0x08）
    uint8_t always0;     // 常に0
    uint8_t flags;       // 0x8E = present/特権0/32bit割り込みゲート
    uint16_t base_high;  // ハンドラ上位16ビット
} __attribute__((packed));


struct idt_ptr {
    uint16_t limit;  // IDTサイズ-1
    uint32_t base;   // IDT先頭アドレス
} __attribute__((packed));

#define IDT_SIZE 256
#define IDT_FLAG_PRESENT_DPL0_32INT 0x8E

static struct idt_entry idt[IDT_SIZE];
static struct idt_ptr idtr;

static inline void lidt(void* idtr_ptr) {
    __asm__ volatile("lidt (%0)" ::"r"(idtr_ptr));
}

static void set_idt_gate(int n, uint32_t handler) {
    idt[n].base_low = handler & 0xFFFF;
    idt[n].sel = 0x08;
    idt[n].always0 = 0;
    idt[n].flags = IDT_FLAG_PRESENT_DPL0_32INT;
    idt[n].base_high = (handler >> 16) & 0xFFFF;
}
static void load_idt(void) {
    idtr.limit = sizeof(idt) - 1;
    idtr.base = (uint32_t)&idt[0];
    lidt(&idtr);
}
```

### 2. ISR スタブ（アセンブリ）

`boot/interrupt.s` に例外 0〜31 のスタブを定義し、共通入口でレジスタを退避 →C 関数へ渡します。

```assembly
; 例外ISRスタブと共通ハンドラ（32bit）
[bits 32]

%macro ISR_NOERR 1        ; エラーコードなし例外
  global isr%1
isr%1:
  push dword 0            ; ダミーのエラーコードを積む
  push dword %1           ; ベクタ番号
  jmp isr_common
%endmacro

%macro ISR_ERR 1          ; エラーコードあり例外
  global isr%1
isr%1:
  push dword %1           ; ベクタ番号（エラーコードはCPUがpush）
  jmp isr_common
%endmacro

extern isr_handler_c      ; C側ハンドラ

isr_common:
  pusha                   ; 汎用レジスタ退避
  cld
  mov eax, esp            ; ESP上: [errcode][vec][pusha...] を渡す
  push eax
  call isr_handler_c
  add esp, 4
  popa
  add esp, 8              ; vec + errcode をpop
  iretd

; 代表的な例外
ISR_NOERR 0    ; Divide-by-zero
ISR_NOERR 3    ; Breakpoint
ISR_NOERR 6    ; Invalid Opcode
ISR_ERR   13   ; General Protection Fault
ISR_ERR   14   ; Page Fault
; ... 必要に応じて 31 まで定義
```

注意:

-   エラーコードがある例外（#DF, #TS, #NP, #SS, #GP, #PF など）は `ISR_ERR` を使います。
-   学習段階では代表的なもの（0, 3, 6, 13, 14 など）から始めても OK です。

### 3. C 側ハンドラ（例外表示）

`kernel.c` に以下のようなハンドラを用意します。VGA またはシリアルで情報を出力しましょう。

#### スタック状態の詳細図解

**割り込み発生時のスタック変化:**

```
割り込み発生前のスタック（ユーザーアプリケーション実行中）:
Higher Address
+------------------+
|   ユーザーデータ |
|       ...        |
+------------------+ <- ESP (アプリケーション)
|                  |
Lower Address


割り込み発生時（CPUが自動で積む）:
+------------------+
|   ユーザーデータ |
|       ...        |
+------------------+
|      EFLAGS      | <- CPU自動
+------------------+
|        CS        | <- CPU自動
+------------------+
|       EIP        | <- CPU自動
+------------------+
|   Error Code     | <- CPU自動（例外によっては無し）
+------------------+ <- ESP (割り込み時)


ISRスタブ処理後（pusha + ベクタ番号）:
+------------------+
|   ユーザーデータ |
+------------------+
|      EFLAGS      |
+------------------+
|        CS        |
+------------------+
|       EIP        |
+------------------+
|   Error Code     |
+------------------+
|   Vector Number  | <- ISRスタブがpush
+------------------+
|       EAX        | <- pusha
|       ECX        | <- pusha
|       EDX        | <- pusha
|       EBX        | <- pusha
|   ESP (dummy)    | <- pusha
|       EBP        | <- pusha
|       ESI        | <- pusha
|       EDI        | <- pusha
+------------------+ <- ESP (C関数呼び出し時)
```

**isr_stack 構造体とスタックの対応:**

```c
struct isr_stack {
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax; // pusha順（逆順）
    uint32_t int_no;   // ベクタ番号
    uint32_t err_code; // エラーコード（ない場合は0）
    // この上にCPUが積んだEIP, CS, EFLAGSがある
};

// C関数側での使用例
void isr_handler_c(struct isr_stack* frame) {
    // frame->eax = 割り込み時のEAXレジスタ値
    // frame->int_no = 割り込みベクタ番号（0=ゼロ除算, 3=ブレークポイント等）
    // frame->err_code = エラーコード（ページフォルトなら違反アドレス）

    vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
    vga_puts("[EXCEPTION] vec=");
    int n = frame->int_no;
    if (n >= 10) vga_putc('0' + (n/10));
    vga_putc('0' + (n%10));
    vga_puts(" err=");
    int e = frame->err_code;
    if (e >= 10) vga_putc('0' + (e/10));
    vga_putc('0' + (e%10));
    vga_puts("\n");
}
```

**エラーコードの有無による違い:**

```
エラーコード無しの例外（ISR_NOERR使用）:
例: ゼロ除算(#DE), ブレークポイント(#BP)

ISRスタブが実行する処理:
push dword 0     ; ダミーのエラーコードを積む
push dword %1    ; ベクタ番号
jmp isr_common   ; 共通処理へ

エラーコード有りの例外（ISR_ERR使用）:
例: 一般保護違反(#GP), ページフォルト(#PF)

ISRスタブが実行する処理:
                 ; CPUが既にエラーコードを積んでいる
push dword %1    ; ベクタ番号のみ追加
jmp isr_common   ; 共通処理へ
```

### 4. IDT 登録と動作テスト

-   `set_idt_gate(0, (uint32_t)isr0);` のように、例外スタブを IDT へ登録
-   `load_idt();` で `lidt`
-   テスト 1: `int 0x03`（ブレークポイント）を発行して表示を確認
-   テスト 2: 故意にゼロ除算して `vec=0` を確認

```c
extern void isr0(void);  // interrupt.s 側で定義
extern void isr3(void);
extern void isr6(void);
extern void isr13(void);
extern void isr14(void);

void init_idt_and_exceptions(void) {
    for (int i=0;i<IDT_SIZE;i++) set_idt_gate(i, 0);
    set_idt_gate(0,  (uint32_t)isr0);
    set_idt_gate(3,  (uint32_t)isr3);
    set_idt_gate(6,  (uint32_t)isr6);
    set_idt_gate(13, (uint32_t)isr13);
    set_idt_gate(14, (uint32_t)isr14);
    load_idt();
}

void kmain(void) {
    vga_init();
    vga_puts("Day 05: IDT & Exceptions\n");
    init_idt_and_exceptions();

    __asm__ volatile ("int $0x03");  // ソフト割り込み
    // ゼロ除算テスト: 下行をコメント解除
    // volatile int x=1, y=0; volatile int z = x / y;
}
```

### 5. Makefile（例）

-   Day 03/04 と同様に、boot 分割＋ C カーネル構成でビルドします （`interrupt.s` を含める）。
-   `day05_complete/Makefile`を参照してください。

## トラブルシューティング

-   画面が真っ黒
    -   GDT/PE 切替/ファージャンプの順序確認（Day 02/03 を参照）
    -   `lidt` の引数（`idtr`）の `limit`/`base` が正しいか
-   例外が表示されない
    -   IDT エントリの `flags=0x8E` と `sel=0x08` を確認
    -   ISR スタブのシンボル名と `set_idt_gate` 登録番号が一致しているか
-   `iret/iretd` でクラッシュ
    -   `isr_common` のスタック操作（push 順/加算量）が合っているか
    -   エラーコードの有無で `ISR_NOERR`/`ISR_ERR` を使い分けているか

## 理解度チェック

1. IDT エントリの各フィールド（base/sel/flags）の意味を説明できますか？
2. 例外のうち、エラーコードが自動で push されるものはどれですか？
3. `int 0x03` とハードウェア割り込み（IRQ）の違いは？
4. `lidt` 実行時に渡す `limit` と `base` の値はどのように決まりますか？

## 次のステップ

-   ✅ IDT の基本構築（例外ハンドラ登録）
-   ✅ ISR スタブ（アセンブリ）と C ハンドラ連携
-   ✅ ソフト割り込み/例外の可視化

Day 06 では PIC/PIT を導入し、ハードウェア割り込み（タイマ IRQ0 など）を扱えるようにします（PIC の再マッピング、割り込みマスク、PIT 設定）。
