#include "kernel.h"

#include <stdarg.h>

#include "error_types.h"
#include "keyboard.h"

// 新しい静的コンテキストインスタンス
static kernel_context_t k_context;

// その他の静的グローバル変数
static uint16_t* vga_buffer = (uint16_t*)VGA_MEMORY;

// VGAテキストモード表示管理（Day12互換）
static uint16_t cx, cy;
static uint8_t col = 0x0F;

// IDT関連の静的変数
static struct idt_entry idt[256];  // 256個の割り込みエントリ
static struct idt_ptr idtr;        // IDTレジスタ用構造体

/*
 * =================================================================================
 * 1. Low-Level I/O & Utilities
 * =================================================================================
 */

/*
 * I/O Port Operations `outb` and `inb` are defined as static inline in kernel.h
 */

/*
 * Serial Port (for debugging)
 * 【役割】COM1ポートを初期化してデバッグ出力を可能にする
 */
void init_serial(void) {
    outb(SERIAL_PORT_COM1 + 1, SERIAL_INT_DISABLE);  // 割り込み無効化
    outb(SERIAL_PORT_COM1 + 3,
         SERIAL_DLAB_ENABLE);  // DLAB有効化（ボーレート設定モード）
    outb(SERIAL_PORT_COM1 + 0,
         SERIAL_BAUD_38400_LOW);  // ボーレート下位: 38400 bps
    outb(SERIAL_PORT_COM1 + 1,
         SERIAL_BAUD_38400_HIGH);  // ボーレート上位: 38400 bps
    outb(SERIAL_PORT_COM1 + 3,
         SERIAL_8N1_CONFIG);  // 8bit, パリティなし, 1ストップビット
    outb(SERIAL_PORT_COM1 + 2,
         SERIAL_FIFO_ENABLE);  // FIFO有効化、クリア、14バイト閾値
    outb(SERIAL_PORT_COM1 + 4, SERIAL_MODEM_READY);  // IRQ有効化、RTS/DSR設定
}

/*
 * 1文字をシリアルポートに送信する関数
 * 【役割】送信バッファが空になるのを待ってから1文字送信する
 */
void serial_write_char(char c) {
    // 送信可能まで待機
    while ((inb(SERIAL_PORT_COM1 + 5) & SERIAL_TRANSMIT_READY) == 0);
    outb(SERIAL_PORT_COM1, c);
}

/*
 * 文字列をシリアルポートに送信する関数
 * 【役割】NULL終端文字まで文字列を1文字ずつ送信する
 */
void serial_write_string(const char* str) {
    while (*str) {
        serial_write_char(*str);
        str++;
    }
}

// ━━━ VGA テキストモード表示管理（Day12互換） ━━━
// VGAエントリ作成：文字と属性を16bitに合成
static inline uint16_t ve(char c, uint8_t a) {
    return (uint16_t)c | ((uint16_t)a << 8);
}

// VGA文字色設定：前景色と背景色を指定
void vga_set_color(vga_color_t f, vga_color_t b) {
    col = (uint8_t)f | ((uint8_t)b << 4);
}

// VGAカーソル移動：画面上の指定位置にカーソルを移動
void vga_move_cursor(uint16_t x, uint16_t y) {
    cx = x;
    cy = y;
    // 線形位置計算（Y * 幅 + X）
    uint16_t p = y * VGA_WIDTH + x;
    // VGAコントローラーレジスタに位置設定
    outb(0x3D4, 14);  // カーソル位置上位バイト
    outb(0x3D5, (p >> 8) & 0xFF);
    outb(0x3D4, 15);  // カーソル位置下位バイト
    outb(0x3D5, p & 0xFF);
}

// VGA画面クリア：全画面を空白文字で埋めカーソルを左上に
void vga_clear(void) {
    for (uint16_t y = 0; y < VGA_HEIGHT; y++)
        for (uint16_t x = 0; x < VGA_WIDTH; x++)
            vga_buffer[y * VGA_WIDTH + x] = ve(' ', col);
    vga_move_cursor(0, 0);
}

// VGA文字出力：1文字を現在のカーソル位置に表示
void vga_putc(char c) {
    if (c == '\n') {
        // 改行文字の場合、次の行の先頭に移動
        cx = 0;
        cy++;
        vga_move_cursor(cx, cy);
        return;
    }
    // 指定位置に文字を書き込み
    vga_buffer[cy * VGA_WIDTH + cx] = ve(c, col);
    // カーソルを次の位置に進める（行末で改行）
    if (++cx >= VGA_WIDTH) {
        cx = 0;
        cy++;
    }
    vga_move_cursor(cx, cy);
}

// VGA文字列出力：NULL終端文字列を順次表示
void vga_puts(const char* s) {
    while (*s) vga_putc(*s++);
}

// VGA数値出力：32bit整数を十進数文字列で表示
void vga_putnum(uint32_t n) {
    int i = 0;
    if (n == 0) {
        vga_putc('0');
        return;
    }
    uint32_t x = n;
    char r[10];  // 数字文字一時格納用配列
    // 下位桁から分離して配列に格納
    while (x) {
        r[i++] = '0' + (x % 10);
        x /= 10;
    }
    // 逆順で出力（上位桁から）
    while (i--) vga_putc(r[i]);
}

// VGA初期化：白文字・黒背景で画面クリア
void vga_init(void) {
    vga_set_color(15, 0);  // VGA_WHITE, VGA_BLACK
    vga_clear();
}

/*
 * 整数を文字列に変換する関数 (Integer to ASCII)
 * 【役割】指定された基数（10進数、16進数など）で整数を文字列に変換する
 */
