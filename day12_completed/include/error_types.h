#ifndef ERROR_TYPES_H
#define ERROR_TYPES_H

// 基本型定義はkernel.hから提供される

// エラーコード定義
typedef enum {
    OS_SUCCESS = 0,
    OS_ERROR_NULL_POINTER = -1,
    OS_ERROR_INVALID_PARAMETER = -2,
    OS_ERROR_OUT_OF_MEMORY = -3,
    OS_ERROR_INVALID_STATE = -4,
    OS_ERROR_THREAD_LIMIT = -5,
    OS_ERROR_KEYBOARD_TIMEOUT = -6
} os_result_t;

// OS操作結果チェックマクロ（既存のコードから移行）
#define OS_SUCCESS_CHECK(result) ((result) == OS_SUCCESS)  // 成功チェック
#define OS_FAILURE_CHECK(result) ((result) != OS_SUCCESS)  // 失敗チェック

// エラーレベル定義（将来のデバッグ機能用）
typedef enum {
    ERROR_LEVEL_DEBUG = 0,
    ERROR_LEVEL_INFO,
    ERROR_LEVEL_WARNING,
    ERROR_LEVEL_ERROR,
    ERROR_LEVEL_CRITICAL
} error_level_t;

#endif  // ERROR_TYPES_H