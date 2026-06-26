#include "keyboard.h"

#include "kernel.h"

// キーボード関連の静的変数
static keyboard_buffer_t kbd_buffer;         // キーボード入力バッファ
static volatile bool shift_pressed = false;  // Shiftキーの状態

// スキャンコード→ASCII変換テーブル（US配列）
static const char scancode_to_ascii[] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 8,
    9,   'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 10,  0,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', 39,  '`', 0,   92,  'z',
    'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,   '*', 0,   ' '};

// Shift押下時のスキャンコード→ASCII変換テーブル
static const char scancode_to_ascii_shift[] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 8,
    9,   'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 10,  0,
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', 34,  '~', 0,   '|', 'Z',
    'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,   '*', 0,   ' '};

/*
 * キーボードバッファ初期化関数
 * 【役割】リングバッファを初期化する
 */
void init_keyboard_buffer(void) {
    kbd_buffer.head = 0;
    kbd_buffer.tail = 0;
    debug_print("KEYBOARD: Buffer initialized");
}

/*
 * キーボードバッファ書き込み関数
 * 【役割】バッファに文字を追加（プロデューサー）
 */
void keyboard_buffer_put(char c) {
    int next_head = (kbd_buffer.head + 1) % KEYBOARD_BUFFER_SIZE;
    if (next_head == kbd_buffer.tail) {
        debug_print("KEYBOARD: Buffer overflow, dropping character");
        return;  // バッファフル
    }

    // 先にデータを書き込み、その後headを公開（SPSC）
    kbd_buffer.buffer[kbd_buffer.head] = c;
    kbd_buffer.head = next_head;
}

/*
 * キーボードバッファ読み取り関数
 * 【役割】バッファから文字を取得（コンシューマー）
 */
char keyboard_buffer_get(void) {
    if (kbd_buffer.head == kbd_buffer.tail) {
        return 0;  // バッファが空
    }

    char c = kbd_buffer.buffer[kbd_buffer.tail];
    kbd_buffer.tail = (kbd_buffer.tail + 1) % KEYBOARD_BUFFER_SIZE;
    return c;
}

/*
 * キーボードバッファ空チェック関数
 */
bool keyboard_buffer_is_empty(void) {
    return kbd_buffer.head == kbd_buffer.tail;
}

/*
 * キーボードバッファフルチェック関数
 */
bool keyboard_buffer_is_full(void) {
    int next_head = (kbd_buffer.head + 1) % KEYBOARD_BUFFER_SIZE;
    return next_head == kbd_buffer.tail;
}

/*
 * キーボードコントローラ初期化関数（分割関数）
 * 【役割】PS/2キーボードコントローラの初期化
 */
void init_keyboard_controller(void) {
    debug_print("KEYBOARD: PS/2 controller initialization");
    // PS/2コントローラは通常、初期化時に自動的に利用可能になる
    // 特別な初期化コマンドは不要（シンプルな実装）
}

/*
 * キーボードステータス読み取り関数（分割関数）
 * 【役割】キーボードコントローラのステータスを読み取る
 */
uint8_t read_keyboard_status(void) {
    return inb(KEYBOARD_STATUS_PORT);
}

/*
 * キーボードデータ読み取り関数（分割関数）
 * 【役割】キーボードコントローラからデータを読み取る
 */
uint8_t read_keyboard_data(void) {
    return inb(KEYBOARD_DATA_PORT);
}

/*
 * スキャンコード→ASCII変換関数（分割関数）
 * 【役割】スキャンコードをASCII文字に変換する
 */
char convert_scancode_to_ascii(uint8_t scancode, bool shift_pressed) {
    if (scancode >= sizeof(scancode_to_ascii)) {
        return 0;  // 範囲外
    }

    if (shift_pressed) {
        return scancode_to_ascii_shift[scancode];
    } else {
        return scancode_to_ascii[scancode];
    }
}

/*
 * キーボード初期化関数（統合関数）
 * 【役割】キーボード機能全体を初期化
 */
void init_keyboard(void) {
    init_keyboard_controller();
    init_keyboard_buffer();
    debug_print("KEYBOARD: Complete initialization");
}

