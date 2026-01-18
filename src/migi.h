#ifndef MIGI_H
#define MIGI_H

#include <stdarg.h>
#include <stdio.h>     // needed for prints in asserts
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef uint8_t byte;

#define KB 1024ull
#define MB (1024ull*KB)
#define GB (1024ull*MB)
#define TB (1024ull*GB)
#define PB (1024ull*TB)

#define MS (1000ull)
#define US (1000ull*MS)
#define NS (1000ull*US)
#define PS (1000ull*NS)
#define FS (1000ull*PS)

#define migi_min(a, b) ((a) < (b)? (a): (b))
#define migi_max(a, b) ((a) > (b)? (a): (b))
#define between(value, start, end) ((start) <= (value) && (value) <= (end))

#define clamp_top(a, b) (migi_min(a, b))
#define clamp_bottom(a, b) (migi_max(a, b))

// Return value if it lies in [low, high], otherwise return the respective end (low or high)
#define clamp(value, low, high) (clamp_bottom(clamp_top((value), (high)), (low)))

// modulo that wraps-around to b - 1 if result is negative
#define modulo(a, b) ((a) - (b) * ((a) / (b)))

#define abs_difference(a, b) ((a) > (b)? (a) - (b): (b) - (a))

// NOTE: Returns `n` if its already a power of two
static uint64_t next_power_of_two(uint64_t n) {
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    n++;
    return n;
}

// TODO: rename these to align_*_pow2

// Return the number of bytes needed to align `value` to the next multiple of `align_to`
static inline uint64_t align_up_padding(uint64_t value, uint64_t align_to) {
    return -value & (align_to - 1);
}

// Return the number of bytes needed to align `value` to the previous multiple of `align_to`
static inline uint64_t align_down_padding(uint64_t value, uint64_t align_to) {
    return value & (align_to - 1);
}

// Align up `value` to the next multiple of `align_to`
// Returns `value` if it is already aligned
// align_up(9, 8) = 16 [next multiple of 8]
static inline uint64_t align_up(uint64_t value, uint64_t align_to) {
    return value + align_up_padding(value, align_to);
}

// Align down `value` to the previous multiple of `align_to`
// Returns `value` if it is already aligned
// align_down(21, 8) = 16 [prev multiple of 8]
static inline uint64_t align_down(uint64_t value, uint64_t align_to) {
    return value - align_down_padding(value, align_to);
}


#if defined(__GNUC__) || defined (__clang__)
    #define migi_crash() __builtin_trap()
#elif defined(_MSC_VER)
    #define migi_crash() __debugbreak()
#else
    #define migi_crash() (*(volatile int *)0 = 0)
#endif

