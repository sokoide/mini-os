// ============================================================================
// Debug Utilities Implementation
// Provides debugging, logging, and system monitoring functionality
// ============================================================================

#include "debug_utils.h"

#include "kernel.h"

// ============================================================================
// グローバル変数とマクロ定義
// ============================================================================

static debug_level_t current_debug_level = DEBUG_LEVEL_INFO;
static system_metrics_t system_metrics = {0};

#define va_copy(dest, src) __builtin_va_copy(dest, src)

// ============================================================================
// デバッグ初期化関数群
// ============================================================================

// デバッグ初期化
void debug_init(void) {
    serial_write_string("デバッグシステム初期化完了\n");
}

// デバッグレベル設定
void debug_set_level(debug_level_t level) {
    current_debug_level = level;
    debug_print("デバッグレベル設定: %d\n", level);
}

// 簡易デバッグ出力
void debug_print(const char* format, ...) {
    char buffer[256];
    va_list ap;

    va_start(ap, format);
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

/*
 * デバッグメッセージ表示関数
 * 【役割】シリアルポートとVGA画面の両方にデバッグメッセージを出力
 */
void simple_vsprintf(char* out_buf, size_t buf_size, const char* format,
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

// ============================================================================
// デバッグ出力関数群
// ============================================================================

// レベル付きデバッグ出力
void debug_print_level(debug_level_t level, const char* format, ...) {
    if (level > current_debug_level) {
        return;
    }

    va_list args;
    va_start(args, format);
    debug_print(format, args);
    va_end(args);
}

// ============================================================================
// システム監視関数群
// ============================================================================

// システムメトリクス更新
void debug_update_metrics(void) {
    // システム状態の収集と更新
    system_metrics.timer_ticks = get_system_ticks();
    // Add other metrics collection as needed
}

// システム状態表示
void debug_print_system_status(void) {
    debug_print("=== システム状態 ===\n");
    debug_print("コンテキストスイッチ: %d\n", system_metrics.context_switches);
    debug_print("割り込み処理: %d\n", system_metrics.interrupts_handled);
    debug_print("キーボードイベント: %d\n", system_metrics.keyboard_events);
    debug_print("アクティブスレッド: %d\n", system_metrics.active_threads);
    debug_print("メモリ使用量: %d\n", system_metrics.memory_usage);
}

// スレッド情報表示
void debug_print_thread_info(void) {
    kernel_context_t* ctx = get_kernel_context();
    thread_t* current = ctx->current_thread;

    debug_print("=== スレッド情報 ===\n");
    debug_print("現在実行中: %x\n", (uint32_t)current);
    debug_print("実行可能リスト: %x\n", (uint32_t)ctx->ready_thread_list);
    debug_print("ブロックリスト: %x\n", (uint32_t)ctx->blocked_thread_list);
    debug_print("システムティック: %d\n", ctx->system_ticks);
}

// ============================================================================
// プロファイリング関数群
// ============================================================================

// プロファイリング機能（現在はスタブ）
void debug_profile_start(const char* section) {
    debug_print("プロファイル開始: %s\n", section);
}

void debug_profile_end(const char* section) {
    debug_print("プロファイル終了: %s\n", section);
}

void debug_print_profile_stats(void) {
    debug_print("プロファイル統計情報（未実装）\n");
}

// ============================================================================
// 高度デバッグ関数群
// ============================================================================

// メモリダンプ機能
void debug_memory_dump(void* ptr, size_t size) {
    debug_print("メモリダンプ: アドレス %x, サイズ %d\n", (uint32_t)ptr, size);

    uint8_t* byte_ptr = (uint8_t*)ptr;
    for (size_t i = 0; i < size; i++) {
        if (i % 16 == 0) {
            if (i > 0)
                serial_write_string("\n");
            serial_puthex((uint32_t)(byte_ptr + i));
            serial_write_string(": ");
        }
        serial_puthex(byte_ptr[i]);
        serial_write_char(' ');
    }
    serial_write_string("\n");
}

// スタックトレース機能（現在はスタブ）
void debug_stack_trace(void) {
    debug_print("スタックトレース（未実装）\n");
}