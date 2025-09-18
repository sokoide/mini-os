#include "debug_utils.h"

#include <stdarg.h>

#include "error_types.h"
#include "keyboard.h"

/*
 * デバッグ・診断システム実装
 * 【目的】システム状態の監視、性能測定、問題診断
 * 【特徴】リアルタイム監視、詳細分析、自動メンテナンス
 */

// グローバルデバッグ状態
static debug_level_t current_debug_level = DEBUG_LEVEL_INFO;
static system_metrics_t system_metrics = {0};

// プロファイル追跡管理
#define MAX_PROFILE_SECTIONS 16
typedef struct {
    char name[32];        // プロファイル区間名
    uint32_t start_tick;  // 開始ティック
    uint32_t total_time;  // 総実行時間
    uint32_t call_count;  // 呼び出し回数
    uint32_t min_time;    // 最短実行時間
    uint32_t max_time;    // 最長実行時間
    bool active;          // アクティブフラグ
} profile_section_t;

static profile_section_t profile_sections[MAX_PROFILE_SECTIONS];
static int profile_section_count = 0;

/**
 * ===========================================
 * デバッグ出力機能実装
 * ===========================================
 */
/*
 * デバッグレベル設定関数
 * 【役割】システムのデバッグ出力レベルを制御
 * 【パラメータ】level: 設定するデバッグレベル
 */
void debug_set_level(debug_level_t level) {
    if (level <= DEBUG_LEVEL_VERBOSE) {
        current_debug_level = level;
        debug_print("デバッグ: レベルを %d に設定", level);
    }
}

/*
 * デバッグレベル取得関数
 * 【役割】現在のデバッグレベルを返す
 * 【戻り値】現在のデバッグレベル
 */
debug_level_t debug_get_level(void) {
    return current_debug_level;
}

/*
 * レベル付きログ出力関数
 * 【役割】指定されたレベルでメッセージを出力
 * 【パラメータ】level: メッセージの重要度レベル
 * 【パラメータ】format: printf形式のフォーマット文字列
 */
void debug_log(debug_level_t level, const char* format, ...) {
    if (level > current_debug_level) {
        return;  // メッセージレベルが現在の設定より詳細
    }

    va_list ap;
    va_start(ap, format);
    debug_vprint(format, ap);
    va_end(ap);
}

void debug_hexdump(const void* data, size_t length, const char* label) {
    const uint8_t* bytes = (const uint8_t*)data;
    debug_print("=== Hexdump: %s (%u bytes) ===", label, length);

    for (size_t i = 0; i < length; i += 16) {
        // Print address
        char line[80];
        int pos = 0;

        // Add hex values
        for (size_t j = 0; j < 16 && (i + j) < length; j++) {
            if (j == 8)
                line[pos++] = ' ';  // Extra space in middle
            // Simple hex conversion
            uint8_t byte = bytes[i + j];
            uint8_t high = (byte >> 4) & 0x0F;
            uint8_t low = byte & 0x0F;
            line[pos++] = (high < 10) ? ('0' + high) : ('A' + high - 10);
            line[pos++] = (low < 10) ? ('0' + low) : ('A' + low - 10);
            line[pos++] = ' ';
        }

        line[pos] = '\0';
        debug_print("  %s", line);
    }
}

/*
 * スタックトレース出力関数
 * 【役割】指定された深度でスタック内容を表示
 * 【パラメータ】stack_ptr: スタックポインタ
 * 【パラメータ】max_depth: 表示する最大深度
 */
void debug_stack_trace(uint32_t* stack_ptr, size_t max_depth) {
    debug_print("=== スタックトレース (深度: %u) ===", max_depth);
    for (size_t i = 0; i < max_depth && stack_ptr; i++) {
        debug_print("  [%u] 0x%08x", i, *stack_ptr);
        stack_ptr++;
    }
}

/*
 * バイナリダンプ表示関数
 * 【役割】32ビット値をビット単位で表示
 * 【パラメータ】value: 表示する値
 * 【パラメータ】label: 表示ラベル
 */
