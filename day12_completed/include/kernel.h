#ifndef KERNEL_H
#define KERNEL_H

// 基本型定義（nostdincのため手動定義）
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef int int32_t;
typedef unsigned long size_t;
#define false 0
#define true 1
#define bool int

#include "error_types.h"

// VGAテキストモード定数
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000
#define VGA_WHITE_ON_BLACK 0x0720

// タイマー関連定数
#define PIT_FREQUENCY 1193180
#define TIMER_FREQUENCY 100

// スレッド管理定数
#define MAX_THREADS 5
#define THREAD_STACK_SIZE 1024
#define MAX_COUNTER_VALUE 65535

// シリアルポート定数
#define SERIAL_PORT_COM1 0x3F8      // COM1ポートベースアドレス
#define SERIAL_TRANSMIT_READY 0x20  // 送信準備完了ビット

// Serial port configuration constants
#define SERIAL_INT_DISABLE 0x00      // 割り込み無効化
#define SERIAL_DLAB_ENABLE 0x80      // DLAB有効化（ボーレート設定モード）
#define SERIAL_BAUD_38400_LOW 0x03   // ボーレート下位: 38400 bps
#define SERIAL_BAUD_38400_HIGH 0x00  // ボーレート上位: 38400 bps
#define SERIAL_8N1_CONFIG 0x03       // 8bit, パリティなし, 1ストップビット
#define SERIAL_FIFO_ENABLE 0xC7      // FIFO有効化、クリア、14バイト閾値
#define SERIAL_MODEM_READY 0x0B      // IRQ有効化、RTS/DSR設定

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

// ビットマスク定数
#define MASK_LOW_BYTE 0xFF    // 下位8ビットマスク
#define MASK_LOW_WORD 0xFFFF  // 下位16ビットマスク
#define SHIFT_HIGH_BYTE 8     // 上位バイトシフト値
#define SHIFT_HIGH_WORD 16    // 上位ワードシフト値

// EFLAGS定数
#define EFLAGS_INTERRUPT_ENABLE 0x202  // IF=1（割り込み有効）, reserved bit=1

// VGA表示色定数
#define VGA_COLOR_WHITE 0x0F    // 白文字
#define VGA_COLOR_YELLOW 0x0E   // 黄色文字
#define VGA_COLOR_GRAY 0x07     // グレー文字
#define VGA_COLOR_RED 0x0C      // 赤文字
#define VGA_COLOR_GREEN 0x0A    // 緑文字
#define VGA_COLOR_CYAN 0x0B     // シアン文字
#define VGA_COLOR_MAGENTA 0x0D  // マゼンタ文字

// Thread management constants
#define DISPLAY_LINE_LENGTH 25  // 表示行の長さ

// VGA色定数
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
    VGA_LIGHT_BROWN = 14,
    VGA_WHITE = 15,
} vga_color_t;

// タイマー関連定数
#define PIT_FREQUENCY 1193180  // PITの基本周波数
#define TIMER_FREQUENCY 100    // 目標周波数（100Hz = 10ms間隔）

// I/Oポート操作（既存のio.hから移行）
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t result;
    __asm__ volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

// スレッド状態列挙（既存のkernel.cから移行）
typedef enum {
    THREAD_READY,    // 実行可能状態（スケジューリング待ち）
    THREAD_RUNNING,  // 実行中状態（CPUを使用中）
    THREAD_BLOCKED,  // ブロック状態（待機中）
} thread_state_t;

// スレッドブロック理由列挙（既存のkernel.cから移行）
typedef enum {
    BLOCK_REASON_NONE,      // ブロックしていない
    BLOCK_REASON_TIMER,     // タイマー待機（sleep関数等）
    BLOCK_REASON_KEYBOARD,  // キーボード入力待機
} block_reason_t;

