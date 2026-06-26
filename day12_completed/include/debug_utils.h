#ifndef DEBUG_UTILS_H
#define DEBUG_UTILS_H

#include <stdint.h>

#include "kernel.h"

/**
 * デバッグ・保守ユーティリティ関数群
 * 【目的】システムの診断、性能監視、デバッグ機能の強化
 * 【特徴】リアルタイムデバッグ、メモリ診断、性能プロファイリング
 */

// デバッグレベル定義
typedef enum {
    DEBUG_LEVEL_OFF = 0,     // デバッグ出力無効
    DEBUG_LEVEL_ERROR = 1,   // エラーのみ出力
    DEBUG_LEVEL_WARN = 2,    // 警告以上を出力
    DEBUG_LEVEL_INFO = 3,    // 情報以上を出力
    DEBUG_LEVEL_DEBUG = 4,   // デバッグ以上を出力
    DEBUG_LEVEL_VERBOSE = 5  // 全ての出力を表示
} debug_level_t;

// システムメトリクス構造体
typedef struct {
    uint32_t total_interrupts;     // 総割り込み回数
    uint32_t context_switches;     // コンテキストスイッチ回数
    uint32_t threads_created;      // 作成されたスレッド数
    uint32_t memory_usage_bytes;   // メモリ使用量（バイト）
    uint32_t system_uptime_ticks;  // システム稼働時間（ティック）
    uint32_t keyboard_inputs;      // キーボード入力回数
    uint32_t serial_writes;        // シリアル出力回数
    uint32_t timer_interrupts;     // タイマー割り込み回数
    uint32_t scheduler_calls;      // スケジューラー呼び出し回数
} system_metrics_t;

// スレッド診断情報構造体
typedef struct {
    uint32_t thread_id;             // スレッドID（ポインタ値）
    thread_state_t state;           // スレッド状態
    uint32_t stack_usage;           // スタック使用量
    uint32_t execution_time;        // 実行時間
    uint32_t sleep_count;           // スリープ回数
    uint32_t context_switch_count;  // コンテキストスイッチ回数
    uint32_t priority;              // 優先度（将来拡張用）
    uint32_t cpu_usage_percent;     // CPU使用率（概算）
} thread_diagnostics_t;

// メモリ領域情報構造体
typedef struct {
    uint32_t start_address;   // 開始アドレス
    uint32_t size;            // サイズ（バイト）
    const char* name;         // 領域名
    const char* description;  // 詳細説明
} memory_region_t;

// 性能プロファイル結果構造体
typedef struct {
    char name[32];              // プロファイル区間名
    uint32_t total_time_ticks;  // 総実行時間（ティック）
    uint32_t call_count;        // 呼び出し回数
    uint32_t min_time_ticks;    // 最短実行時間
    uint32_t max_time_ticks;    // 最長実行時間
    uint32_t avg_time_ticks;    // 平均実行時間
} profile_result_t;

// メモリ使用統計構造体
typedef struct {
    uint32_t total_allocated;        // 総割り当て量
    uint32_t peak_usage;             // ピーク使用量
    uint32_t free_memory;            // 空きメモリ量
    uint32_t fragmentation_percent;  // 断片化率
} memory_stats_t;

/**
 * ===========================================
 * デバッグ出力機能
 * ===========================================
 */
// デバッグレベル制御
void debug_set_level(debug_level_t level);
debug_level_t debug_get_level(void);

// ログ出力（レベル付き）
void debug_log(debug_level_t level, const char* format, ...);

// 16進ダンプ出力
void debug_hexdump(const void* data, size_t length, const char* label);

// スタックトレース出力
void debug_stack_trace(uint32_t* stack_ptr, size_t max_depth);

// バイナリデータ表示（ビット単位）
void debug_binary_dump(uint32_t value, const char* label);

// メモリ内容比較表示
void debug_memory_compare(const void* addr1, const void* addr2, size_t length,
                          const char* label);

/**
 * ===========================================
 * システムメトリクス・診断機能
 * ===========================================
 */
// メトリクス初期化・更新・取得
void metrics_init(void);
void metrics_update(void);
system_metrics_t* metrics_get(void);
void metrics_print_summary(void);
void metrics_reset(void);

// 個別メトリクス更新関数
void metrics_increment_interrupts(void);
void metrics_increment_context_switches(void);
void metrics_increment_keyboard_inputs(void);
void metrics_increment_serial_writes(void);
void metrics_set_memory_usage(uint32_t bytes);

/**
 * ===========================================
 * スレッド診断機能
 * ===========================================
 */
// スレッド診断情報収集・出力
void thread_diagnostics_collect(thread_t* thread, thread_diagnostics_t* diag);
void thread_diagnostics_print(const thread_diagnostics_t* diag);
void thread_diagnostics_print_all(void);
void thread_diagnostics_print_detailed(thread_t* thread);