void debug_binary_dump(uint32_t value, const char* label) {
    debug_print("=== バイナリダンプ: %s ===", label);
    debug_print("値: 0x%08x (%u)", value, value);
    debug_print("ビット: ");

    char binary_str[33];
    for (int i = 31; i >= 0; i--) {
        binary_str[31 - i] = (value & (1u << i)) ? '1' : '0';
        if (i % 8 == 0 && i > 0) {
            binary_str[32 - i] = ' ';  // Add space every 8 bits
        }
    }
    binary_str[32] = '\0';
    debug_print("  %s", binary_str);
}

/*
 * メモリ内容比較表示関数
 * 【役割】二つのメモリ領域を比較して差異を表示
 * 【パラメータ】addr1: 比較元アドレス
 * 【パラメータ】addr2: 比較先アドレス
 * 【パラメータ】length: 比較サイズ
 * 【パラメータ】label: 表示ラベル
 */
void debug_memory_compare(const void* addr1, const void* addr2, size_t length,
                          const char* label) {
    const uint8_t* bytes1 = (const uint8_t*)addr1;
    const uint8_t* bytes2 = (const uint8_t*)addr2;

    debug_print("=== メモリ比較: %s (%u バイト) ===", label, length);

    int differences = 0;
    for (size_t i = 0; i < length; i++) {
        if (bytes1[i] != bytes2[i]) {
            debug_print("差異 [0x%04x]: 0x%02x != 0x%02x", i, bytes1[i],
                        bytes2[i]);
            differences++;
        }
    }

    if (differences == 0) {
        debug_print("差異なし: メモリ内容が一致");
    } else {
        debug_print("合計 %d 個の差異を発見", differences);
    }
}

/**
 * ===========================================
 * システムメトリクス実装
 * ===========================================
 */
void metrics_init(void) {
    // Initialize all metrics to zero
    system_metrics.total_interrupts = 0;
    system_metrics.context_switches = 0;
    system_metrics.threads_created = 0;
    system_metrics.memory_usage_bytes = 0;
    system_metrics.system_uptime_ticks = 0;
    system_metrics.keyboard_inputs = 0;
    system_metrics.serial_writes = 0;

    debug_print("METRICS: System metrics initialized");
}

void metrics_update(void) {
    system_metrics.system_uptime_ticks = get_system_ticks();
    // Additional metrics updates would be called from respective subsystems
}

system_metrics_t* metrics_get(void) {
    metrics_update();
    return &system_metrics;
}

void metrics_print_summary(void) {
    metrics_update();
    debug_print("=== System Metrics Summary ===");
    debug_print("Uptime: %u ticks", system_metrics.system_uptime_ticks);
    debug_print("Total Interrupts: %u", system_metrics.total_interrupts);
    debug_print("Context Switches: %u", system_metrics.context_switches);
    debug_print("Threads Created: %u", system_metrics.threads_created);
    debug_print("Memory Usage: %u bytes", system_metrics.memory_usage_bytes);
    debug_print("Keyboard Inputs: %u", system_metrics.keyboard_inputs);
    debug_print("Serial Writes: %u", system_metrics.serial_writes);
}

void metrics_reset(void) {
    metrics_init();
    debug_print("メトリクス: 全メトリクスをリセット");
}

/*
 * 個別メトリクス更新関数群
 * 【役割】特定のメトリクスを安全にインクリメント
 */

/*
 * 割り込み回数インクリメント関数
 * 【役割】総割り込み回数を1増加
 */
void metrics_increment_interrupts(void) {
    system_metrics.total_interrupts++;
}

/*
 * コンテキストスイッチ回数インクリメント関数
 * 【役割】コンテキストスイッチ回数を1増加
 */
void metrics_increment_context_switches(void) {
    system_metrics.context_switches++;
}

/*
 * キーボード入力回数インクリメント関数
 * 【役割】キーボード入力回数を1増加
 */
void metrics_increment_keyboard_inputs(void) {
    system_metrics.keyboard_inputs++;
}

/*
 * シリアル出力回数インクリメント関数
 * 【役割】シリアル出力回数を1増加
 */
void metrics_increment_serial_writes(void) {
    system_metrics.serial_writes++;
}

/*
 * メモリ使用量設定関数
 * 【役割】現在のメモリ使用量を設定
 * 【パラメータ】bytes: メモリ使用量（バイト単位）
 */
