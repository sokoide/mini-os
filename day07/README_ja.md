# Day 07: スレッドデータ構造（TCB）🧵

## 本日のゴール
マルチスレッドOSの基礎となるスレッド制御ブロック（TCB）とREADYリストをCで設計・実装する。

## 背景
Day6でタイマー割り込みを生成できるようになったが、OS開発では複数のタスクを並行して実行する必要がある。本日はスレッドの状態を管理するためのTCBデータ構造と、スレッドを管理するためのREADYリストを設計し、マルチスレッドの基盤を構築する。今日は設計が主眼で、次回で実際のコンテキストスイッチを実装する。

## 新しい概念
- **TCB (Thread Control Block)**: スレッドの状態を保存するためのデータ構造。レジスタ値、スタックポインタ、実行状態、優先度などの情報を含む。各スレッドに1つずつ割り当てられ、スケジューラがこれを操作。

## 学習内容

-   スレッド状態（READY/RUNNING/BLOCKED）とブロック理由
-   スレッド制御ブロック（TCB: Thread Control Block）の設計
-   スタック領域の確保と初期化設計（実装は次回のコンテキスト切替で活用）
-   READY リスト（循環リスト）管理の基本関数
-   スレッド生成 API（雛形）設計

【メモ】TCB の最小セット
- まずは `esp`（スタックポインタ）, `state`（READY/RUNNING/BLOCKED）, `next`/`next_ready`（READY循環用）, `next_blocked`（BLOCKED直鎖用）を揃えるのが最小構成です。

## タスクリスト
- [ ] スレッド状態（READY/RUNNING/BLOCKED）とブロック理由の列挙型を定義する
- [ ] TCB構造体を設計し、スタック、状態、カウンタなどのフィールドを定義する
- [ ] カーネルコンテキスト構造体を定義し、current_thread、ready_thread_listなどを管理する
- [ ] READYリストの循環リスト操作関数（push_back、pop_front）を実装する
- [ ] スレッド生成API（create_thread）を実装し、属性設定とREADY投入を行う
- [ ] kmainでデモスレッドを作成し、READYリストの初期化を確認する
- [ ] QEMUで動作確認し、リスト初期化のメッセージを表示する

## 前提知識の確認

### 必要な知識

-   Day 01〜06（ブート、GDT、VGA、IDT/例外、タイマ IRQ）
-   C の構造体、列挙型、ポインタの基礎

### 今日新しく学ぶこと

-   TCB の安全なレイアウト（スタックを先頭に置く理由）
-   循環単方向リストによる READY キュー
-   スレッドの属性（表示行、カウンタ、遅延など）

## 進め方と構成

Day 05/06 と同様、アセンブリは最小限（boot/ 配下）で、主に C でデータ構造を作ります。

```
├── boot
│   ├── boot.s           # 16bit: A20, GDT内蔵, INT 13hでkernel読込, PE切替
│   └── kernel_entry.s   # 32bit: セグメント/スタック設定 → kmain()
├── io.h                 # inb/outb/io_wait（既存流用）
├── kernel.c             # TCB定義、readyリスト、スレッド生成API雛形
├── vga.h                # VGA API（既存流用）
└── Makefile             # boot + kernel を連結
```

完成版の全体像や命名は `day99_completed/include/kernel.h` と `src/kernel.c` を参考にしてください（スレッド構造・ready キューの考え方が近いです）。

## 実装ガイド（例）

以下は実装指針です。実際のコードではソース内コメントを日本語で丁寧に記述しましょう。

### 1. スレッド状態とブロック理由（C）

```c
// スレッド状態
typedef enum {
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED
} thread_state_t;

// ブロック理由
typedef enum {
    BLOCK_REASON_NONE,
    BLOCK_REASON_TIMER,    // sleep() 等
    BLOCK_REASON_KEYBOARD  // 入力待ち 等
} block_reason_t;
```

### 2. スレッド制御ブロック（TCB）（C）