void itoa(uint32_t value, char* buffer, int base) {
    char* p = buffer;
    char *p1, *p2;
    uint32_t digits = 0;

    if (value == 0) {
        *p++ = '0';
        *p = '\0';
        return;
    }

    while (value) {
        uint32_t remainder = value % base;
        *p++ = (remainder < 10) ? remainder + '0' : remainder + 'A' - 10;
        value /= base;
        digits++;
    }

    *p = '\0';

    // 文字列を逆順にする
    p1 = buffer;
    p2 = buffer + digits - 1;
    while (p1 < p2) {
        char tmp = *p1;
        *p1 = *p2;
        *p2 = tmp;
        p1++;
        p2--;
    }
}

/*
 * =================================================================================
 * 2. VGA Display & Debugging
 * =================================================================================
 */

/*
 * 画面クリア関数
 * 【役割】VGAテキストモードの画面を黒背景・白文字のスペースで埋める
 */
void clear_screen(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = VGA_WHITE_ON_BLACK;  // 黒背景、白文字、スペース
    }
}

/*
 * 指定行クリア関数
 * 【役割】指定された行をスペースで埋めてクリアする
 */
void clear_line(int row) {
    if (row < 0 || row >= VGA_HEIGHT)
        return;

    for (int col = 0; col < VGA_WIDTH; col++) {
        vga_buffer[row * VGA_WIDTH + col] = VGA_WHITE_ON_BLACK;
    }
}

/*
 * 指定位置への文字列表示関数
 * 【役割】指定された行・列に指定色で文字列を表示する
 */
void print_at(int row, int col, const char* str, uint8_t color) {
    if (row < 0 || row >= VGA_HEIGHT || col < 0 || col >= VGA_WIDTH) {
        return;  // 範囲外は無視
    }

    uint16_t* pos = vga_buffer + row * VGA_WIDTH + col;
    while (*str && col < VGA_WIDTH) {
        *pos++ = (*str++ | (color << 8));
        col++;
    }
}

/*
 * デバッグメッセージ表示関数
 * 【役割】シリアルポートとVGA画面の両方にデバッグメッセージを出力
 */

static void simple_vsprintf(char* out_buf, size_t buf_size, const char* format,
                            va_list args) {
    char* buf_ptr = out_buf;
    char* buf_end = out_buf + buf_size - 1;  // 1 for null terminator
    const char* fmt_ptr = format;

    while (*fmt_ptr) {
        if (buf_ptr >= buf_end)
            break;

        if (*fmt_ptr == '%') {
            fmt_ptr++;
            switch (*fmt_ptr) {
            case 's': {
                const char* s = va_arg(args, const char*);
                while (*s && buf_ptr < buf_end) {
                    *buf_ptr++ = *s++;
                }
                break;
            }
            case 'd':  // itoa handles unsigned, but for simplicity we can use
                       // it
            case 'u': {
                unsigned int u = va_arg(args, unsigned int);
                char num_buf[11];  // max 10 digits for 32-bit unsigned + null
                itoa(u, num_buf, 10);
                char* p = num_buf;
                while (*p && buf_ptr < buf_end) {
                    *buf_ptr++ = *p++;
                }
                break;
            }
            case 'x': {
                unsigned int x = va_arg(args, unsigned int);
                char num_buf[9];  // 8 hex digits + null
                itoa(x, num_buf, 16);
                char* p = num_buf;
                while (*p && buf_ptr < buf_end) {
                    *buf_ptr++ = *p++;
                }
                break;
            }
            case '%':
                if (buf_ptr < buf_end) {
                    *buf_ptr++ = '%';
                }
                break;
            }
        } else {
            if (buf_ptr < buf_end) {
                *buf_ptr++ = *fmt_ptr;
            }
        }
        fmt_ptr++;
    }
    *buf_ptr = '\0';
}

void debug_vprint(const char* format, va_list args) {
    char buffer[256];
    va_list ap;
    va_copy(ap, args);
    simple_vsprintf(buffer, sizeof(buffer), format, ap);
    va_end(ap);

    // シリアルポートに出力
    serial_write_string("[DEBUG] ");
    serial_write_string(buffer);
    serial_write_string("\r\n");

    // VGAにも出力（最下行に表示）
    // static int debug_row = 24;
    // print_at(debug_row, 0, buffer, VGA_COLOR_RED);
}

void debug_print(const char* format, ...) {
    va_list args;
    va_start(args, format);
    debug_vprint(format, args);
    va_end(args);
}

/*
 * システム情報表示関数
 * 【役割】システムの基本情報と操作説明を画面に表示
 */
void display_system_info(void) {
    print_at(0, 0, "Timer-based Multi-threaded OS with Context Switching",
             VGA_COLOR_WHITE);
    print_at(2, 0, "System Information:", VGA_COLOR_YELLOW);
    print_at(3, 2, "Timer Frequency: 100Hz (10ms intervals)", VGA_COLOR_GRAY);
    print_at(4, 2, "Scheduling: Preemptive Round-Robin", VGA_COLOR_GRAY);
    print_at(5, 2, "Context Switch: Hardware timer interrupt", VGA_COLOR_GRAY);

    print_at(7, 0, "Thread Information:", VGA_COLOR_YELLOW);
    print_at(8, 2,
             "Thread 1: Counter updates every 1.0 second, checking the counter "
             "every 10ms",
             VGA_COLOR_GRAY);
    print_at(9, 2,
             "Thread 2: Counter updates every 1.5 seconds, checking the "
             "counter every 10ms",
             VGA_COLOR_GRAY);
    print_at(10, 2,
             "Thread 3: Keyboard input thread blocked by BLOCK_REASON_KEYBOARD",
             VGA_COLOR_GRAY);

    print_at(12, 0, "Live Thread Status:", VGA_COLOR_RED);
}