/*
 * キーボード割り込みハンドラ（C言語部分）
 * 【役割】キーボード割り込み発生時の処理
 * 【重要】interrupt.sのkeyboard_interrupt_handlerから呼び出される
 */
void keyboard_handler_c(void) {
    // PICに割り込み処理完了を通知
    outb(PIC_MASTER_COMMAND, 0x20);

    // キーボードデータの読み取り可能性をチェック
    uint8_t status = read_keyboard_status();
    if (!(status & KEYBOARD_STATUS_OUTPUT_FULL)) {
        debug_print("KEYBOARD: Interrupt fired but no data available");
        return;
    }

    // スキャンコードを読み取り
    uint8_t scancode = read_keyboard_data();

    // キー離す操作は無視
    if (scancode & SCANCODE_RELEASE_MASK) {
        uint8_t key_code = scancode & 0x7F;

        // Shiftキーの離す処理
        if (key_code == SCANCODE_LEFT_SHIFT ||
            key_code == SCANCODE_RIGHT_SHIFT) {
            shift_pressed = false;
        }
        return;
    }

    // Shiftキーの押下処理
    if (scancode == SCANCODE_LEFT_SHIFT || scancode == SCANCODE_RIGHT_SHIFT) {
        shift_pressed = true;
        return;
    }

    // スキャンコードをASCII文字に変換
    char ascii = convert_scancode_to_ascii(scancode, shift_pressed);

    if (ascii != 0) {
        // 有効なASCII文字をバッファに格納
        keyboard_buffer_put(ascii);

        // キーボード入力待ちでブロックされているスレッドを起床させる
        unblock_keyboard_threads();

        // デバッグ出力
        char debug_msg[32];
        debug_msg[0] = 'K';
        debug_msg[1] = 'E';
        debug_msg[2] = 'Y';
        debug_msg[3] = ':';
        debug_msg[4] = ' ';
        debug_msg[5] = ascii;
        debug_msg[6] = ' ';
        debug_msg[7] = '(';
        itoa((uint32_t)scancode, &debug_msg[8], 10);
        int len = 8;
        while (debug_msg[len] != 0) len++;
        debug_msg[len] = ')';
        debug_msg[len + 1] = 0;
        debug_print(debug_msg);
    }
}

/*
 * 高レベル文字入力関数（ブロッキング）
 * 【役割】1文字が入力されるまで待機し、その文字を返す
 * 【重要】この関数はキー入力があるまでブロック（待機）する
 */
char getchar(void) {
    char c;

    // キーボードバッファから文字が取得できるまで待機
    while ((c = keyboard_buffer_get()) == 0) {
        // 汎用ブロック関数を呼び出し、キーボード入力を待つ
        // このスレッドは BLOCK_REASON_KEYBOARD でブロックされる
        block_current_thread(BLOCK_REASON_KEYBOARD, 0);
        schedule();
    }
    return c;
}

/*
 * scanf用文字入力関数
 * 【役割】scanf("%c", &ch)と同等の機能
 * 【備考】getchar()のエイリアス関数
 */
char scanf_char(void) {
    return getchar();
}

/*
 * 文字列入力関数（安全な行入力）
 * 【役割】改行まで文字列を読み取る（最大max_length-1文字）
 * 【重要】バックスペース対応、エコー表示付き
 */
void read_line(char* buffer, int max_length) {
    // Enhanced input validation
    if (!buffer || max_length <= 1) {
        debug_print("read_line: Invalid parameters");
        return;  // 無効なパラメータ
    }

    // Additional safety: reasonable upper limit check
    if (max_length > 1024) {
        debug_print("read_line: Buffer size too large, limiting to 1024");
        max_length = 1024;  // Prevent excessive buffer sizes
    }

    int pos = 0;
    char c;

    // Initialize buffer for safety
    buffer[0] = 0;

    while (pos < max_length - 1) {
        c = getchar();

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

/*
 * scanf用文字列入力関数
 * 【役割】scanf("%s", buffer)と同等の機能
 * 【備考】read_line() のエイリアス関数
 */
void scanf_string(char* buffer, int max_length) {
    read_line(buffer, max_length);
}
