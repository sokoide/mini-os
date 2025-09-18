#ifndef DEBUG_UTILS_H
#define DEBUG_UTILS_H

#include "error_types.h"
#include "kernel.h"

// Simple va_list implementation
typedef char* va_list;
#define va_start(ap, v) (ap = (va_list) & v + sizeof(v))
#define va_arg(ap, type) (*(type*)((ap += sizeof(type)) - sizeof(type)))
#define va_end(ap) (ap = (va_list)0)

// デバッグレベル定義
typedef enum {
    DEBUG_LEVEL_ERROR = 0,
    DEBUG_LEVEL_WARNING,
    DEBUG_LEVEL_INFO,
    DEBUG_LEVEL_DEBUG,
    DEBUG_LEVEL_VERBOSE
} debug_level_t;

// システムメトリクス構造体
typedef struct {
    uint32_t context_switches;
    uint32_t interrupts_handled;
    uint32_t keyboard_events;
    uint32_t timer_ticks;
    uint32_t memory_usage;
    uint32_t active_threads;
} system_metrics_t;

// デバッグ関数プロトタイプ
void debug_init(void);
void debug_set_level(debug_level_t level);
void debug_print(const char* format, ...);
void debug_print_level(debug_level_t level, const char* format, ...);
void simple_vsprintf(char* out_buf, size_t buf_size, const char* format,
                     va_list args);

// システム監視関数
void debug_update_metrics(void);
void debug_print_system_status(void);
void debug_print_thread_info(void);

// プロファイリング関数
void debug_profile_start(const char* section);
void debug_profile_end(const char* section);
void debug_print_profile_stats(void);

// メモリダンプ関数
void debug_memory_dump(void* ptr, size_t size);
void debug_stack_trace(void);

#endif  // DEBUG_UTILS_H