/*
 * =================================================================================
 * 3. Interrupt & Timer System
 * =================================================================================
 */

/*
 * PIT（Programmable Interval Timer）初期化
 * 【役割】指定された周波数でタイマー割り込みを発生させる
 */
void init_timer(uint32_t frequency) {
    /*
     * PITの動作原理：
     * - PITは1.193180MHzで動作するカウンター
     * - 設定した値まで数えると割り込みを発生
     * - divisor = 基本周波数 / 目標周波数
     */
    uint32_t divisor = PIT_FREQUENCY / frequency;

    // PITにコマンド送信
    // PIT_MODE_SQUARE_WAVE = 00110110b
    // bit 7-6: チャンネル0選択
    // bit 5-4: アクセスモード（Lo/Hi byte）
    // bit 3-1: モード3（矩形波発生）
    // bit 0: BCD/バイナリ選択（バイナリ）
    outb(PIT_COMMAND, PIT_MODE_SQUARE_WAVE);

    // 分周値を下位・上位バイトの順で送信
    outb(PIT_CHANNEL0, divisor & MASK_LOW_BYTE);  // 下位8bit
    outb(PIT_CHANNEL0,
         (divisor >> SHIFT_HIGH_BYTE) & MASK_LOW_BYTE);  // 上位8bit

    print_at(20, 0, "Timer initialized: 100Hz (10ms intervals)",
             VGA_COLOR_GREEN);
}

/*
 * IDTエントリ設定関数
 * 【役割】指定された割り込み番号に対するハンドラ関数を登録
 */
void set_idt_gate(int n, uint32_t handler) {
    idt[n].base_low = handler & MASK_LOW_WORD;  // アドレスの下位16bit
    idt[n].base_high =
        (handler >> SHIFT_HIGH_WORD) & MASK_LOW_WORD;  // アドレスの上位16bit
    idt[n].selector = IDT_KERNEL_CODE_SEGMENT;  // カーネルコードセグメント
    idt[n].always0 = 0;
    idt[n].flags =
        IDT_FLAG_PRESENT_DPL0_32BIT;  // Present, DPL=0, 32bit Interrupt Gate
}

/*
 * IDT構造体設定とロード
 * 【役割】IDT構造体を設定してCPUにロードする
 */
void setup_idt_structure(void) {
    debug_print("IDT: IDT structure configured and loaded");

    // IDT構造体の設定
    idtr.limit = sizeof(idt) - 1;  // IDTのサイズ
    idtr.base = (uint32_t)&idt;    // IDTのアドレス

    // IDTをCPUにロード
    asm volatile("lidt %0" : : "m"(idtr));
}

/*
 * 割り込みハンドラ登録
 * 【役割】タイマーとキーボードの割り込みハンドラを登録
 */
void register_interrupt_handlers(void) {
    debug_print("IDT: Timer interrupt handler registered");
    debug_print("IDT: Keyboard interrupt handler registered");

    // タイマー割り込み（IRQ0 = 割り込み番号32）のハンドラ設定
    set_idt_gate(32, (uint32_t)timer_interrupt_handler);

    // キーボード割り込み（IRQ1 = 割り込み番号33）のハンドラ設定
    set_idt_gate(33, (uint32_t)keyboard_interrupt_handler);
}

/*
 * PIC再マップ関数
 * 【役割】PICの割り込みベクターを再マップして、CPU例外との衝突を回避
 */
void remap_pic(void) {
    debug_print("PIC: Starting PIC remapping");

    // PICを再マップ（IRQ0-7を割り込み32-39に移動）
    // 【重要】デフォルトではIRQ0-7は割り込み8-15にマップされ、CPU例外と衝突する

    // マスター PIC 初期化
    outb(PIC_MASTER_COMMAND, PIC_ICW1_INIT);  // 初期化コマンド (ICW1)
    outb(PIC_MASTER_DATA,
         PIC_ICW2_MASTER_BASE);  // 割り込みベクター0x20(32)から開始 (ICW2)
    outb(PIC_MASTER_DATA,
         PIC_ICW3_SLAVE_IRQ2);  // スレーブPICはIRQ2に接続 (ICW3)
    outb(PIC_MASTER_DATA, PIC_ICW4_8086_MODE);  // 8086モード (ICW4)

    debug_print("PIC: Master PIC remapped to interrupts 32-39");
}

/*
 * 割り込みマスク設定関数
 * 【役割】どの割り込みを有効/無効にするかを設定
 */
void configure_interrupt_masks(void) {
    debug_print("PIC: Configuring interrupt masks");

    // 全割り込みをマスク（無効化）
    outb(PIC_MASTER_DATA, PIC_MASK_ALL_DISABLED);  // 全割り込み無効化

    debug_print("PIC: All interrupts masked");
}

/*
 * タイマーとキーボード割り込み有効化
 * 【役割】タイマー（IRQ0）とキーボード（IRQ1）割り込みのみを有効化
 */
void enable_timer_interrupt(void) {
    debug_print("PIC: Enabling timer and keyboard interrupts");

    // タイマー割り込み（IRQ0）とキーボード割り込み（IRQ1）を有効化
    // bit 0 = 0: IRQ0（タイマー）有効
    // bit 1 = 0: IRQ1（キーボード）有効
    // bit 2-7 = 1: その他の割り込み無効
    outb(PIC_MASTER_DATA,
         PIC_MASK_TIMER_KEYBOARD);  // 11111100b = IRQ0とIRQ1のみ有効

    debug_print("PIC: Timer (IRQ0) and Keyboard (IRQ1) interrupts enabled");
}