// スレッド制御ブロック構造体（既存のkernel.cから移行）
typedef struct thread {
    uint32_t stack[THREAD_STACK_SIZE];  // スレッド専用スタック領域
    uint32_t esp;  // スタックポインタ（コンテキスト切り替え用）

    thread_state_t state;         // スレッドの現在状態
    block_reason_t block_reason;  // ブロック中の場合の理由

    uint32_t counter;       // スレッド内部カウンター
    uint32_t delay_ticks;   // スリープ間隔（タイマーティック数）
    uint32_t last_tick;     // 最後に実行されたタイマーティック
    uint32_t wake_up_tick;  // 起動予定タイマーティック

    int display_row;  // VGA表示用行番号

    struct thread* next_ready;    // 実行可能リストの次のスレッド
    struct thread* next_blocked;  // ブロックリストの次のスレッド
} thread_t;

// カーネルコンテキスト構造体（既存のkernel.cから移行）
typedef struct {
    thread_t* current_thread;           // 現在実行中のスレッド
    thread_t* ready_thread_list;        // 実行可能スレッドの循環リスト
    thread_t* blocked_thread_list;      // ブロック中スレッドのリスト
    volatile uint32_t system_ticks;     // システムタイマーティック数
    volatile int scheduler_lock_count;  // スケジューラーロックカウンタ
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

// コア関数プロトタイプ（既存のkernel.cから移行）
kernel_context_t* get_kernel_context(void);
thread_t* get_current_thread(void);
uint32_t get_system_ticks(void);

// 関数プロトタイプ
void init_serial(void);
void serial_write_char(char c);
void serial_write_string(const char* str);
void serial_puthex(uint32_t v);

// スレッド管理関数（内部関数として公開）
os_result_t add_thread_to_ready_list(thread_t* thread);
void vga_init(void);
void vga_putc(char c);
void vga_puts(const char* s);
void vga_putnum(uint32_t n);
void vga_set_color(vga_color_t foreground, vga_color_t background);
void vga_move_cursor(uint16_t x, uint16_t y);
void vga_clear(void);

// スレッド管理関数プロトタイプ（既存のkernel.cから移行）
os_result_t create_thread(void (*func)(void), uint32_t delay_ticks,
                          int display_row, thread_t** out_thread);
os_result_t add_thread_to_ready_list(thread_t* thread);
void schedule(void);
void sleep(uint32_t ticks);
void block_current_thread(block_reason_t reason, uint32_t data);

// キーボード関数プロトタイプ（既存のkernel.cから移行）
void keyboard_handler_c(void);
char getchar_blocking(void);

// General Utilities
void itoa(uint32_t value, char* buffer, int base);

// VGA Display & Debugging
void clear_screen(void);
void clear_line(int row);
void print_at(int row, int col, const char* str, uint8_t color);
void display_system_info(void);

// Interrupt & Timer System
void set_idt_gate(int n, uint32_t handler);
void setup_idt_structure(void);
void register_interrupt_handlers(void);
void remap_pic(void);
void configure_interrupt_masks(void);
void enable_timer_interrupt(void);
void init_pic(void);
void init_timer(uint32_t frequency);
void init_interrupts(void);
void enable_cpu_interrupts(void);

// Thread Management & Scheduling
os_result_t validate_thread_params(void (*func)(void), int display_row,
                                   uint32_t* delay_ticks);
void initialize_thread_stack(thread_t* thread, void (*func)(void));
void configure_thread_attributes(thread_t* thread, uint32_t delay_ticks,
                                 int display_row);
int update_thread_counter(uint32_t* last_tick_ptr, uint32_t interval_ticks,
                          const char* thread_name, int display_row);

// High-Level Application Logic
void kernel_thread_function(void);
void thread_function_1(void);
void thread_function_2(void);
void thread_function_3(void);
void kernel_main(void);

// Interrupt Handlers (C-level)
void timer_handler_c(void);

// External Assembly Functions
extern void timer_interrupt_handler(void);
extern void keyboard_interrupt_handler(void);
extern void switch_context(uint32_t* old_esp, uint32_t new_esp);
extern void initial_context_switch(uint32_t new_esp);

#endif  // KERNEL_H