# Day 03: freestanding C への移行 🔧

## 本日のゴール

**Day 03 から開発手法が大きく変わります。** Day 01-02 のシンプルなアセンブリファイル構成から、C 言語を中心とした本格的な OS 開発に移行します。アセンブリで書いたブート処理から初めて C の関数 `kmain()` に制御を移し、**C コードが実行できること**を最小限の動作確認で示します。本格的な VGA ドライバは次の Day 04 で実装します。

## SWE 向けモチベーション

「アセンブリで書かれていた内核を高級言語（C）で書き直す」という移行は、OS 開発の歴史そのものです（初期の UNIX もアセンブリから C で書き直されました）。この日は、その移行の**接合点**を作ります。具体的には、(1) ブートコード（16bit → 32bit）を役割ごとに `boot/boot.s`・`boot/kernel_entry.s` に分割し、(2) 32bit の世界で `call kmain` によって C 関数を呼び出す、という構造を作ります。「アセンブリと高級言語の境界（ABI: 呼出規約・スタック配置）」を自分の手で設計する経験は、FFI やシステムコール境界を理解する SWE に直結します。

## 前提と依存

- **Day 01-02**（ブート、GDT、プロテクトモード移行）が完成していること。
- 32 ビット C（ポインタ、配列、ヘッダと実装の分離）の基礎。
- Day 02 の成果物（プロテクトモードへ移行するブートセクタ）を出発点にする。

## 新しい概念

### なぜ C 言語に移行するのか？

**Day 01-02 のアセンブリ中心開発の限界**

- **複雑性の急増**: VGA 制御、割り込み処理、メモリ管理等が加わると、アセンブリだけでは保守困難
- **型安全性の欠如**: レジスタやメモリ操作でのバグが発見しにくい
- **可読性の低下**: コードの意図が分かりにくく、チーム開発に不向き

**C 言語移行の利点**

- **型システム**: 構造体、ポインタ型による設計明確化
- **関数分割**: 機能をモジュール化し、テストしやすい構造
- **保守性**: 高レベル言語による意図の明確な表現
- **実用性**: Linux、Windows 等、実際の OS は C/C++で開発

### freestanding C 環境の理解

通常の C プログラム（例: Hello World）は OS の上に乗って動作します：

- OS がメモリ管理、ファイル I/O、標準ライブラリを提供
- `main()` が自動的に呼ばれる
- `printf()` や `malloc()` が使える

OS 開発では、自分自身が OS になるため：

- 標準ライブラリが存在しない（OS 自体が提供するもの）
- メモリ、I/O を直接制御する必要がある
- `kmain()` を手動で呼び出す

#### 通常の C 環境 vs freestanding C 環境

| 項目               | 通常の C 環境                             | freestanding C 環境（OS 開発）          |
| ------------------ | ----------------------------------------- | --------------------------------------- |
| **標準ライブラリ** | `printf`, `malloc`, `strlen` 等が利用可能 | **利用不可** - 自分で実装が必要         |
| **メモリ管理**     | `malloc`/`free` でヒープ管理              | **自前実装** - 物理メモリ直接管理       |
| **文字列操作**     | `strlen`, `strcpy` 等が利用可能           | **自前実装** - 1 文字ずつ手動処理       |
| **I/O 操作**       | `printf` でコンソール出力                 | **ハードウェア直接制御** - VGA/シリアル |
| **起動処理**       | `main()` から自動開始                     | **ブートローダーが手動呼び出し**        |
| **リンカ**         | OS がメモリ配置を決定                     | **明示的にメモリ配置を指定**            |

#### freestanding C で禁止/利用不可な要素

```c
// ❌ 以下は一切使用できません
printf("Hello");           // → vga_puts("Hello") で代替（Day 04）
int* ptr = malloc(100);    // → 配列または物理メモリ直接管理
strlen(str);               // → 手動でループしてカウント
```

#### コンパイル時の特別なフラグ

```bash
-ffreestanding          # 標準ライブラリを仮定しない
-nostdlib               # 標準ライブラリをリンクしない
-fno-stack-protector    # スタック保護機能を無効化
-m32                    # 32ビットコード生成
```

#### 制約がある理由

1. **OS がまだ存在しない**: `printf` や `malloc` は OS 機能に依存している
2. **メモリ管理未実装**: ヒープ管理システムがまだない
3. **ハードウェア直接制御**: VGA、シリアル等を直接操作する必要
4. **最小限の実行環境**: ブートローダーから呼び出される最小環境

## 進め方と全体像