// スレッド状態分析
uint32_t thread_stack_usage(const thread_t* thread);
uint32_t thread_cpu_usage_estimate(const thread_t* thread);
bool thread_is_responsive(const thread_t* thread);

/**
 * ===========================================
 * メモリ診断機能
 * ===========================================
 */
// メモリ整合性チェック・レイアウト表示
void memory_check_integrity(void);
void memory_print_layout(void);
void memory_print_detailed_layout(void);
uint32_t memory_get_usage(void);

// メモリ領域検証
bool memory_validate_range(uint32_t start, uint32_t size);
void memory_dump_region(uint32_t start, uint32_t size, const char* name);

// メモリ使用統計
memory_stats_t memory_get_statistics(void);

/**
 * ===========================================
 * 性能プロファイリング機能
 * ===========================================
 */

// プロファイル制御
void profile_start(const char* section_name);
void profile_end(const char* section_name);
void profile_print_results(void);
void profile_print_detailed_results(void);
void profile_reset(void);

// プロファイル結果取得
profile_result_t* profile_get_results(int* count);
void profile_export_csv(void);

/**
 * ===========================================
 * システムヘルスチェック機能
 * ===========================================
 */

// ヘルス状態定義
typedef enum {
    HEALTH_OK = 0,   // 正常
    HEALTH_WARNING,  // 警告
    HEALTH_ERROR,    // エラー
    HEALTH_CRITICAL  // 致命的
} health_status_t;

// ヘルスチェック実行・レポート出力
health_status_t system_health_check(void);
void system_health_print_report(void);
void system_health_print_detailed_report(void);
void system_maintenance_tasks(void);

// 個別ヘルスチェック機能
health_status_t health_check_memory(void);
health_status_t health_check_threads(void);
health_status_t health_check_interrupts(void);
health_status_t health_check_timing(void);

/**
 * ===========================================
 * デバッグコマンドインターフェース
 * ===========================================
 */

// 基本デバッグコマンド
void debug_command_help(void);
void debug_command_status(void);
void debug_command_threads(void);
void debug_command_memory(void);
void debug_command_metrics(void);
void debug_command_profile(void);
void debug_command_health(void);

// 拡張デバッグコマンド
void debug_command_interrupts(void);
void debug_command_scheduler(void);
void debug_command_keyboard(void);
void debug_command_serial(void);
void debug_command_timer(void);
void debug_command_dump(uint32_t address, uint32_t length);
void debug_command_trace(void);
void debug_command_benchmark(void);
void debug_command_stress_test(void);

// インタラクティブデバッグモード
void debug_enter_interactive_mode(void);
void debug_process_command(const char* command);

/**
 * ===========================================
 * ユーティリティマクロ
 * ===========================================
 */

// レベル別ログ出力マクロ
#define DEBUG_ERROR(msg, ...) \
    debug_log(DEBUG_LEVEL_ERROR, "[エラー] " msg, ##__VA_ARGS__)
#define DEBUG_WARN(msg, ...) \
    debug_log(DEBUG_LEVEL_WARN, "[警告] " msg, ##__VA_ARGS__)
#define DEBUG_INFO(msg, ...) \
    debug_log(DEBUG_LEVEL_INFO, "[情報] " msg, ##__VA_ARGS__)
#define DEBUG_VERBOSE(msg, ...) \
    debug_log(DEBUG_LEVEL_VERBOSE, "[詳細] " msg, ##__VA_ARGS__)

// 性能測定マクロ
#define PROFILE_FUNCTION_START() profile_start(__func__)
#define PROFILE_FUNCTION_END() profile_end(__func__)

#define PROFILE_SECTION_BEGIN(name) profile_start(name)
#define PROFILE_SECTION_END(name) profile_end(name)

// デバッグアサート
#define ASSERT_DEBUG(condition, msg)                                           \
    do {                                                                       \
        if (!(condition)) {                                                    \
            DEBUG_ERROR("アサート失敗: %s at %s:%d", msg, __FILE__, __LINE__); \
            system_health_print_report();                                      \
        }                                                                      \
    } while (0)

// メモリアクセス検証マクロ
#define VALIDATE_MEMORY_ACCESS(ptr, size)                          \
    do {                                                           \
        if (!memory_validate_range((uint32_t)(ptr), (size))) {     \
            DEBUG_ERROR("無効なメモリアクセス: 0x%08x (size: %u)", \
                        (uint32_t)(ptr), (size));                  \
        }                                                          \
    } while (0)

// メトリクス更新マクロ（自動化）
#define METRICS_AUTO_INCREMENT(metric) metrics_increment_##metric()

// 条件付きデバッグ出力
#define DEBUG_IF(condition, level, msg, ...)      \
    do {                                          \
        if (condition) {                          \
            debug_log(level, msg, ##__VA_ARGS__); \
        }                                         \
    } while (0)

#endif  // DEBUG_UTILS_H