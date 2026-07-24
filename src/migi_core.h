#ifndef MIGI_CORE_H
#define MIGI_CORE_H

#include <stdarg.h>
#include <stdio.h>     // needed for prints in asserts
#include <stdbool.h>
#include <stdint.h>
#include <string.h>    // needed for mem_* functions


#ifdef MIGI_ENABLE_SANITIZERS
    #include <sanitizer/asan_interface.h>
#endif


// Determining OS, compiler, and architecture
// Taken from: https://github.com/EpicGames/raddebugger/blob/master/src/base/base_context_cracking.h
//
// Clang
#if defined(__clang__)
    #define COMPILER_CLANG 1

#if defined(_WIN32)
    #define OS_WINDOWS 1
#elif defined(__gnu_linux__) || defined(__linux__)
    #define OS_LINUX 1
#elif defined(__APPLE__) && defined(__MACH__)
    #define OS_MAC 1
#else
    #error Unsupported compiler/OS combo
#endif

#if defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64)
    #define ARCH_X64 1
#elif defined(i386) || defined(__i386) || defined(__i386__)
    #define ARCH_X86 1
#elif defined(__aarch64__)
    #define ARCH_ARM64 1
#elif defined(__arm__)
    #define ARCH_ARM32 1
#else
    #error Unsupported architecture
#endif


// MSVC
#elif defined(_MSC_VER)
    #define COMPILER_MSVC 1

#if defined(_WIN32)
    #define OS_WINDOWS 1
#else
    #error Unsupported compiler/OS combo (cursed usage of MSVC outside of windows)
#endif

#if defined(_M_AMD64)
    #define ARCH_X64 1
#elif defined(_M_IX86)
    #define ARCH_X86 1
#elif defined(_M_ARM64)
    #define ARCH_ARM64 1
#elif defined(_M_ARM)
    #define ARCH_ARM32 1
#else
    #error Unsupported architecture
#endif


// GCC
#elif defined(__GNUC__) || defined(__GNUG__)
    #define COMPILER_GCC 1

#if defined(__gnu_linux__) || defined(__linux__)
    #define OS_LINUX 1
#elif defined(_WIN32)
    #define OS_WINDOWS 1  // mingw
#else
    #error Unsupported OS/compiler combo
#endif

#if defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64)
    #define ARCH_X64 1
#elif defined(i386) || defined(__i386) || defined(__i386__)
    #define ARCH_X86 1
#elif defined(__aarch64__)
    #define ARCH_ARM64 1
#elif defined(__arm__)
    #define ARCH_ARM32 1
#else
    #error Unsupported architecture
#endif

#else
    #error Unsupported compiler
#endif

// Most things are shared between both gcc and clang
#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
    #define COMPILER_GCC_OR_CLANG 1
#endif

#if defined(COMPILER_GCC) && defined(OS_WINDOWS)
    #define COMPILER_MINGW 1
#endif

#if defined(ARCH_X64)
    #define ARCH_64BIT 1
#elif defined(ARCH_X86)
    #define ARCH_32BIT 1
#endif

#if ARCH_ARM32 || ARCH_ARM64 || ARCH_X64 || ARCH_X86
    #define ARCH_LITTLE_ENDIAN 1
#else
    #error Endianness of this architecture cannot be understood
#endif



typedef uint8_t byte;

// TODO: make these function like macros to prevent ambiguity with the order of operations
// For example 2/4*MS == (2/4)*MS but 2/(4*MS) is what is generally expected
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

#define align_of(type) _Alignof(type)
#define type_of(type) __typeof__(type)
#define bool_to_str(cond) ((cond)? S("true"): S("false"))
#define bool_to_cstr(cond) ((cond)? "true": "false")

// Obtains a pointer to the parent struct from a pointer to a member element
#define parent_of(T, member_name, elem) (T *)((uintptr_t)(elem) - offsetof(T, member_name))