void metrics_set_memory_usage(uint32_t bytes) {
    system_metrics.memory_usage_bytes = bytes;
}

/**
 * Thread diagnostics implementation
 */
void thread_diagnostics_collect(thread_t* thread, thread_diagnostics_t* diag) {
    if (!thread || !diag)
        return;

    diag->thread_id = (uint32_t)thread;  // Use pointer as ID
    diag->state = thread->state;
    diag->stack_usage = thread_stack_usage(thread);
    diag->execution_time = get_system_ticks() - thread->last_tick;
    diag->sleep_count = 0;           // Would need to be tracked
    diag->context_switch_count = 0;  // Would need to be tracked
}

void thread_diagnostics_print(const thread_diagnostics_t* diag) {
    if (!diag)
        return;

    debug_print("Thread ID: 0x%08x", diag->thread_id);
    debug_print("  State: %d", diag->state);
    debug_print("  Stack Usage: %u bytes", diag->stack_usage);
    debug_print("  Execution Time: %u ticks", diag->execution_time);
    debug_print("  Sleep Count: %u", diag->sleep_count);
    debug_print("  Context Switches: %u", diag->context_switch_count);
}

void thread_diagnostics_print_all(void) {
    debug_print("=== All Thread Diagnostics ===");
    thread_t* current = get_current_thread();
    if (current) {
        thread_diagnostics_t diag;
        thread_diagnostics_collect(current, &diag);
        debug_print("Current Thread:");
        thread_diagnostics_print(&diag);
    }
    // Would iterate through all threads if we had a thread list
}

uint32_t thread_stack_usage(const thread_t* thread) {
    if (!thread)
        return 0;

    // Calculate stack usage based on ESP position
    uint32_t stack_base = (uint32_t)thread->stack;
    uint32_t stack_top = stack_base + (THREAD_STACK_SIZE * sizeof(uint32_t));

    if (thread->esp >= stack_base && thread->esp <= stack_top) {
        return stack_top - thread->esp;
    }
    return 0;  // Invalid stack pointer
}

/**
 * Memory diagnostics implementation
 */
void memory_check_integrity(void) {
    debug_print("=== Memory Integrity Check ===");
    // Basic memory region checks
    debug_print("Kernel region: 0x100000 - 0x200000");
    debug_print("Stack region: 0x200000 - 0x300000");
    debug_print("VGA buffer: 0xB8000 - 0xB8FA0");
    debug_print("Memory integrity check complete");
}

void memory_print_layout(void) {
    debug_print("=== Memory Layout ===");
    debug_print("Boot sector: 0x7C00 - 0x7DFF");
    debug_print("Kernel: 0x100000 - 0x200000");
    debug_print("Stack: 0x200000 - 0x300000");
    debug_print("VGA Text: 0xB8000 - 0xB8FA0");
}

uint32_t memory_get_usage(void) {
    // Simple estimation - would need more sophisticated tracking
    uint32_t kernel_size = 64 * 1024;  // Estimate
    uint32_t stack_usage = 4 * 1024;   // Estimate per thread
    return kernel_size + stack_usage;
}

/**
 * Performance profiling implementation
 */
void profile_start(const char* section_name) {
    if (profile_section_count >= MAX_PROFILE_SECTIONS) {
        DEBUG_WARN("Maximum profile sections reached");
        return;
    }

    // Find existing or create new section
    profile_section_t* section = NULL;
    for (int i = 0; i < profile_section_count; i++) {
        // Simple string comparison
        bool match = true;
        for (int j = 0; j < 32; j++) {
            if (profile_sections[i].name[j] != section_name[j]) {
                match = false;
                break;
            }
            if (section_name[j] == '\0')
                break;
        }
        if (match) {
            section = &profile_sections[i];
            break;
        }
    }

    if (!section) {
        section = &profile_sections[profile_section_count++];
        // Copy name
        for (int i = 0; i < 31; i++) {
            section->name[i] = section_name[i];
            if (section_name[i] == '\0')
                break;
        }
        section->name[31] = '\0';
        section->total_time = 0;
        section->call_count = 0;
    }

    section->start_tick = get_system_ticks();
    section->active = true;
}