/*
 * PIC初期化関数
 * 【役割】PICの設定を行い、タイマー割り込みを有効化
 */
void init_pic(void) {
    debug_print("PIC: Starting PIC initialization");

    // PIC初期化を4つのステップに分割
    remap_pic();                  // 1. PIC再マップ
    configure_interrupt_masks();  // 2. 割り込みマスク設定
    enable_timer_interrupt();     // 3. タイマー割り込み有効化

    debug_print("PIC: PIC configured: Timer interrupt enabled");
}

/*
 * 割り込みシステム初期化
 */
void init_interrupts(void) {
    debug_print("INTERRUPTS: Starting interrupt system initialization");

    // 割り込みシステム初期化を3つのステップに分割
    setup_idt_structure();          // 1. IDT構造体設定とロード
    register_interrupt_handlers();  // 2. 割り込みハンドラ登録

    // PICとタイマーを初期化
    init_pic();
    init_timer(TIMER_FREQUENCY);

    enable_cpu_interrupts();  // 3. CPU割り込み有効化

    debug_print("INTERRUPTS: Interrupt system initialized");
}

/*
 * CPU割り込み有効化関数
 * 【役割】CPUレベルで割り込みを有効化
 */
void enable_cpu_interrupts(void) {
    // 割り込み有効化
    asm volatile("sti");

    debug_print("CPU: Interrupts enabled");
}

/*
 * =================================================================================
 * 4. Thread Management & Scheduling
 * =================================================================================
 */

/*
 * スレッド作成時のパラメータバリデーション
 * 【役割】スレッド作成時のパラメータの妥当性をチェック
 * @return: 0=成功, -1=エラー
 */
os_result_t validate_thread_params(void (*func)(void), int display_row,
                                   uint32_t* delay_ticks) {
    if (!func) {
        debug_print("ERROR: create_thread called with NULL function pointer");
        return OS_ERROR_NULL_POINTER;
    }

    if (display_row < 0 || display_row >= VGA_HEIGHT) {
        debug_print("ERROR: create_thread called with invalid display_row");
        return OS_ERROR_INVALID_PARAMETER;
    }

    if (*delay_ticks == 0) {
        debug_print(
            "WARNING: create_thread called with delay_ticks=0, using 1");
        *delay_ticks = 1;
    }

    return OS_SUCCESS;
}

/*
 * スレッドスタック初期化関数
 * 【役割】コンテキストスイッチに必要なスタック構造を構築する
 */
void initialize_thread_stack(thread_t* thread, void (*func)(void)) {
    /*
     * スタック初期化の詳細:
     * timer_interrupt -> initial_context_switch -> thread_function の流れに対応
     *
     * initial_context_switch のpop命令の順序に合わせてスタックを構築:
     * 1. popa (eax, ecx, edx, ebx, esp, ebp, esi, edi)
     * 2. popfd (EFLAGS)
     * 3. pop eax; jmp eax (関数アドレス)
     *
     * スタックレイアウト（下から上へ）:
     * 1. 汎用レジスタ (EAX, EBX, ECX, EDX, ESI, EDI, EBP) - 最初にpopaで復元
     * 2. EFLAGS - 次にpopfdで復元
     * 3. 関数アドレス - 最後にpop eax; jmp eaxでジャンプ
     */

    // initial_context_switch で復元されるレジスタの初期値
    uint32_t* sp = &thread->stack[THREAD_STACK_SIZE];  // スタックトップから開始

    *--sp = (uint32_t)func;  // 関数アドレス（最初に積む）
    // 割り込み有効化のためのEFLAGS設定
    *--sp = EFLAGS_INTERRUPT_ENABLE;  // EFLAGS, IF=1（割り込み有効）, reserved
                                      // bit=1
    *--sp = 0;                        // EBP
    *--sp = 0;                        // EDI
    *--sp = 0;                        // ESI
    *--sp = 0;                        // EDX
    *--sp = 0;                        // ECX
    *--sp = 0;                        // EBX
    *--sp = 0;                        // EAX
    // ESPを設定 （context_switch が期待するスタック位置）
    thread->esp = (uint32_t)sp;
}

/*
 * スレッド属性設定関数
 * 【役割】スレッドの基本属性（状態、カウンター、表示行など）を設定する
 */
void configure_thread_attributes(thread_t* thread, uint32_t delay_ticks,
                                 int display_row) {
    thread->state = THREAD_READY;
    thread->counter = 0;
    thread->delay_ticks = delay_ticks;
    thread->last_tick = 0;
    thread->display_row = display_row;
    thread->next_ready = NULL;
}

/*
 * スレッドをREADYリストに追加する関数
 * 【役割】スレッドを実行可能（READY）リストの末尾に追加する（循環リスト）
 */
os_result_t add_thread_to_ready_list(thread_t* thread) {
    kernel_context_t* ctx = get_kernel_context();

    if (ctx->ready_thread_list == NULL) {
        ctx->ready_thread_list = thread;
        thread->next_ready = thread;
        debug_print("INFO: First thread added to list");
    } else {
        thread_t* last = ctx->ready_thread_list;
        int safety_counter = 0;

        // Prevent infinite loop in case of corrupted list
        while (last->next_ready != ctx->ready_thread_list &&
               safety_counter < MAX_THREADS) {
            last = last->next_ready;
            safety_counter++;
        }

        if (safety_counter >= MAX_THREADS) {
            debug_print("ERROR: Thread list appears corrupted");
            return OS_ERROR_INVALID_STATE;
        }

        thread->next_ready = ctx->ready_thread_list;
        last->next_ready = thread;
        // debug_print("INFO: Thread added to existing list");
    }

    return OS_SUCCESS;
}