// Array with a single element that decays to a pointer
// Needed for calls like `hashmap_put(&h, 1, foo)`, since `&1` is invalid
// NOTE: `type_of(value)` cannot be used here since if `value` is a c-string,
// then `type_of(value)` returns `char[1][LEN(cstr)]` instead of `char**`
#define address_of(typevar, value) ((type_of(typevar)[1]){value})


#if COMPILER_GCC_OR_CLANG
    #define migi_crash() __builtin_trap()
#elif COMPILER_MSVC
    #define migi_crash() __debugbreak()
#else
    #define migi_crash() (*(volatile int *)0 = 0)
#endif

#ifndef MIGI_DISABLE_ASSERTS
    #define assert(expr)                                                                   \
        (!(expr))?                                                                         \
            (fprintf(stderr, "%s:%d: assertion `%s` failed\n", __FILE__, __LINE__, #expr), \
            migi_crash())                                                                  \
        : (void)0

    #define assertf(expr, ...)                                                               \
        (!(expr))?                                                                           \
            (fprintf(stderr, "%s:%d: assertion `%s` failed: \"", __FILE__, __LINE__, #expr), \
            fprintf(stderr, __VA_ARGS__),                                                    \
            fprintf(stderr, "\"\n"),                                                         \
            migi_crash())                                                                    \
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
   #define avow(expr, ...)                                                               \
        (!(expr)                                                                         \
         ? fprintf(stderr, "%s:%d: assertion `%s` failed\n", __FILE__, __LINE__, #expr), \
           migi_crash()                                                                  \
         : (void)0)
#endif

#define crash_with_message(...)                      \
    (fprintf(stderr, "%s:%d: ", __FILE__, __LINE__), \
    fprintf(stderr, __VA_ARGS__),                    \
    fprintf(stderr, "\n"),                           \
    migi_crash())

#define static_assert(expr, msg) _Static_assert(expr, msg)
#define todo() crash_with_message("%s: not yet implemented!", __func__)
#define todof(...) crash_with_message(__VA_ARGS__)
#define incomplete() static_assert(false, "incomplete")
#define incomplete_msg(msg) static_assert(false, (msg))

// todo() which returns an expression of any type instead of the regular void expression
// Eg.: `int x = todo_expr(int);` will compile but crash at runtime
// Eg.: `int x = todo_exprf(int, "`%s` doesnt have a value!", "x");` is also the same
#define todo_expr(type) (todo(), (type){0})
#define todof_expr(type, ...) (todof(__VA_ARGS__), (type){0})

// GCC defines `unreachable` in stddef.h since C23 (why squat this name???!!!!)
// TODO: rename migi_unreachable to unreachable_code
#if OS_WINDOWS
    #define migi_unreachable() (crash_with_message("%s: migi_unreachable!", __func__), __assume(0))
    #define migi_unreachablef(...)  (crash_with_message(__VA_ARGS__), __assume(0))
#else
    #define migi_unreachable() crash_with_message("%s: migi_unreachable!", __func__)
    #define migi_unreachablef(...) crash_with_message(__VA_ARGS__)
#endif

#define unused(a) ((void)a)

#if COMPILER_GCC_OR_CLANG
    #define no_optimize_begin()     \
        _Pragma("GCC push_options") \
        _Pragma("GCC optimize(\"O0\")")

    #define no_optimize_end() _Pragma("GCC pop options")

#elif COMPILER_MSVC
    #define no_optimize_begin() __pragma(optimize("", off))
    #define no_optimize_end()   __pragma(optimize("", on))
#endif


#if COMPILER_GCC_OR_CLANG
    #define breakpoint() asm("int3")
#elif COMPILER_MSVC
    #define breakpoint() __debugbreak()
#else
    #error "breakpoint() not supported for this compiler"
#endif


// NOTE: memory_first_poisoned() returns a pointer to the first poisoned byte
// in the region, or NULL if the region was not poisoned
#ifdef MIGI_ENABLE_SANITIZERS
    #define memory_poison(mem, size)         __asan_poison_memory_region((mem), (size))
    #define memory_unpoison(mem, size)       __asan_unpoison_memory_region((mem), (size))
    #define memory_first_poisoned(mem, size) __asan_region_is_poisoned((mem), (size))
#else
    #define memory_poison(mem, size)
    #define memory_unpoison(mem, size)
    #define memory_first_poisoned(mem, size) NULL
#endif


// Useful for defining bit flags or selecting a particular bit
#define bit(n) (1ULL << (n))

// Fill n bits (Eg. bit_fill(4) == 0b1111)
#define bit_fill(n) (assertf(n < 64, "bit_fill: number must be lower than 64"), (1ULL << (n)) - 1ULL)


// Basic definitions so that migi_string.h doesnt need to always be included
typedef struct {
    char *data;
    size_t length;
} Str;

// TODO: find what other basic string functions should instead be here
#define S(str_lit)  ((Str){(str_lit), sizeof((str_lit)) - 1})
#define SArg(sv) (int)(sv).length, (sv).data
#define str_zero() ((Str){0})

static Str str_from(char *data, size_t length) {
    return (Str){
        .data   = data,
        .length = length
    };
}


typedef struct {
    Str *data;
    size_t length;
} StrSpan;
#define str_span(...) span(Str, StrSpan, __VA_ARGS__)
#define str_span_new(arena, ...) span_new((arena), Str, StrSpan, __VA_ARGS__)




typedef enum {
    Ordering_Eq =  0, // left == right
    Ordering_Gt =  1, // left > right
    Ordering_Lt = -1, // left < right
} Ordering;


#define macro__concat(A, B) A ## B
#define macro_concat(A, B) macro__concat(A, B)
#define make_unique(a) macro_concat(a, __LINE__)

// Checks that `obj` is a pointer to `type`
// This can be helpful to typecheck macros which take in a pointer and a type to
// ensure that they are the same. See `ring_buffer.h` for an example
// NOTE: Use like: check_type(int, int_arr)
#define check_type(type, obj) (1 == 0)? (type *)0: (obj)

// Same as check_type but uses the value to compare instead
#define check_type_value(type, obj) (void)((1 == 0)? (type){0}: (obj))


// Allows typechecking of printf-like format arguments
// `format_index` - index of format string in parameters
// `vararg_index` - index of var arg start in parameters
#if COMPILER_GCC_OR_CLANG
    #ifdef __MINGW_PRINTF_FORMAT
        #define migi_printf_format(format_index, vararg_index) __attribute__ ((format (__MINGW_PRINTF_FORMAT, format_index, vararg_index)))
    #else
        #define migi_printf_format(format_index, vararg_index) __attribute__ ((format (printf, format_index, vararg_index)))
    #endif
#else
    // TODO: implement windows version
    #define migi_printf_format(format_index, vararg_index)
#endif

#if COMPILER_GCC_OR_CLANG
    #define threadvar __thread
#elif COMPILER_MSVC
    #define threadvar __declspec(thread)
#else
    #error "threadvar is not supported for compiler"
#endif


#define mem_swap(a, b)   \
do {                     \
    type_of(a) temp = a; \
    a = b;               \
    b = temp;            \
} while(0)

// Incrementally shift command line arguments
#define shift_args(argc, argv) ((argc--), *(argv)++)
#define array_shift(array) (*(array)++)

#define array_len(array) (sizeof(array) / sizeof(*(array)))

// Creates a Span (any struct with a data and length) from an 
// array designated initializer and allocates the data on an arena
#define span_new(arena, T, SpanT, ...)                                                                \
    (SpanT){                                                                                          \
        .data = arena_copy_bytes(arena, (T[]){__VA_ARGS__}, sizeof((T[]){__VA_ARGS__}), align_of(T)), \
        .length = sizeof((T[]){__VA_ARGS__}) / sizeof(T),                                             \
    }

// Creates a Span (any struct with a data and length) from an array designated initializer
#define span(T, SpanT, ...)                               \
    (SpanT){                                              \
        .data = (T[]){__VA_ARGS__},                       \
        .length = sizeof((T[]){__VA_ARGS__}) / sizeof(T), \
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


#define list_print(head, type, item, ...)           \
do {                                                \
    printf("[");                                    \
    type *item = (head);                            \
    for (; item && item->next; item = item->next) { \
        printf(__VA_ARGS__);                        \
        printf(", ");                               \
    }                                               \
    if (item) printf(__VA_ARGS__);                  \
    printf("]\n");                                  \
} while (0)


#define mem_eq_array(a, b, length) \
    (memcmp((a), (b), sizeof(*(a))*(length)) == 0)

#define mem_eq(a, b) mem_eq_array(a, b, 1)

#define mem_clear_array(mem, length) \
    (memset((mem), 0, sizeof(*(mem))*(length)))

#define mem_clear(mem) mem_clear_array(mem, 1)


// Iterate over a dynamic array *by reference*
// Should be used like array_foreach(&array, i) { ... }
// NOTE: Get the current index using `item - array->data`
#define array_foreach(array, item)                      \
    for (type_of((array)->data) (item) = (array)->data; \
        item < (array)->data + (array)->length;         \
        item++)



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

// Set `result` to a value and goto the `end` label
// Useful for cleaning up stuff before exiting
#define goto_end_with(expr) \
do {                        \
    result = expr;          \
    goto end;               \
} while (0)



// Logging functions
typedef enum {
    Log_Debug,
    Log_Info,
    Log_Warning,
    Log_Error,

    Log_Count,
} LogLevel;

const char *global_log_level_names[] = {
    [Log_Debug]   = "DEBUG",
    [Log_Info]    = "INFO",
    [Log_Warning] = "WARNING",
    [Log_Error]   = "ERROR",
};
static_assert(array_len(global_log_level_names) == Log_Count, "the number of log levels has changed");

#ifdef MIGI_DEBUG_LOGS
    threadvar LogLevel MIGI_GLOBAL_LOG_LEVEL = Log_Debug;
#else
    threadvar LogLevel MIGI_GLOBAL_LOG_LEVEL = Log_Info;
#endif

// TODO: check if running in a tty, and use ansi colour escape codes in that case
// `context` is usually the name of the function (passed in as __func__) calling migi_log
// If it's NULL then it is not printed
migi_printf_format(5, 6)
static void migi_log_opt(LogLevel level, const char *file, int line, const char *context, const char *fmt, ...) {
    if (level < MIGI_GLOBAL_LOG_LEVEL) return;

    if (level == Log_Debug) {
        fprintf(stderr, "%s:%d: ", file, line);
    }

    fprintf(stderr, "[%s] ", global_log_level_names[level]);

    if (context) fprintf(stderr, "%s: ", context);

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

// Returns the previous log level
static LogLevel migi_log_set_level(LogLevel level) {
    LogLevel prev_log_level = MIGI_GLOBAL_LOG_LEVEL;
    MIGI_GLOBAL_LOG_LEVEL = level;
    return prev_log_level;
}

// migi_log_with_ctx will print the name of the function where the logger was called
#define migi_log_with_ctx(level, ...) migi_log_opt(level, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define migi_log(level, ...) migi_log_opt(level, __FILE__, __LINE__, NULL, __VA_ARGS__)

#define log_debug(...) migi_log_opt(Log_Debug, __FILE__, __LINE__, NULL, __VA_ARGS__)
#define log_info(...) migi_log_opt(Log_Info, __FILE__, __LINE__, NULL, __VA_ARGS__)
#define log_warning(...) migi_log_opt(Log_Warning, __FILE__, __LINE__, NULL, __VA_ARGS__)
#define log_error(...) migi_log_opt(Log_Error, __FILE__, __LINE__, NULL, __VA_ARGS__)

#endif // MIGI_CORE_H
