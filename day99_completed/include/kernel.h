#ifndef KERNEL_H
#define KERNEL_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_types.h"

// VGAテキストモード定数
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000
#define VGA_WHITE_ON_BLACK 0x0720  // 白文字、黒背景、スペース文字

// デバッグマーカー定数（VGA表示用）
#define DEBUG_MARKER_A 0x0741  // 'A' - 32ビット開始マーカー
#define DEBUG_MARKER_B 0x0742  // 'B' - セグメント設定後
#define DEBUG_MARKER_C 0x0743  // 'C' - セグメント設定完了
#define DEBUG_MARKER_D 0x0744  // 'D' - コピー開始前
#define DEBUG_MARKER_E 0x0745  // 'E' - コピー完了
#define DEBUG_MARKER_F 0x0746  // 'F' - スタック設定完了
#define DEBUG_MARKER_G 0x0747  // 'G' - ジャンプ前
#define DEBUG_MARKER_H 0x0748  // 'H' - カーネルエントリ到達
#define DEBUG_MARKER_I 0x0749  // 'I' - セグメント設定完了
#define DEBUG_MARKER_J 0x074a  // 'J' - スタック設定完了
#define DEBUG_MARKER_K 0x074b  // 'K' - BSS クリア開始前
#define DEBUG_MARKER_L 0x074c  // 'L' - BSS クリア完了
#define DEBUG_MARKER_M 0x074d  // 'M' - kernel_main 呼び出し前
#define DEBUG_MARKER_N 0x074e  // 'N' - kernel_main から戻り

// セグメントセレクタ定数
#define CODE_SEGMENT_SELECTOR 0x08  // コードセグメントセレクタ
#define DATA_SEGMENT_SELECTOR 0x10  // データセグメントセレクタ

// Error types are now defined in error_types.h

// タイマー関連定数
#define PIT_FREQUENCY 1193180  // PITの基本周波数（1.193180MHz）
#define TIMER_FREQUENCY 100    // 目標周波数（100Hz = 10ms間隔）

// I/Oポート定数
#define PIT_CHANNEL0 0x40        // PITチャンネル0データポート
#define PIT_COMMAND 0x43         // PITコマンドポート
#define PIC_MASTER_COMMAND 0x20  // マスターPICコマンドポート
#define PIC_MASTER_DATA 0x21     // マスターPICデータポート

// PIC初期化コマンド定数 (ICW1-ICW4)
#define PIC_ICW1_INIT \
    0x11  // ICW1: 初期化コマンド（エッジトリガー、カスケード、ICW4必要）
#define PIC_ICW2_MASTER_BASE \
    0x20  // ICW2: マスターPIC割り込みベース（IRQ0-7 → INT32-39）
#define PIC_ICW3_SLAVE_IRQ2 0x04  // ICW3: スレーブPICはIRQ2に接続
#define PIC_ICW4_8086_MODE 0x01   // ICW4: 8086/8088モード

// PIC割り込みマスク定数
#define PIC_MASK_ALL_DISABLED 0xFF    // 全割り込み無効化
#define PIC_MASK_TIMER_KEYBOARD 0xFC  // 11111100b = IRQ0とIRQ1のみ有効

// PIC終了コマンド定数
#define PIC_EOI 0x20  // End of Interrupt - 割り込み処理終了通知

// PIT制御コマンド定数
#define PIT_MODE_SQUARE_WAVE \
    0x36  // チャンネル0、Lo/Hi byte、モード3（矩形波）、バイナリ

// IDT関連定数
#define IDT_KERNEL_CODE_SEGMENT 0x08  // カーネルコードセグメントセレクタ
#define IDT_FLAG_PRESENT_DPL0_32BIT \
    0x8E  // プレゼント、DPL=0、32bit割り込みゲート

// Thread management constants
#define MAX_THREADS 4            // 最大スレッド数
#define THREAD_STACK_SIZE 1024   // スレッドスタックサイズ
#define MAX_COUNTER_VALUE 65535  // スレッドカウンター最大値
#define DISPLAY_LINE_LENGTH 25   // 表示行の長さ
#define MAX_THREAD_NAME_LEN 15   // スレッド名最大長

// Serial port constants
#define SERIAL_PORT_COM1 0x3F8  // COM1ポートベースアドレス