/*
 * スレッド作成関数
 * 【役割】新しいスレッドを作成し、初期化して実行可能リストに追加する
 */
os_result_t create_thread(void (*func)(void), uint32_t delay_ticks,
                          int display_row, thread_t** out_thread) {
    static thread_t threads[MAX_THREADS];  // 最大4スレッド
    static int thread_count = 0;

    // 1. パラメータ検証
    if (!out_thread) {
        debug_print("ERROR: create_thread called with NULL out_thread pointer");
        return OS_ERROR_NULL_POINTER;
    }
    *out_thread = NULL;

    os_result_t validation_result =
        validate_thread_params(func, display_row, &delay_ticks);
    if (OS_FAILURE_CHECK(validation_result)) {
        return validation_result;
    }

    if (thread_count >= MAX_THREADS) {
        debug_print("ERROR: Maximum number of threads exceeded");
        return OS_ERROR_OUT_OF_MEMORY;
    }

    thread_t* thread = &threads[thread_count++];

    // 2. スタック初期化
    initialize_thread_stack(thread, func);

    // 3. スレッド属性設定
    configure_thread_attributes(thread, delay_ticks, display_row);

    // 4. READYリストに追加
    os_result_t add_result = add_thread_to_ready_list(thread);
    if (OS_FAILURE_CHECK(add_result)) {
        thread_count--;  // スレッド数をロールバック
        return add_result;
    }

    debug_print("SUCCESS: Thread created successfully");
    *out_thread = thread;
    return OS_SUCCESS;
}

/*
 * READYリストからスレッドを削除
 * 【役割】循環リストから現在のスレッドを安全に削除
 */
static void remove_from_ready_list(thread_t* thread) {
    kernel_context_t* ctx = get_kernel_context();

    if (ctx->ready_thread_list == thread && thread->next_ready == thread) {
        // 自分ひとりしかいなかった場合 → 空リストに
        ctx->ready_thread_list = NULL;
    } else {
        // 自分以外がいる場合 → リングから外す
        thread_t* prev = ctx->ready_thread_list;
        while (prev->next_ready != thread) {
            prev = prev->next_ready;
        }
        prev->next_ready = thread->next_ready;

        if (ctx->ready_thread_list == thread) {
            ctx->ready_thread_list = thread->next_ready;
        }
    }
}

/*
 * スレッドをスリープ状態に遷移させる関数
 * 【役割】スレッドの状態と起床時刻を設定する
 */

/*
 * sleep() システムコール関数
 * 【役割】指定されたティック数だけ現在のスレッドをスリープさせる
 */
void sleep(uint32_t ticks) {
    // パラメータ検証
    if (ticks == 0) {
        debug_print("SLEEP: Zero ticks - no sleep needed");
        return;
    }

    if (ticks > MAX_COUNTER_VALUE) {
        debug_print("SLEEP: Ticks too large, limiting");
        ticks = MAX_COUNTER_VALUE;
    }

    if (!get_current_thread()) {
        debug_print("SLEEP: No current thread to sleep");
        return;
    }

    uint32_t wake_up_time = get_system_ticks() + ticks;

    // 汎用ブロック関数を呼び出す
    block_current_thread(BLOCK_REASON_TIMER, wake_up_time);

    // スケジューラへ
    schedule();
}

/*
 * 現在のスレッドをブロックする汎用関数
 * 【役割】スレッドをブロックし、理由に応じてブロックリストに挿入する
 */
void block_current_thread(block_reason_t reason, uint32_t data) {
    asm volatile("cli");

    thread_t* thread = get_current_thread();
    if (!thread) {
        asm volatile("sti");
        return;
    }

    // 1. READYリストから削除
    remove_from_ready_list(thread);

    // 2. ブロック状態と理由を設定
    thread->state = THREAD_BLOCKED;
    thread->block_reason = reason;
    thread->next_blocked = NULL;

    // 3. ブロックリストに挿入
    if (reason == BLOCK_REASON_TIMER) {
        thread->wake_up_tick = data;
        // 時刻順でソートして挿入
        if (!get_kernel_context()->blocked_thread_list ||
            thread->wake_up_tick <
                get_kernel_context()->blocked_thread_list->wake_up_tick) {
            thread->next_blocked = get_kernel_context()->blocked_thread_list;
            get_kernel_context()->blocked_thread_list = thread;
        } else {
            thread_t* current = get_kernel_context()->blocked_thread_list;
            while (current->next_blocked &&
                   current->next_blocked->wake_up_tick <=
                       thread->wake_up_tick) {
                current = current->next_blocked;
            }
            thread->next_blocked = current->next_blocked;
            current->next_blocked = thread;
        }
    } else {  // FIFOで末尾に追加 (キーボードなど)
        if (!get_kernel_context()->blocked_thread_list) {
            get_kernel_context()->blocked_thread_list = thread;
        } else {
            thread_t* current = get_kernel_context()->blocked_thread_list;
            while (current->next_blocked) {
                current = current->next_blocked;
            }
            current->next_blocked = thread;
        }
    }

    asm volatile("sti");
}