「スタックオーバーフローから ESP を守るため、配列 stack を最初に置く」など安全なレイアウトを意識します。

```c
#define MAX_THREADS       4
#define THREAD_STACK_SIZE 1024  // 4KB（uint32_t × 1024）

typedef struct thread {
    uint32_t stack[THREAD_STACK_SIZE]; // 先頭に配置（保護目的）
    thread_state_t state;              // スレッド状態
    uint32_t counter;                  // 任意のカウンタ（デモ用）
    uint32_t delay_ticks;              // カウンタ更新間隔（tick）
    uint32_t last_tick;                // 最後に更新したtick
    block_reason_t block_reason;       // ブロック理由
    uint32_t wake_up_tick;             // 起床予定tick
    int display_row;                   // 表示行
    struct thread* next_ready;         // READY循環リスト用
    struct thread* next_blocked;       // BLOCKEDリスト用
    uint32_t esp;                      // スタックポインタ（コンテキスト切替で利用）
} thread_t;
```

### 3. カーネル側コンテキスト（C）

「現在実行中」「READY リスト先頭」「BLOCKED リスト先頭」「tick」を一箇所に集約します。

```c
typedef struct {
    thread_t* current_thread;
    thread_t* ready_thread_list;   // 循環リストの先頭
    thread_t* blocked_thread_list; // 任意: 今日は未使用でも可
    uint32_t system_ticks;         // Day 06 のtickを参照
} kernel_context_t;

static kernel_context_t g_ctx = {0};
```

### 4. READY リスト操作（C）

循環単方向リストとして実装し、挿入・削除の基本を整えます。

```c
// リスト末尾に追加（先頭がNULLなら自分を指す1要素循環に）
static void ready_list_push_back(thread_t* t) {
    if (!t) return;
    if (!g_ctx.ready_thread_list) {
        g_ctx.ready_thread_list = t;
        t->next_ready = t;
        return;
    }
    thread_t* head = g_ctx.ready_thread_list;
    thread_t* last = head;
    while (last->next_ready != head) last = last->next_ready;
    t->next_ready = head;
    last->next_ready = t;
}

// 先頭をポップ（空:NULL, 1要素:NULLへ）
static thread_t* ready_list_pop_front(void) {
    thread_t* head = g_ctx.ready_thread_list;
    if (!head) return NULL;
    if (head->next_ready == head) {
        g_ctx.ready_thread_list = NULL;
        head->next_ready = NULL;
        return head;
    }
    thread_t* last = head;
    while (last->next_ready != head) last = last->next_ready;
    thread_t* ret = head;
    g_ctx.ready_thread_list = head->next_ready;
    last->next_ready = g_ctx.ready_thread_list;
    ret->next_ready = NULL;
    return ret;
}
```

### 5. スレッド生成 API（雛形）（C）

本格的なスタック初期化やコンテキスト切替は Day 08 以降に実装します。ここでは属性設定と READY 投入までを用意します。

```c
static thread_t g_threads[MAX_THREADS];
static int g_thread_count = 0;

static thread_t* alloc_thread_slot(void) {
    if (g_thread_count >= MAX_THREADS) return NULL;
    return &g_threads[g_thread_count++];
}

// スタック初期化は次回（Day 08）で本格実装
static void initialize_thread_stack_stub(thread_t* t, void (*func)(void)) {
    (void)func;
    t->esp = (uint32_t)&t->stack[THREAD_STACK_SIZE];
}

// 代表的な属性を設定してREADYへ入れる
int create_thread(void (*func)(void), uint32_t delay_ticks, int display_row, thread_t** out) {
    if (!func || !out) return -1;
    *out = NULL;
    thread_t* t = alloc_thread_slot();
    if (!t) return -2;

    // 属性
    t->state = THREAD_READY;
    t->counter = 0;
    t->delay_ticks = delay_ticks ? delay_ticks : 1;
    t->last_tick = 0;
    t->block_reason = BLOCK_REASON_NONE;
    t->wake_up_tick = 0;
    t->display_row = display_row;
    t->next_ready = NULL;
    t->next_blocked = NULL;

    initialize_thread_stack_stub(t, func);
    ready_list_push_back(t);
    *out = t;
    return 0;
}
```