// Serial port configuration constants
#define SERIAL_INT_DISABLE 0x00      // 割り込み無効化
#define SERIAL_DLAB_ENABLE 0x80      // DLAB有効化（ボーレート設定モード）
#define SERIAL_BAUD_38400_LOW 0x03   // ボーレート下位: 38400 bps
#define SERIAL_BAUD_38400_HIGH 0x00  // ボーレート上位: 38400 bps
#define SERIAL_8N1_CONFIG 0x03       // 8bit, パリティなし, 1ストップビット
#define SERIAL_FIFO_ENABLE 0xC7      // FIFO有効化、クリア、14バイト閾値
#define SERIAL_MODEM_READY 0x0B      // IRQ有効化、RTS/DSR設定
#define SERIAL_TRANSMIT_READY 0x20   // 送信準備完了ビット

// VGA表示色定数
#define VGA_COLOR_WHITE 0x0F    // 白文字
#define VGA_COLOR_YELLOW 0x0E   // 黄色文字
#define VGA_COLOR_GRAY 0x07     // グレー文字
#define VGA_COLOR_RED 0x0C      // 赤文字
#define VGA_COLOR_GREEN 0x0A    // 緑文字
#define VGA_COLOR_CYAN 0x0B     // シアン文字
#define VGA_COLOR_MAGENTA 0x0D  // マゼンタ文字

// ビットマスク定数
#define MASK_LOW_BYTE 0xFF    // 下位8ビットマスク
#define MASK_LOW_WORD 0xFFFF  // 下位16ビットマスク
#define SHIFT_HIGH_BYTE 8     // 上位バイトシフト値
#define SHIFT_HIGH_WORD 16    // 上位ワードシフト値

// EFLAGS定数
#define EFLAGS_INTERRUPT_ENABLE 0x202  // IF=1（割り込み有効）, reserved bit=1

/*
 * スレッド状態の定義
 * 【説明】各スレッドは以下の3つの状態のいずれかを持つ
 */
typedef enum {
    THREAD_READY,    // 実行可能（CPUを待っている状態）
    THREAD_RUNNING,  // 現在実行中
    THREAD_BLOCKED   // I/Oやタイマー待ちでブロック中
} thread_state_t;

/*
 * スレッドがブロックされている理由
 */
typedef enum {
    BLOCK_REASON_NONE,
    BLOCK_REASON_TIMER,     // sleep()によるタイマー待ち
    BLOCK_REASON_KEYBOARD,  // getchar()によるキーボード入力待ち
    // 将来的にディスクI/O、ネットワークI/Oなどを追加可能
} block_reason_t;

/*
 * スレッド制御ブロック（TCB: Thread Control Block）
 * 【重要】各スレッドの全ての情報を保持する構造体
 */
typedef struct thread {
    uint32_t stack
        [THREAD_STACK_SIZE];  // 4KBのスタック領域（最初に配置してオーバーフロー防止）
    thread_state_t state;         // スレッドの現在状態
    uint32_t counter;             // このスレッド専用のカウンター
    uint32_t delay_ticks;         // カウンター更新間隔（ティック数）
    uint32_t last_tick;           // 最後に更新した時刻
    block_reason_t block_reason;  // スレッドがブロックされている理由
    uint32_t wake_up_tick;        // スリープからの起床予定時刻（ティック数）
    int display_row;              // 画面表示行
    struct thread* next_ready;    // READY リスト用（循環リスト）
    struct thread* next_blocked;  // BLOCKED リスト用ポインタ
    uint32_t
        esp;  // スタックポインタ（最後に配置してスタックオーバーフローから保護）
} thread_t;

/*
 * カーネルコンテキスト構造体
 * 【役割】カーネルの主要な状態を単一の構造体に集約
 */
typedef struct {
    thread_t* current_thread;           // 現在実行中のスレッド
    thread_t* ready_thread_list;        // READYスレッドリストの先頭
    thread_t* blocked_thread_list;      // ブロックされたスレッドのリスト
    uint32_t system_ticks;              // システム起動からの経過ティック数
    volatile int scheduler_lock_count;  // スケジューラのリエントラントロック
} kernel_context_t;

/*
 * IDT（Interrupt Descriptor Table）関連構造体
 * 【役割】割り込みが発生した時に、どの関数を呼び出すかを定義
 */
struct idt_entry {
    uint16_t base_low;   // ハンドラ関数アドレスの下位16bit
    uint16_t selector;   // コードセグメントセレクタ
    uint8_t always0;     // 常に0
    uint8_t flags;       // 属性フラグ
    uint16_t base_high;  // ハンドラ関数アドレスの上位16bit
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;  // IDTのサイズ - 1
    uint32_t base;   // IDTのベースアドレス
} __attribute__((packed));

