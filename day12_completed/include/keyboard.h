#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdbool.h>
#include <stdint.h>

// Keyboard PS/2 controller constants
#define KEYBOARD_DATA_PORT 0x60    // PS/2キーボードデータポート
#define KEYBOARD_STATUS_PORT 0x64  // PS/2キーボードステータス/コマンドポート
#define KEYBOARD_BUFFER_SIZE 256   // キーボード入力バッファサイズ

// Keyboard status register bits
#define KEYBOARD_STATUS_OUTPUT_FULL \
    0x01  // 出力バッファフル（データ読み取り可能）
#define KEYBOARD_STATUS_INPUT_FULL 0x02  // 入力バッファフル（コマンド送信不可）

// Special scan codes
#define SCANCODE_RELEASE_MASK 0x80  // キー離す時のマスク
#define SCANCODE_ENTER 0x1C         // Enterキーのスキャンコード
#define SCANCODE_BACKSPACE 0x0E     // Backspaceキーのスキャンコード
#define SCANCODE_LEFT_SHIFT 0x2A    // 左Shiftキーのスキャンコード
#define SCANCODE_RIGHT_SHIFT 0x36   // 右Shiftキーのスキャンコード

/*
 * キーボード入力バッファ構造体
 * 【役割】リングバッファでキーストロークを管理
 */
typedef struct {
    char buffer[KEYBOARD_BUFFER_SIZE];  // 入力データ格納配列
    volatile int head;                  // 書き込み位置（プロデューサー）
    volatile int tail;                  // 読み取り位置（コンシューマー）
    // SPSCロックフリーバッファ: 空判定は head==tail, 満杯判定は
    // (head+1)%N==tail
} keyboard_buffer_t;

// Keyboard buffer management functions
void init_keyboard_buffer(void);
void keyboard_buffer_put(char c);
char keyboard_buffer_get(void);
bool keyboard_buffer_is_empty(void);
bool keyboard_buffer_is_full(void);

// Keyboard controller functions (split function pattern)
void init_keyboard_controller(void);
uint8_t read_keyboard_status(void);
uint8_t read_keyboard_data(void);
char convert_scancode_to_ascii(uint8_t scancode, bool shift_pressed);

// Keyboard initialization and interrupt handling
void init_keyboard(void);
void keyboard_handler_c(void);

// High-level input functions
char getchar(void);
char scanf_char(void);
// Safe line input with max length (formerly gets)
void read_line(char* buffer, int max_length);
void scanf_string(char* buffer, int max_length);

#endif  // KEYBOARD_H