/*
 * キーボード入力待ちでブロックされた全スレッドを起床させる
 * 【役割】キーボード入力があった時に、ブロック状態の全スレッドをREADYに戻す
 */
static void unblock_and_requeue_thread(thread_t* thread, thread_t* prev) {
    // blocked_listから削除
    if (prev) {
        prev->next_blocked = thread->next_blocked;
    } else {
        get_kernel_context()->blocked_thread_list = thread->next_blocked;
    }

    // READYリストに追加
    thread->state = THREAD_READY;
    thread->block_reason = BLOCK_REASON_NONE;
    thread->next_blocked = NULL;
    add_thread_to_ready_list(thread);
}

/*
 * タイマーでブロックされたスレッドをチェックして起床させる
 */
static void check_and_wake_timer_threads(void) {
    asm volatile("cli");
    thread_t* current = get_kernel_context()->blocked_thread_list;
    thread_t* prev = NULL;
    while (current) {
        thread_t* next = current->next_blocked;
        if (current->block_reason == BLOCK_REASON_TIMER &&
            current->wake_up_tick <= get_kernel_context()->system_ticks) {
            unblock_and_requeue_thread(current, prev);
        } else {
            prev = current;
        }
        current = next;
    }
    asm volatile("sti");
}

void unblock_keyboard_threads(void) {
    asm volatile("cli");
    thread_t* current = get_kernel_context()->blocked_thread_list;
    thread_t* prev = NULL;
    while (current) {
        thread_t* next = current->next_blocked;
        if (current->block_reason == BLOCK_REASON_KEYBOARD) {
            unblock_and_requeue_thread(current, prev);
        } else {
            prev = current;
        }
        current = next;
    }
}

/*
 * スケジューラ関数
 * 【役割】次に実行するスレッドを決定し、コンテキストスイッチを実行する
 * 【重要】タイマー割り込みから呼び出されるOSの心臓部
 */
/*
 * スケジューラのロック状態を管理する関数群
 */
static inline void acquire_scheduler_lock(void) {
    kernel_context_t* ctx = get_kernel_context();
    asm volatile("cli");
    ctx->scheduler_lock_count++;
    asm volatile("sti");
}

static inline void release_scheduler_lock(void) {
    kernel_context_t* ctx = get_kernel_context();
    asm volatile("cli");
    ctx->scheduler_lock_count--;
    asm volatile("sti");
}

static inline bool is_scheduler_locked(void) {
    return get_kernel_context()->scheduler_lock_count > 0;
}

/*
 * 初回スレッド選択とコンテキストスイッチ
 * 【役割】システム起動後の最初のスレッド実行を開始
 */
static void handle_initial_thread_selection(void) {
    kernel_context_t* ctx = get_kernel_context();

    asm volatile("cli");
    ctx->current_thread = ctx->ready_thread_list;
    ctx->current_thread->state = THREAD_RUNNING;
    asm volatile("sti");

    debug_print("SCHEDULER: First thread selected, starting multithreading");

    release_scheduler_lock();
    initial_context_switch(ctx->current_thread->esp);
    // この後には到達しない
}

/*
 * ラウンドロビンスケジューリングによるスレッド切り替え
 * 【役割】現在のスレッドから次の実行可能スレッドへ切り替え
 */
static void perform_thread_switch(void) {
    kernel_context_t* ctx = get_kernel_context();

    thread_t* old_thread = ctx->current_thread;
    thread_t* next_thread = ctx->current_thread->next_ready;

    // 実行可能な次のスレッドを見つける（BLOCKED状態をスキップ）
    thread_t* search_start = next_thread;
    while (next_thread && next_thread != old_thread) {
        // THREAD_READYの場合のみスレッドスイッチを実行
        if (next_thread->state == THREAD_READY) {
            asm volatile("cli");
            old_thread->state = THREAD_READY;
            next_thread->state = THREAD_RUNNING;
            ctx->current_thread = next_thread;
            asm volatile("sti");

            release_scheduler_lock();
            context_switch(&old_thread->esp, next_thread->esp);
            return;
        }

        // 次のスレッドを試す
        next_thread = next_thread->next_ready;

        // 一周してしまった場合（全スレッドが非READY状態）は終了
        if (next_thread == search_start) {
            break;
        }
    }

    // 実行可能なスレッドが見つからなかった場合
    // debug_print("SCHEDULER: No ready threads found for switch");
    release_scheduler_lock();
}

/*
 * ブロックされたスレッドからの強制スケジューリング
 * 【役割】現在のスレッドがBLOCKED/SLEEPINGの場合、強制的に次のREADYスレッドに切り替え
 */
static void handle_blocked_thread_scheduling(void) {
    kernel_context_t* ctx = get_kernel_context();
    thread_t* blocked_thread = ctx->current_thread;

    // 実行可能なスレッドを探す
    if (ctx->ready_thread_list &&
        ctx->ready_thread_list->state == THREAD_READY) {
        asm volatile("cli");
        ctx->ready_thread_list->state = THREAD_RUNNING;
        ctx->current_thread = ctx->ready_thread_list;
        asm volatile("sti");

        // debug_print("SCHEDULER: Switched to next ready thread from blocked
        // thread");
        release_scheduler_lock();
        // ブロックされたスレッドのコンテキストを保存してから切り替え
        context_switch(&blocked_thread->esp, ctx->current_thread->esp);
    } else {
        // 実行可能なスレッドがない場合は、システムを一時停止
        debug_print("SCHEDULER: No ready threads available, system idle");
        release_scheduler_lock();

        // CPUを停止してタイマー割り込みを待つ
        while (!ctx->ready_thread_list ||
               ctx->ready_thread_list->state != THREAD_READY) {
            asm volatile("hlt");  // 次の割り込みまでCPU停止
        }

        // 新しいREADYスレッドが復活したらスケジューラを再実行
        schedule();
    }
}