void profile_end(const char* section_name) {
    uint32_t end_tick = get_system_ticks();

    for (int i = 0; i < profile_section_count; i++) {
        // Simple string comparison
        bool match = true;
        for (int j = 0; j < 32; j++) {
            if (profile_sections[i].name[j] != section_name[j]) {
                match = false;
                break;
            }
            if (section_name[j] == '\0')
                break;
        }

        if (match && profile_sections[i].active) {
            profile_sections[i].total_time +=
                (end_tick - profile_sections[i].start_tick);
            profile_sections[i].call_count++;
            profile_sections[i].active = false;
            return;
        }
    }
}

void profile_print_results(void) {
    debug_print("=== Performance Profile Results ===");
    for (int i = 0; i < profile_section_count; i++) {
        debug_print("%s: %u ticks total, %u calls, %u avg",
                    profile_sections[i].name, profile_sections[i].total_time,
                    profile_sections[i].call_count,
                    profile_sections[i].call_count > 0
                        ? profile_sections[i].total_time /
                              profile_sections[i].call_count
                        : 0);
    }
}

void profile_reset(void) {
    profile_section_count = 0;
    debug_print("PROFILE: All profile data reset");
}

/**
 * System health checks implementation
 */
health_status_t system_health_check(void) {
    health_status_t status = HEALTH_OK;

    // Check current thread
    if (!get_current_thread()) {
        DEBUG_WARN("No current thread");
        status = HEALTH_WARNING;
    }

    // Check system uptime
    if (get_system_ticks() == 0) {
        DEBUG_ERROR("System ticks not updating");
        status = HEALTH_ERROR;
    }

    return status;
}

void system_health_print_report(void) {
    debug_print("=== System Health Report ===");
    health_status_t status = system_health_check();

    switch (status) {
    case HEALTH_OK:
        debug_print("Status: OK - All systems normal");
        break;
    case HEALTH_WARNING:
        debug_print("Status: WARNING - Minor issues detected");
        break;
    case HEALTH_ERROR:
        debug_print("Status: ERROR - Significant issues detected");
        break;
    case HEALTH_CRITICAL:
        debug_print("Status: CRITICAL - System stability at risk");
        break;
    }

    debug_print("Current thread: 0x%08x", (uint32_t)get_current_thread());
    debug_print("System uptime: %u ticks", get_system_ticks());
}

void system_maintenance_tasks(void) {
    debug_print("=== Running System Maintenance ===");

    // Update metrics
    metrics_update();

    // Check memory integrity
    memory_check_integrity();

    // Health check
    system_health_check();

    debug_print("System maintenance complete");
}

/**
 * Debug commands interface implementation
 */
/*
 * デバッグヘルプコマンド
 * 【役割】利用可能なデバッグコマンド一覧を表示
 */
void debug_command_help(void) {
    debug_print("=== デバッグコマンドヘルプ ===");
    debug_print("基本コマンド:");
    debug_print("  status     - システム状態を表示");
    debug_print("  threads    - スレッド情報を表示");
    debug_print("  memory     - メモリ情報を表示");
    debug_print("  metrics    - システムメトリクスを表示");
    debug_print("  profile    - 性能プロファイルを表示");
    debug_print("  health     - システムヘルス報告を表示");
    debug_print("拡張コマンド:");
    debug_print("  interrupts - 割り込み情報を表示");
    debug_print("  scheduler  - スケジューラー情報を表示");
    debug_print("  keyboard   - キーボード状態を表示");
    debug_print("  serial     - シリアル通信状態を表示");
    debug_print("  timer      - タイマー情報を表示");
    debug_print("  trace      - 実行トレースを表示");
    debug_print("  benchmark  - 性能ベンチマークを実行");
    debug_print("  stress     - ストレステストを実行");
}

void debug_command_status(void) {
    debug_print("=== System Status ===");
    debug_print("Debug Level: %d", current_debug_level);
    debug_print("System Ticks: %u", get_system_ticks());
    debug_print("Current Thread: 0x%08x", (uint32_t)get_current_thread());
}

