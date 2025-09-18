#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "error_types.h"

// PS/2キーボード定数
#define PS2_DATA 0x60
#define PS2_STATUS 0x64
#define PS2_OUTPUT_FULL 0x01

// キーボードバッファサイズ
#define KEYBOARD_BUFFER_SIZE 256

// キーボード関数プロトタイプ
void keyboard_init(void);
void keyboard_handler_c(void);
char getchar_blocking(void);
int keyboard_buffer_empty(void);
void read_line(char* buffer, int max_length);

// キーボードテスト・デバッグ関数
int ps2_output_full(void);
void unblock_keyboard_threads(void);

#endif  // KEYBOARD_H