/*
 * メインスケジューラ関数
 * 【役割】スレッドスケジューリングの統合制御
 */
void schedule(void) {
    if (is_scheduler_locked()) {
        return;
    }

    acquire_scheduler_lock();
    check_and_wake_timer_threads();
    kernel_context_t* ctx = get_kernel_context();

    if (!ctx->ready_thread_list) {
        release_scheduler_lock();
        return;
    }

    if (!ctx->current_thread) {
        handle_initial_thread_selection();
        return;
    }

    // 現在のスレッドがブロック状態の場合、強制的に次のスレッドに切り替え
    if (ctx->current_thread->state == THREAD_BLOCKED) {
        handle_blocked_thread_scheduling();
        return;
    }

    perform_thread_switch();
}

/*
 * カーネルコンテキストへのアクセサ
 */
kernel_context_t* get_kernel_context(void) {
    return &k_context;
}

/*
 * 現在実行中のスレッドへのポインタを取得
 */
thread_t* get_current_thread(void) {
    return get_kernel_context()->current_thread;
}

/*
 * システムティック数を取得
 * 【役割】OS起動からの経過ティック数を返す
 * 【備考】スレッドのタイミング制御やsleep機能で使用される
 */
uint32_t get_system_ticks(void) {
    return get_kernel_context()->system_ticks;
}

/*
 * スレッドの共通カウンター更新処理
 * @param last_tick_ptr: 前回更新時のtick値へのポインタ
 * @param interval_ticks: 更新間隔（tick数）
 * スレッドの共通カウンター更新処理
 * @param last_tick_ptr: 前回更新時のtick値へのポインタ
 * @param interval_ticks: 更新間隔（tick数）
 * @param thread_name: 表示用スレッド名
 * @param display_row: 画面表示行
 * @return: カウンターが更新された場合は1、そうでなければ0
 */
int update_thread_counter(uint32_t* last_tick_ptr, uint32_t interval_ticks,
                          const char* thread_name, int display_row) {
    thread_t* self = get_current_thread();
    uint32_t current_ticks = get_system_ticks();

    if (self && current_ticks - *last_tick_ptr >= interval_ticks) {
        self->counter++;

        // カウンターが65535を超えたら0にリセット
        if (self->counter > MAX_COUNTER_VALUE) {
            self->counter = 0;
        }

        *last_tick_ptr = current_ticks;

        // 画面に表示
        char buffer[16];
        itoa(self->counter, buffer, 10);

        char display[40];
        int pos = 0;

        // スレッド名をコピー
        while (*thread_name) display[pos++] = *thread_name++;

        // カウンター値をコピー
        for (int i = 0; buffer[i]; i++) {
            display[pos++] = buffer[i];
        }

        // 残りをスペースで埋める
        while (pos < DISPLAY_LINE_LENGTH) display[pos++] = ' ';
        display[pos] = 0;  // null終端

        print_at(display_row, 2, display, VGA_COLOR_WHITE);
        return 1;  // 更新された
    }
    return 0;  // 更新されなかった
}

/*
 * =================================================================================
 * 5. High-Level Application Logic
 * =================================================================================
 */

/*
 * スレッド関数群
 * 【説明】各スレッドが実行する関数
 * 無限ループでHLT命令を実行し、割り込み待ちする
 */
void idle_thread(void) {
    // アイドルスレッド - システム情報表示とメインループ
    debug_print("KERNEL: System running... Watch the counters update!");
    debug_print("KERNEL: Each thread runs in 10ms time slices");
    debug_print("KERNEL: Idle thread running with HLT");

    while (1) {
        asm volatile("hlt");  // 割り込み待ち
    }
}

/*
 * スレッド関数1
 * 【役割】1.0秒間隔でカウンターを更新する
 */
static void threadA(void) {
    uint32_t last_tick = 0;

    while (1) {
        update_thread_counter(&last_tick, 100, "Thread A: ", 13);
        sleep(50);  // 500ms
    }
}

static void threadB(void) {
    uint32_t last_tick = 0;

    while (1) {
        update_thread_counter(&last_tick, 150, "Thread B: ", 14);
        sleep(75);  // 750ms
    }
}

/*
 * スレッド関数3
 * 【役割】キーボードからの入力を処理し、画面に表示するデモ
 */
static void threadC(void) {
    char input_buffer[64];
    char ch;

    // キーボード入力デモンストレーション
    print_at(15, 2,
             "Thread C: Keyboard Input Demo - Press keys:", VGA_COLOR_WHITE);
    print_at(16, 3, "Press 'q' to quit, Enter for string input",
             VGA_COLOR_GRAY);

    while (1) {
        print_at(17, 3, "Press a key (or 's' for string): ", VGA_COLOR_WHITE);
        ch = getchar();

        if (ch == 'q' || ch == 'Q') {
            print_at(18, 3, "Keyboard demo terminated.         ",
                     VGA_COLOR_RED);
            break;
        } else if (ch == 's' || ch == 'S') {
            print_at(18, 3, " Enter string: ", VGA_COLOR_YELLOW);
            read_line(input_buffer, sizeof(input_buffer));

            clear_line(19);
            print_at(19, 3, " You entered: ", VGA_COLOR_GREEN);
            print_at(19, 17, input_buffer, VGA_COLOR_CYAN);
        } else {
            char msg[32];
            msg[0] = 'K';
            msg[1] = 'e';
            msg[2] = 'y';
            msg[3] = ':';
            msg[4] = ' ';
            msg[5] = ch;
            msg[6] = ' ';
            msg[7] = '(';
            itoa((uint32_t)ch, &msg[8], 10);
            int len = 8;
            while (msg[len] != 0) len++;
            msg[len] = ')';
            msg[len + 1] = 0;

            clear_line(18);
            print_at(18, 3, msg, VGA_COLOR_MAGENTA);
        }

        sleep(5);  // 短い待機
    }

    // この行は決して実行されない
    asm volatile("hlt");
}

