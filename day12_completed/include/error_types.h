#ifndef ERROR_TYPES_H
#define ERROR_TYPES_H

/**
 * Standard result type for OS operations
 * Promotes consistent error handling across all kernel components
 */
typedef enum {
    OS_SUCCESS = 0,              // Operation completed successfully
    OS_ERROR_NULL_POINTER,       // Null pointer passed as argument
    OS_ERROR_INVALID_PARAMETER,  // Invalid parameter value
    OS_ERROR_OUT_OF_MEMORY,      // Memory allocation failed
    OS_ERROR_RESOURCE_BUSY,      // Resource is currently in use
    OS_ERROR_NOT_FOUND,          // Requested item not found
    OS_ERROR_PERMISSION_DENIED,  // Operation not permitted
    OS_ERROR_TIMEOUT,            // Operation timed out
    OS_ERROR_HARDWARE_FAILURE,   // Hardware malfunction detected
    OS_ERROR_BUFFER_OVERFLOW,    // Buffer size exceeded
    OS_ERROR_INVALID_STATE,      // Object in invalid state for operation
    OS_ERROR_NOT_IMPLEMENTED,    // Feature not yet implemented
    OS_ERROR_UNKNOWN = -1        // Unknown error condition
} os_result_t;

/**
 * Check if result indicates success
 */
#define OS_SUCCESS_CHECK(result) ((result) == OS_SUCCESS)

/**
 * Check if result indicates failure
 */
#define OS_FAILURE_CHECK(result) ((result) != OS_SUCCESS)

/**
 * Error handling macro for early return on failure
 */
#define OS_RETURN_ON_ERROR(expr)         \
    do {                                 \
        os_result_t _result = (expr);    \
        if (OS_FAILURE_CHECK(_result)) { \
            return _result;              \
        }                                \
    } while (0)

#endif  // ERROR_TYPES_H