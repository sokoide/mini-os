// Test-specific kernel functions for PIC testing
// Only includes the PIC-related functions to avoid linking conflicts

#include "test_framework.h"

// Constants from kernel.h
#define PIC_MASTER_COMMAND 0x20
#define PIC_MASTER_DATA 0x21

// Simple debug function for tests
void debug_print(const char* message) {
    test_serial_write_string("[KERNEL_DEBUG] ");
    test_serial_write_string(message);
    test_serial_write_string("\r\n");
}

// Simple print function for tests
void print_at(int row, int col, const char* str, uint8_t color) {
    (void)row;
    (void)col;
    (void)color;
    test_serial_write_string("[KERNEL_PRINT] ");
    test_serial_write_string(str);
    test_serial_write_string("\r\n");
}

/*
 * PIC remapping function
 * 【役割】IRQ0-7を割り込み32-39に再マップして CPU例外との衝突を回避
 */
void remap_pic(void) {
    debug_print("PIC: Starting PIC remapping");

    // PICを再マップ（IRQ0-7を割り込み32-39に移動）
    // 【重要】デフォルトではIRQ0-7は割り込み8-15にマップされ、CPU例外と衝突する

    // マスター PIC 初期化
    outb(PIC_MASTER_COMMAND, 0x11);  // 初期化コマンド (ICW1)
    outb(PIC_MASTER_DATA, 0x20);     // 割り込みベクター0x20(32)から開始 (ICW2)
    outb(PIC_MASTER_DATA, 0x04);     // スレーブPICはIRQ2に接続 (ICW3)
    outb(PIC_MASTER_DATA, 0x01);     // 8086モード (ICW4)

    debug_print("PIC: Master PIC remapped to interrupts 32-39");
}

/*
 * 割り込みマスク設定関数
 * 【役割】全ての割り込みを無効化（マスク）する
 */
void configure_interrupt_masks(void) {
    debug_print("PIC: Configuring interrupt masks");

    // 全割り込みをマスク（無効化）
    outb(PIC_MASTER_DATA, 0xFF);  // 全割り込み無効化

    debug_print("PIC: All interrupts masked");
}

/*
 * タイマー割り込み有効化関数
 * 【役割】IRQ0（タイマー）のみを有効化する
 */
void enable_timer_interrupt(void) {
    debug_print("PIC: Enabling timer interrupt");

    // タイマー割り込み（IRQ0）のみ有効化
    // bit 0 = 0: IRQ0（タイマー）有効
    // bit 1-7 = 1: その他の割り込み無効
    outb(PIC_MASTER_DATA, 0xFE);  // 11111110b = IRQ0のみ有効

    debug_print("PIC: Timer interrupt (IRQ0) enabled");
}

/*
 * PIC（Programmable Interrupt Controller）初期化
 * 【役割】どの割り込みを有効にするかを制御
 */
void init_pic(void) {
    /*
     * PICの役割：
     * - 複数の割り込みソース（タイマー、キーボード等）を管理
     * - CPUには1つずつ順番に割り込みを送信
     * - 優先度制御や割り込みマスクを提供
     */

    debug_print("PIC: Starting PIC initialization");

    // PIC初期化を3つのステップに分割
    remap_pic();                  // 1. PICを再マップ
    configure_interrupt_masks();  // 2. 割り込みマスクを設定
    enable_timer_interrupt();     // 3. タイマー割り込みを有効化

    print_at(21, 0, "PIC configured: Timer interrupt enabled", 0x0A);
}