/*
 * カーネルコンテキスト初期化関数
 */
static void init_kernel_context(void) {
    k_context.current_thread = NULL;
    k_context.ready_thread_list = NULL;
    k_context.blocked_thread_list = NULL;
    k_context.system_ticks = 0;
    k_context.scheduler_lock_count = 0;
    debug_print("KERNEL: Context initialized");
}

/*
 * メインカーネル関数
 * 【役割】シリアルポート、画面、システム情報表示の初期化
 */
static void init_basic_systems(void) {
    init_serial();
    debug_print("KERNEL: Serial port initialized");

    clear_screen();
    debug_print("KERNEL: Screen cleared");

    display_system_info();
    debug_print("KERNEL: System info displayed");
}

/*
 * 割り込みとI/Oシステム初期化
 * 【役割】割り込みとキーボード初期化（順序が重要）
 */
static void init_interrupt_and_io_systems(void) {
    /*
     * 割り込みシステム初期化
     * 【重要】この時点からタイマー割り込みが発生し始める
     */
    debug_print("KERNEL: About to initialize interrupts");
    init_interrupts();
    debug_print("KERNEL: Interrupts initialized");

    /*
     * キーボード初期化
     * 【重要】割り込みシステム初期化後に実行する必要がある
     */
    debug_print("KERNEL: About to initialize keyboard");
    init_keyboard();
    debug_print("KERNEL: Keyboard initialized");
}

/*
 * スレッドシステム初期化
 * 【役割】すべてのスレッドを作成し、スレッドシステムを開始準備
 */
static void init_thread_system(void) {
    debug_print("KERNEL: About to create threads");

    // カーネルメインスレッドを作成（最初のスレッド）
    thread_t* kernel_thread;
    os_result_t result = create_thread(idle_thread, 1, 0, &kernel_thread);
    if (OS_FAILURE_CHECK(result)) {
        debug_print("FATAL: Failed to create kernel thread");
        while (1) asm volatile("hlt");  // システム停止
    }
    debug_print("KERNEL: Kernel thread created");

    thread_t* thread_a;
    result = create_thread(threadA, 100, 13, &thread_a);
    if (OS_FAILURE_CHECK(result)) {
        debug_print("ERROR: Failed to create thread A");
    } else {
        debug_print("KERNEL: Thread A created");
    }

    thread_t* thread_b;
    result = create_thread(threadB, 150, 14, &thread_b);
    if (OS_FAILURE_CHECK(result)) {
        debug_print("ERROR: Failed to create thread B");
    } else {
        debug_print("KERNEL: Thread B created");
    }

    thread_t* thread_c;
    result = create_thread(threadC, 200, 15, &thread_c);
    if (OS_FAILURE_CHECK(result)) {
        debug_print("ERROR: Failed to create thread C");
    } else {
        debug_print("KERNEL: Thread C created");
    }

    debug_print("KERNEL: Thread system initialized");
    debug_print("KERNEL: Waiting for timer interrupt to start scheduling");
}

/*
 * カーネルメインループ
 * 【役割】スレッドスケジューリング開始待ちとメインループ
 */
static void kernel_main_loop(void) {
    /*
     * スレッドシステム初期化完了
     * 【重要】current_thread は NULL のまま、最初のタイマー割り込みで
     * スケジューラが最初のスレッドを選択する
     */
    debug_print("KERNEL: Waiting for timer interrupt");

    // メインカーネルループ - タイマー割り込みを待つ
    while (1) {
        asm volatile("hlt");  // タイマー割り込み待ち
    }
}

/*
 * カーネルメイン関数（リファクタ済み）
 * 【役割】システム全体の初期化を段階的に実行
 */
void kernel_main(void) {
    init_kernel_context();
    init_basic_systems();
    init_interrupt_and_io_systems();
    init_thread_system();
    kernel_main_loop();
}

/*
 * =================================================================================
 * 6. Interrupt Handlers (C-level)
 * =================================================================================
 */

/*
 * タイマー割り込みハンドラ（C言語部分）
 * 【重要】この関数は10ms間隔で自動的に呼ばれる
 */
void timer_handler_c(void) {
    // PIC（Programmable Interrupt Controller）に割り込み処理完了を通知
    // これがないと次の割り込みが発生しない
    outb(PIC_MASTER_COMMAND, PIC_EOI);

    // デバッグ: 割り込みハンドラ実行確認（簡略版）
    static uint32_t interrupt_count = 0;
    interrupt_count++;
    if (interrupt_count % 100 == 0) {
        debug_print("TIMER: Timer interrupt fired 100 times");
    }

    get_kernel_context()->system_ticks++;  // システム時刻を更新

    /*
     * スケジューラ実行
     * 【重要】ここでスレッドの切り替えが発生する可能性
     * 【注意】タイマー割り込みハンドラ内では既に割り込みが無効化されているため
     * 追加のcliは不要。context_switch内でpopfdによりEFLAGSが復元され割り込み有効化される
     */
    schedule();
}