### 6. デモ（kmain）（C）

当面は READY リストの作成までを確認し、表示で状況を可視化します。コンテキスト切替は Day 08 で追加。

```c
void demo_thread_func_A(void) { for(;;) { /* 実装はDay 08以降 */ } }
void demo_thread_func_B(void) { for(;;) { /* 実装はDay 08以降 */ } }

void kmain(void) {
    vga_init();
    vga_puts("Day 07: Thread TCB design\n");

    thread_t* t1 = NULL; thread_t* t2 = NULL;
    create_thread(demo_thread_func_A, 10, 10, &t1);
    create_thread(demo_thread_func_B, 20, 11, &t2);

    // READYリストの先頭要素の有無を簡易表示
    if (g_ctx.ready_thread_list) vga_puts("READY list initialized\n");

    for(;;) { __asm__ volatile ("hlt"); }
}
```

### 7. Makefile（例）

Day 05/06 と同じく、boot 分割＋ C カーネル構成でビルドします（割り込みは必要最小限）。

```makefile
AS   = nasm
CC   = i686-elf-gcc
LD   = i686-elf-ld
OBJCOPY = i686-elf-objcopy
QEMU = qemu-system-i386

CFLAGS = -ffreestanding -m32 -nostdlib -fno-stack-protector -fno-pic -O2 -Wall -Wextra

BOOT    = boot/boot.s
ENTRY   = boot/kernel_entry.s
KERNELC = kernel.c

BOOTBIN = boot.bin
KELF    = kernel.elf
KBIN    = kernel.bin
OS_IMG  = os.img

all: $(OS_IMG)
	@echo "✅ Day 07: TCB/READYリスト ビルド完了"
	@echo "🚀 make run で起動"

$(BOOTBIN): $(BOOT)
	$(AS) -f bin $(BOOT) -o $(BOOTBIN)

kernel_entry.o: $(ENTRY)
	$(AS) -f elf32 $(ENTRY) -o $@

kernel.o: $(KERNELC) vga.h io.h
	$(CC) $(CFLAGS) -c $(KERNELC) -o $@

$(KELF): kernel_entry.o kernel.o
	$(LD) -m elf_i386 -Ttext 0x00010000 -e kernel_entry -o $(KELF) kernel_entry.o kernel.o

$(KBIN): $(KELF)
	$(OBJCOPY) -O binary $(KELF) $(KBIN)

$(OS_IMG): $(BOOTBIN) $(KBIN)
	cat $(BOOTBIN) $(KBIN) > $(OS_IMG)

run: $(OS_IMG)
	$(QEMU) -fda $(OS_IMG) -boot a -serial stdio

clean:
	rm -f $(BOOTBIN) kernel_entry.o kernel.o $(KELF) $(KBIN) $(OS_IMG)

.PHONY: all run clean
```

## トラブルシューティング

-   READY リストが空のまま
    -   `create_thread` の戻り値や `ready_list_push_back` のロジックを確認
    -   `MAX_THREADS` 超過や `func==NULL` などのバリデーション
-   表示が出ない
    -   Day 03〜06 の初期化手順（boot/、VGA 初期化）を確認

## 理解度チェック

1. なぜ TCB の先頭に `stack[]` を置くと安全性が高まるのか？
2. 循環単方向リストで READY キューを持つメリットと注意点は？
3. `delay_ticks` / `last_tick` / `wake_up_tick` の役割は？
4. コンテキスト切替時に TCB のどのフィールドが参照されるか？

## 次のステップ

-   ✅ TCB（スレッド制御ブロック）の設計
-   ✅ READY リストの基本操作
-   ✅ スレッド生成 API の雛形

Day 08 ではコンテキストスイッチ（アセンブリ）を実装し、TCB の `esp` と初期スタックを活用して実際にスレッドを切り替えます。