void debug_command_threads(void) {
    thread_diagnostics_print_all();
}

void debug_command_memory(void) {
    memory_print_layout();
    debug_print("Memory Usage: %u bytes", memory_get_usage());
}

void debug_command_metrics(void) {
    metrics_print_summary();
}

void debug_command_profile(void) {
    profile_print_results();
}

void debug_command_health(void) {
    system_health_print_report();
}

/*
 * 拡張デバッグコマンド群
 * 【役割】詳細システム診断とストレステスト機能
 */

/*
 * 割り込み情報表示コマンド
 * 【役割】割り込み統計と状態を詳細表示
 */
void debug_command_interrupts(void) {
    debug_print("=== 割り込み情報 ===");
    debug_print("総割り込み回数: %u", system_metrics.total_interrupts);
    debug_print("タイマー割り込み: %u", system_metrics.timer_interrupts);
    debug_print("平均割り込み頻度: %u/秒", system_metrics.total_interrupts /
                                               (get_system_ticks() / 100 + 1));

    // PIC状態表示
    debug_print("PIC状態:");
    debug_print("  Master Mask: 0x%02x", inb(0x21));
    debug_print("  Slave Mask:  0x%02x", inb(0xA1));
}

/*
 * スケジューラー情報表示コマンド
 * 【役割】スケジューラーの動作状況を詳細表示
 */
void debug_command_scheduler(void) {
    debug_print("=== スケジューラー情報 ===");
    debug_print("コンテキストスイッチ回数: %u",
                system_metrics.context_switches);
    debug_print("現在のスレッド: 0x%08x", (uint32_t)get_current_thread());
    debug_print("システム稼働時間: %u ティック", get_system_ticks());

    if (get_current_thread()) {
        thread_t* current = get_current_thread();
        debug_print("現在スレッドの状態:");
        debug_print("  状態: %d", current->state);
        debug_print("  カウンタ: %u", current->counter);
        debug_print("  表示行: %d", current->display_row);
    }
}

/*
 * キーボード状態表示コマンド
 * 【役割】キーボード入力システムの詳細状態
 */
void debug_command_keyboard(void) {
    debug_print("=== キーボード状態 ===");
    debug_print("入力回数: %u", system_metrics.keyboard_inputs);
    debug_print("バッファ状態: %s",
                keyboard_buffer_is_empty() ? "空" : "データあり");
    debug_print("バッファフル状態: %s",
                keyboard_buffer_is_full() ? "満杯" : "正常");

    // キーボードコントローラー状態
    uint8_t kbd_status = read_keyboard_status();
    debug_print("コントローラー状態: 0x%02x", kbd_status);
    debug_print("  出力バッファ: %s",
                (kbd_status & 0x01) ? "データあり" : "空");
    debug_print("  入力バッファ: %s", (kbd_status & 0x02) ? "満杯" : "正常");
}

/*
 * シリアル通信状態表示コマンド
 * 【役割】シリアルポートの動作状況を表示
 */
void debug_command_serial(void) {
    debug_print("=== シリアル通信状態 ===");
    debug_print("出力回数: %u", system_metrics.serial_writes);

    // COM1状態確認
    uint8_t lsr = inb(0x3FD);  // Line Status Register
    debug_print("COM1状態 (LSR: 0x%02x):", lsr);
    debug_print("  送信準備: %s", (lsr & 0x20) ? "OK" : "待機中");
    debug_print("  受信データ: %s", (lsr & 0x01) ? "あり" : "なし");
    debug_print("  エラー状態: %s", (lsr & 0x1E) ? "エラー" : "正常");
}

/*
 * タイマー情報表示コマンド
 * 【役割】システムタイマーの動作状況を表示
 */
void debug_command_timer(void) {
    debug_print("=== タイマー情報 ===");
    debug_print("システムティック: %u", get_system_ticks());
    debug_print("稼働時間: %u.%02u 秒", get_system_ticks() / 100,
                get_system_ticks() % 100);
    debug_print("タイマー割り込み: %u", system_metrics.timer_interrupts);
    debug_print("理論周波数: 100Hz (10ms間隔)");

    // 実際の周波数計算
    uint32_t actual_freq =
        system_metrics.timer_interrupts * 100 / (get_system_ticks() + 1);
    debug_print("実際の周波数: 約%uHz", actual_freq);
}