#ifndef MIGI_DISABLE_ASSERTS
    #define assert(expr)                                                          \
        (!(expr))?                                                                \
            (printf("%s:%d: assertion `%s` failed\n", __FILE__, __LINE__, #expr), \
            fflush(NULL),                                                         \
            migi_crash())                                                         \
        : (void)0

    #define assertf(expr, ...)                                                      \
        (!(expr))?                                                                  \
            (printf("%s:%d: assertion `%s` failed: \"", __FILE__, __LINE__, #expr), \
            printf(__VA_ARGS__),                                                    \
            putchar('"'),                                                           \
            putchar('\n'),                                                          \
            fflush(NULL),                                                           \
            migi_crash())                                                           \
        : (void)0

// NOTE: Use avow when the application should fail even if asserts are disabled
// For example, if the memory allocator fails for some reason.
    #define avow assertf

#else
// assert is implemented as an expression and so it must be replaced by the
// expression passed in this also keeps the expression even if asserts are
// disabled
    #define assert(expr) ((void)(expr))
    #define assertf(expr, ...) ((void)(expr))

// fallback to slightly simpler implementation when asserts are disabled
   #define avow(expr, ...)                                                      \
        (!(expr)                                                                \
         ? printf("%s:%d: assertion `%s` failed\n", __FILE__, __LINE__, #expr), \
           migi_crash()                                                         \
         : (void)0)
#endif


#if defined( __STDC_VERSION__ ) && __STDC_VERSION__ >= 201112L
    #define static_assert(expr, msg) _Static_assert(expr, msg)
#else
    #if defined(_MSC_VER)
        #define static_assert(expr, msg) enum { static__assert__tmp(__COUNTER__) = 1 / (!!(expr)) }
    #else
        #define static_assert(expr, msg) \
            typedef char static__assert__tmp[!(expr)? -1: 1] __attribute__((unused))
    #endif
#endif

#define crash_with_message(...)             \
    (printf("%s:%d: ", __FILE__, __LINE__), \
    printf(__VA_ARGS__),                    \
    putchar('\n'),                          \
    fflush(NULL),                           \
    migi_crash())

#define todo() crash_with_message("%s: not yet implemented!", __func__)
#define todof(...) crash_with_message(__VA_ARGS__)

// todo() which returns an expression of any type instead of the regular void expression
// Eg.: `int x = todo_expr(int);` will compile but crash at runtime
// Eg.: `int x = todo_exprf(int, "`%s` doesnt have a value!", "x");` is also the same
#define todo_expr(type) (todo(), (type){0})
#define todof_expr(type, ...) (todof(__VA_ARGS__), (type){0})

#define migi_unreachable() crash_with_message("%s: unreachable!", __func__)
#define migi_unreachablef crash_with_message

#define unused(a) ((void)a)

#define macro__concat(A, B) (A ## B)
#define macro_concat(A, B) macro__concat(A, B)
#define make_unique(a) macro_concat(a, __LINE__)


// Allows typechecking of printf-like format arguments
// `format_index` - index of format string in parameters
// `vararg_index` - index of var arg start in parameters
#if defined(__GNUC__) || defined(__clang__)
    #ifdef __MINGW_PRINTF_FORMAT
        #define migi_printf_format(format_index, vararg_index) __attribute__ ((format (__MINGW_PRINTF_FORMAT, format_index, vararg_index)))
    #else
        #define migi_printf_format(format_index, vararg_index) __attribute__ ((format (printf, format_index, vararg_index)))
    #endif
#else
    // TODO: implement windows version
    #define migi_printf_format(format_index, vararg_index)
#endif


#define mem_swap(type, a, b) \
do {                         \
    type temp = a;           \
    a = b;                   \
    b = temp;                \
} while(0)

// Incrementally shift command line arguments
#define shift_args(argc, argv) ((argc--), *(argv)++)

#define array_shift(array) (*(array)++)

#define array_len(array) (sizeof(array) / sizeof(*(array)))



// Creates a Slice (any struct with a data and length) from an 
// array designated initializer and allocate the data on an arena
#define slice_new(arena, type, slice, ...)                                                                     \
    (slice){                                                                                                   \
        .data = arena_copy_bytes(arena, (type[]){__VA_ARGS__}, sizeof((type[]){__VA_ARGS__}), _Alignof(type)), \
        .length = sizeof((type[]){__VA_ARGS__}) / sizeof(type),                                                \
    }

// Creates a Slice (any struct with a data and length) from an array designated initializer
#define slice_from(type, slice, ...)                            \
    (slice){                                                    \
        .data = (type[]){__VA_ARGS__},                          \
        .length = sizeof((type[]){__VA_ARGS__}) / sizeof(type), \
    }

// Print an array with a pointer and a length. The printf
// format string for the type needs to be passed in as well
#define array_print(arr, length, fmt)                   \
do {                                                    \
    printf("[");                                        \
    for (size_t i = 0; i < (length) - 1; i++) {         \
        printf(fmt", ", (arr)[i]);                      \
    }                                                   \
    if ((length) > 0) printf(fmt, (arr)[(length) - 1]); \
    printf("]\n");                                      \
} while (0)


#define list_print(head, type, ...)                 \
do {                                                \
    printf("[");                                    \
    type *node = (head);                            \
    for (; node && node->next; node = node->next) { \
        printf(__VA_ARGS__);                        \
        printf(", ");                               \
    }                                               \
    if (node) printf(__VA_ARGS__);                  \
    printf("]\n");                                  \
} while (0)


#define mem_eq_array(a, b, length) \
    (memcmp((a), (b), sizeof(*(a))*(length)) == 0)

#define mem_eq(a, b) mem_eq_array(a, b, 1)

#define mem_clear_array(mem, length) \
    (memset((mem), 0, sizeof(*(mem))*(length)))

#define mem_clear(mem) mem_clear_array(mem, 1)


// TODO: add an enumerated version of this?
// Iterate over a dynamic array *by reference*
// Should be used like array_foreach(&array, int, i) { ... }
#define array_foreach(array, type, item)        \
    for (type *(item) = (array)->data;          \
        item < (array)->data + (array)->length; \
        item++)


// Iterate over a linked list
#define list_foreach(head, type, item) \
    for (type *(item) = (head); (item); (item) = (item)->next)


// Slightly cursed macros that probably shouldn't be used much

// Defers execution of the passed in expression until the
// end of the block. 
// NOTE: Using `break` in this version of defer will return early
// from the block, skipping execution of the deferred expression. Using
// `return` will also have a similar effect, so use with care.
#define defer_block(expr)                   \
    for (int make_unique(DEFER_TEMP_) = 0;  \
             make_unique(DEFER_TEMP_) != 1; \
             make_unique(DEFER_TEMP_)++, expr)

// Return false if the expression passed is false
// Also takes variable arguments that are run before returning
// Useful for propagating errors up the call stack
#define return_if_false(expr, ...) \
    if (!(expr)) {                 \
        __VA_ARGS__;               \
        return false;              \
    }

// Version of return_if_false that supports returning
// custom values. For instance, this may be useful
// when returning error codes from main instead of booleans.
#define return_val_if_false(expr, val, ...) \
    if (!(expr)) {                          \
        __VA_ARGS__;                        \
        return val;                         \
    }

typedef enum {
    Log_Debug,
    Log_Info,
    Log_Warning,
    Log_Error,

    Log_Count,
} LogLevel;

static struct {
    const char *name;
    const char *colour_code;
} MIGI_LOG_LEVELS[] = {
    [Log_Debug]   = { "DEBUG",   "\033[35m" },
    [Log_Info]    = { "INFO",    "\033[32m" },
    [Log_Warning] = { "WARNING", "\033[33m" },
    [Log_Error]   = { "ERROR",   "\033[31m" },
};
static_assert(array_len(MIGI_LOG_LEVELS) == Log_Count, "the number of log levels has changed");

// TODO: maybe make this thread_local?
#ifdef MIGI_DEBUG_LOGS
    static LogLevel MIGI_GLOBAL_LOG_LEVEL = Log_Debug;
#else
    static LogLevel MIGI_GLOBAL_LOG_LEVEL = Log_Info;
#endif

// `context` is usually the name of the function (passed in as __func__) calling migi_log
migi_printf_format(5, 6)
static void migi_log_ex(LogLevel level, const char *file, int line, const char *context, const char *fmt, ...) {
    if (level < MIGI_GLOBAL_LOG_LEVEL) return;

    if (level == Log_Debug) {
        fprintf(stderr, "%s:%d: ", file, line);
    }

    const char *bold_code  = "\033[1m";
    const char *reset_code = "\033[0m";
    fprintf(stderr, "%s%s[%s]%s %s: ", bold_code, MIGI_LOG_LEVELS[level].colour_code,
            MIGI_LOG_LEVELS[level].name, reset_code, context);

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

#define migi_log(level, ...) migi_log_ex(level, __FILE__, __LINE__, __func__, __VA_ARGS__)

#endif // MIGI_H
