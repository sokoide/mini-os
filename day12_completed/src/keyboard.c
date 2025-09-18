// ============================================================================
// Keyboard Driver Implementation
// PS/2 Keyboard Input Handling with Ring Buffer
// ============================================================================

#include "keyboard.h"

#include "kernel.h"

#define NULL ((void*)0)

// ============================================================================
// 定数定義とグローバル変数
// ============================================================================

// スキャンコード→ASCII（US配列, Shift無視, makeのみ）
static const char scancode_map[128] = {
    0,   27,   '1',  '2', '3',  '4', '5', '6', '7', '8', '9', '0', '-',
    '=', '\b', '\t', 'q', 'w',  'e', 'r', 't', 'y', 'u', 'i', 'o', 'p',
    '[', ']',  '\n', 0,   'a',  's', 'd', 'f', 'g', 'h', 'j', 'k', 'l',
    ';', '\'', '`',  0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',',
    '.', '/',  0,    '*', 0,    ' ', 0,   0,   0,   0,   0,   0,   0,
    0,   0,    0,    0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,    0,    0,   0,    0,   0,   0,   0,   0,
};

// キーボードリングバッファ
static volatile char kbuf[KEYBOARD_BUFFER_SIZE];
static volatile uint32_t khead = 0, ktail = 0;

static inline int kbuf_is_empty(void) {
    return khead == ktail;
}

static inline int kbuf_is_full(void) {
    return ((khead + 1) % KEYBOARD_BUFFER_SIZE) == ktail;
}

static void kbuf_push(char c) {
    uint32_t nh = (khead + 1) % KEYBOARD_BUFFER_SIZE;
    if (nh != ktail) {
        kbuf[khead] = c;
        khead = nh;
    }
}

static int kbuf_pop(char* out) {
    if (kbuf_is_empty())
        return 0;
    *out = kbuf[ktail];
    ktail = (ktail + 1) % KEYBOARD_BUFFER_SIZE;
    return 1;
}

// ============================================================================
// 内部ヘルパー関数
// ============================================================================

// PS/2キーボード初期化
static inline int ps2_output_full_internal(void) {
    return (inb(0x64) & 0x01);
}

void ps2_keyboard_init(void) {
    // QEMUでは特別な初期化は不要な場合が多い。出力バッファを軽く掃除のみ。
    int guard = 32;
    while (ps2_output_full_internal() && guard--) {
        (void)inb(0x60);
    }
}

// ============================================================================
// パブリックAPI関数群
// ============================================================================

// キーボード初期化
void keyboard_init(void) {
    ps2_keyboard_init();
}

// キーボード割り込みハンドラ
void keyboard_handler_c(void) {
    // PIC に割り込み処理完了を通知
    outb(0x20, 0x20);

    // キーボードデータの読み取り可能性をチェック
    uint8_t status = inb(0x64);
    if (!(status & 0x01)) {
        serial_write_string(
            "KEYBOARD: Interrupt fired but no data available\n");
        return;
    }

    // スキャンコードを読み取り
    uint8_t scancode = inb(0x60);

    // キー離す操作は無視（break code）
    if (scancode & 0x80) {
        return;
    }

    // スキャンコードをASCII文字に変換
    char ch = (scancode < 128) ? scancode_map[scancode] : 0;

    if (ch != 0) {
        // バッファに格納
        kbuf_push(ch);

        // デバッグ出力
        serial_write_string("KEY: ");
        serial_write_char(ch);
        serial_write_string(" (");
        serial_puthex(scancode);
        serial_write_string(")\n");

        // 入力待ちスレッドをREADYへ（もしあれば）
        unblock_keyboard_threads();
    }
}

// ブロッキング文字入力
char getchar_blocking(void) {
    char c;
    for (;;) {
        if (kbuf_pop(&c))
            return c;

        // 入力が無ければブロック
        asm volatile("cli");
        if (kbuf_pop(&c)) {
            asm volatile("sti");
            return c;
        }
        block_current_thread(BLOCK_REASON_KEYBOARD, 0);
        asm volatile("sti");

        schedule();
    }
}

// キーボードバッファ空チェック
int keyboard_buffer_empty(void) {
    return kbuf_is_empty();
}

// 行入力関数 (day99_completed から移植・改良)
void read_line(char* buffer, int max_length) {
    // Enhanced input validation
    if (!buffer || max_length <= 1) {
        serial_write_string("read_line: Invalid parameters\n");
        return;  // 無効なパラメータ
    }

    // Additional safety: reasonable upper limit check
    if (max_length > 1024) {
        serial_write_string(
            "read_line: Buffer size too large, limiting to 1024\n");
        max_length = 1024;  // Prevent excessive buffer sizes
    }

    int pos = 0;
    char c;

    // Initialize buffer for safety
    buffer[0] = 0;

    while (pos < max_length - 1) {
        c = getchar_blocking();

        if (c == 10 || c == 13) {  // LF or CR
            // 改行で入力終了
            break;
        } else if (c == 8 && pos > 0) {  // Backspace
            // バックスペース処理
            pos--;
            // 画面からも文字を削除（バックスペース + スペース +
            // バックスペース）
            serial_write_char(8);
            serial_write_char(' ');
            serial_write_char(8);
        } else if (c >= 32 && c <= 126) {
            // 印刷可能文字のみ受け入れ
            buffer[pos] = c;
            pos++;
            // エコー表示
            serial_write_char(c);
        }
        // それ以外の制御文字は無視
    }

    buffer[pos] = 0;        // 文字列終端
    serial_write_char(10);  // 改行を出力
}

// PS/2出力バッファ確認
int ps2_output_full(void) {
    return ps2_output_full_internal();
}

// キーボード待機スレッドのブロック解除
void unblock_keyboard_threads(void) {
    asm volatile("cli");
    kernel_context_t* ctx = get_kernel_context();
    thread_t* current = ctx->blocked_thread_list;
    thread_t* prev = NULL;
    while (current) {
        thread_t* next = current->next_blocked;
        if (current->block_reason == BLOCK_REASON_KEYBOARD) {
            if (prev) {
                prev->next_blocked = current->next_blocked;
            } else {
                ctx->blocked_thread_list = current->next_blocked;
            }

            current->state = THREAD_READY;
            current->block_reason = BLOCK_REASON_NONE;
            current->next_blocked = NULL;
            add_thread_to_ready_list(current);
        } else {
            prev = current;
        }
        current = next;
    }
    asm volatile("sti");
}