```
boot/boot.s（16bit）:
  - A20有効化、GDT定義と登録、BIOSのINT 0x13でカーネルを読み込み
  - CR0.PE=1 → far jump（32bitオフセット）で kernel_entry へ

boot/kernel_entry.s（32bit）:
  - セグメント設定、スタック初期化、extern kmain を call

kernel.c: kmain() を実装（Day 03 では最小動作確認、Day 04 で VGA 本格実装）
```

ブート関連は最終形（day12_completed）の構成に合わせ、`boot/boot.s`（16bit ローダ）と `boot/kernel_entry.s`（32bit エントリ）に分割します。GDT は `boot.s` 内へ統合します。

## 実装ガイド

完成形は `day03_completed/` を参照。Day 03 では VGA ドライバを作らず、**C が動いたことの最小確認**にとどめます。

### ステップ 1: プロジェクト構造

```
day03_completed/
├── boot/
│   ├── boot.s           # 16bit: A20, GDT統合, INT 0x13 で kernel 読込, PE へ
│   └── kernel_entry.s   # 32bit: セグメント/スタック設定 → kmain()
├── kernel.c             # kmain（最小動作確認のみ）
└── Makefile             # boot と kernel を連結して os.img を作成
```

### ステップ 2: kernel.c（最小動作確認）

まだ VGA ドライバはありません。VRAM 先頭（`0xB8000`）に直接文字を書き込んで、C コードが実行されていることを視覚的に確認します。

```c
#include <stdint.h>
static volatile uint16_t* const VGA_MEM = (uint16_t*)0xB8000;

static inline uint16_t vga_entry(char c, uint8_t attr) {
    return (uint16_t)c | ((uint16_t)attr << 8);
}

void kmain(void) {
    VGA_MEM[0] = vga_entry('C', 0x0F);  // 白文字/黒背景
    VGA_MEM[1] = vga_entry('!', 0x0F);
    while (1) { __asm__ volatile("hlt"); }
}
```

画面左上に "C!" と表示されれば、アセンブリ → C の制御移行が成功した証拠です。

### ステップ 3: boot/boot.s（16bit, GDT 統合, kernel 読込, PE 切替）

- 割り込み無効化、A20 有効化（0x92）
- GDT を「ファイル内に定義」し、`lgdt` で登録
- BIOS `INT 0x13` でディスク第 2 セクタ以降を `0x00100000` へ読み込み
- `CR0.PE=1` でプロテクトモードへ → `jmp dword 0x08:kernel_entry` で 32bit へ

```assembly
[org 0x7C00]
[bits 16]
start:
  cli
  in al, 0x92
  or al, 0x02
  out 0x92, al          ; A20 enable
  lgdt [gdt_descriptor]
  ; ... INT 0x13 で kernel を 0x00100000 へ読込 ...
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

### ステップ 4: boot/kernel_entry.s（32bit, kmain 呼び出し）

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

### ステップ 5: Makefile

`day03_completed/Makefile` を参照。`kernel.c` を `-ffreestanding -nostdlib` でコンパイルし、`kernel_entry.s` とリンク、`boot.bin` と連結して `os.img` を生成します。

## 動作確認

```bash
cd day03_completed
make run    # QEMU が起動し、画面左上に "C!" が表示されれば成功
```

## トラブルシューティング

**🔴 kmain() が呼び出されない**

- 原因: カーネルのリンクアドレスと読み込み先アドレスの不一致
- 確認: `objdump -h kernel.elf | grep text` と boot.s の読み込み先を一致（`ld -Ttext 0x00010000`）

**🔴 C コンパイルエラー（`undefined reference to printf/strlen`）**

- 解決: freestanding C 用の自作関数に置き換え（`printf` → Day 04 の `vga_puts`）

**🔴 リンクエラー（`_start` シンボルが見つからない）**

- 解決: `kernel_entry.s` で `global kernel_entry` を宣言し、`-e kernel_entry` でリンク

## 理解度チェック

1. freestanding C と通常の C 実行環境の違いを 3 つ挙げよ。
2. アセンブリ（kernel_entry.s）から C の関数を呼び出すために必要なのは何か？（シンボル名・スタック・呼出規約の観点）
3. `-ffreestanding -nostdlib` フラグはそれぞれ何を抑えるか？
4. なぜ `main()` ではなく `kmain()` なのか？

## 次の day へのブリッジ

C コードが動くようになったら、次は **Day 04: VGA テキスト表示** で本格的な出力ドライバ（`vga_putc` / `vga_puts` / 色 / スクロール）を実装します。これにより、以降のデバッグ出力（`printf` 相当）が可能になります。