// Function declarations

/*
 * =================================================================================
 * 1. Low-Level I/O & Utilities
 * =================================================================================
 */

// I/O Port Operations
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Serial Port (for debugging)
void init_serial(void);
void serial_write_char(char c);
void serial_write_string(const char* str);

// General Utilities
void itoa(uint32_t value, char* buffer, int base);

/*
 * =================================================================================
 * 2. VGA Display & Debugging
 * =================================================================================
 */

// VGA色定義（Day12互換）
typedef enum {
    VGA_BLACK = 0,
    VGA_BLUE = 1,
    VGA_GREEN = 2,
    VGA_CYAN = 3,
    VGA_RED = 4,
    VGA_MAGENTA = 5,
    VGA_BROWN = 6,
    VGA_LIGHT_GREY = 7,
    VGA_DARK_GREY = 8,
    VGA_LIGHT_BLUE = 9,
    VGA_LIGHT_GREEN = 10,
    VGA_LIGHT_CYAN = 11,
    VGA_LIGHT_RED = 12,
    VGA_LIGHT_MAGENTA = 13,
    VGA_YELLOW = 14,
    VGA_WHITE = 15
} vga_color_t;

// VGAテキストモード関数（Day12互換）
void vga_set_color(vga_color_t f, vga_color_t b);
void vga_move_cursor(uint16_t x, uint16_t y);
void vga_clear(void);
void vga_putc(char c);
void vga_puts(const char* s);
void vga_putnum(uint32_t n);
void vga_init(void);

void clear_screen(void);
void clear_line(int row);
void print_at(int row, int col, const char* str, uint8_t color);
// Varargs-aware debug print helpers
void debug_vprint(const char* format, va_list args);
void debug_print(const char* format, ...);
void display_system_info(void);

/*
 * =================================================================================
 * 3. Interrupt & Timer System
 * =================================================================================
 */

// 3.1 IDT Management
void set_idt_gate(int n, uint32_t handler);
void setup_idt_structure(void);
void register_interrupt_handlers(void);

// 3.2 PIC (Programmable Interrupt Controller)
void remap_pic(void);
void configure_interrupt_masks(void);
void enable_timer_interrupt(void);
void init_pic(void);

// 3.3 PIT (Programmable Interval Timer)
void init_timer(uint32_t frequency);

// 3.4 Main Interrupt System Initialization
void init_interrupts(void);
void enable_cpu_interrupts(void);

/*
 * =================================================================================
 * 4. Thread Management & Scheduling
 * =================================================================================
 */

// 4.1 Thread Creation (Split Functions)
os_result_t validate_thread_params(void (*func)(void), int display_row,
                                   uint32_t* delay_ticks);
void initialize_thread_stack(thread_t* thread, void (*func)(void));
void configure_thread_attributes(thread_t* thread, uint32_t delay_ticks,
                                 int display_row);
os_result_t add_thread_to_ready_list(thread_t* thread);
os_result_t create_thread(void (*func)(void), uint32_t delay_ticks,
                          int display_row, thread_t** out_thread);

// 4.2 Thread State & Sleep Management (Split Functions)
void sleep(uint32_t ticks);

// 4.3 Blocked Thread Management
void block_current_thread(block_reason_t reason, uint32_t data);
void unblock_keyboard_threads(void);

// 4.4 Scheduler & Core Logic
void schedule(void);

// 4.5 Thread Helpers
kernel_context_t* get_kernel_context(void);
thread_t* get_current_thread(void);
uint32_t get_system_ticks(void);
int update_thread_counter(uint32_t* last_tick_ptr, uint32_t interval_ticks,
                          const char* thread_name, int display_row);

/*
 * =================================================================================
 * 5. High-Level Application Logic
 * =================================================================================
 */

// Thread Functions (Application Layer)
void kernel_thread_function(void);
void thread_function_1(void);
void thread_function_2(void);
void thread_function_3(void);

// Main Kernel Entry
void kernel_main(void);

/*
 * =================================================================================
 * 6. External Assembly Functions (defined in interrupt.s)
 * =================================================================================
 */

// Interrupt Handlers
extern void timer_interrupt_handler(void);
extern void keyboard_interrupt_handler(void);

// Context Switching
extern void context_switch(uint32_t* old_esp, uint32_t new_esp);
extern void initial_context_switch(uint32_t new_esp);

#endif  // KERNEL_H