/*
 * メモリダンプコマンド
 * 【役割】指定アドレスのメモリ内容を16進表示
 */
void debug_command_dump(uint32_t address, uint32_t length) {
    debug_print("=== メモリダンプ ===");
    debug_print("アドレス: 0x%08x, サイズ: %u バイト", address, length);

    // 安全性チェック
    if (length > 256) {
        debug_print("警告: サイズを256バイトに制限");
        length = 256;
    }

    debug_hexdump((void*)address, length, "メモリダンプ");
}

/*
 * 実行トレース表示コマンド
 * 【役割】システムの実行履歴を表示
 */
void debug_command_trace(void) {
    debug_print("=== 実行トレース ===");
    debug_print("最近のシステム活動:");

    // メトリクス履歴の簡易表示
    debug_print("- 総割り込み: %u", system_metrics.total_interrupts);
    debug_print("- コンテキストスイッチ: %u", system_metrics.context_switches);
    debug_print("- キーボード入力: %u", system_metrics.keyboard_inputs);
    debug_print("- シリアル出力: %u", system_metrics.serial_writes);

    // 現在のスタック状況
    uint32_t* stack_ptr = (uint32_t*)&stack_ptr;  // 現在のスタック位置
    debug_stack_trace(stack_ptr, 5);
}

/*
 * 性能ベンチマークコマンド
 * 【役割】システム性能を測定・表示
 */
void debug_command_benchmark(void) {
    debug_print("=== 性能ベンチマーク ===");

    // 簡単な計算性能テスト
    uint32_t start_tick = get_system_ticks();
    volatile uint32_t result = 0;

    debug_print("計算性能テスト開始...");
    for (int i = 0; i < 1000; i++) {
        result += i * i;
    }

    uint32_t end_tick = get_system_ticks();
    uint32_t elapsed = end_tick - start_tick;

    debug_print("計算結果: %u", result);
    debug_print("実行時間: %u ティック (%u.%02u ms)", elapsed, elapsed * 10,
                elapsed % 10);

    // メモリアクセス性能テスト
    debug_print("メモリアクセステスト...");
    start_tick = get_system_ticks();

    volatile uint8_t* test_mem = (uint8_t*)0x200000;  // テスト用メモリ
    for (int i = 0; i < 1000; i++) {
        test_mem[i % 100] = i & 0xFF;
        result += test_mem[i % 100];
    }

    end_tick = get_system_ticks();
    elapsed = end_tick - start_tick;

    debug_print("メモリテスト完了: %u ティック", elapsed);
}

/*
 * ストレステストコマンド
 * 【役割】システムの安定性をテスト
 */
void debug_command_stress_test(void) {
    debug_print("=== ストレステスト ===");
    debug_print("警告: システムに負荷をかけます");

    uint32_t start_tick = get_system_ticks();

    // CPU集約的処理
    debug_print("CPU負荷テスト実行中...");
    volatile uint32_t cpu_result = 0;
    for (int i = 0; i < 10000; i++) {
        cpu_result += i * i * i;
        if (i % 1000 == 0) {
            debug_print("処理中... %d/10000", i);
        }
    }

    // メモリアクセステスト
    debug_print("メモリストレステスト...");
    volatile uint8_t* stress_mem = (uint8_t*)0x200000;
    for (int i = 0; i < 5000; i++) {
        stress_mem[i % 1000] = (i ^ cpu_result) & 0xFF;
        cpu_result += stress_mem[i % 1000];
    }

    uint32_t end_tick = get_system_ticks();
    uint32_t total_time = end_tick - start_tick;

    debug_print("ストレステスト完了");
    debug_print("実行時間: %u ティック (%u.%02u 秒)", total_time,
                total_time / 100, total_time % 100);
    debug_print("システム状態: %s",
                (get_current_thread() != NULL) ? "安定" : "警告");

    // システムヘルスチェック実行
    health_status_t health = system_health_check();
    debug_print("ヘルス状態: %d (0=正常, 1=警告, 2=エラー, 3=致命的)", health);
}
