#ifndef MIGI_H
#define MIGI_H

#include <stdio.h>     // needed for asserts
#include <stdbool.h>
#include <stdint.h>
#include <string.h>    // needed for array_extend
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

#define min(a, b) ((a) < (b)? (a): (b))
#define max(a, b) ((a) > (b)? (a): (b))
#define between(value, start, end) ((start) <= (value) && (value) <= (end))

#define clamp_top(a, b) (min(a, b))
#define clamp_bottom(a, b) (max(a, b))

// Return value if it lies in [low, high], otherwise return the respective end (low or high)
#define clamp(value, low, high) (clamp_bottom(clamp_top((value), (high)), (low)))

// modulo that wraps-around to b - 1 if result is negative
#define modulo(a, b) ((a) - (b) * ((a) / (b)))

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

// Aligns `value` to `align_to`
static inline uint64_t align_up(uint64_t value, uint64_t align_to) {
    return (value & (align_to - 1))? (value + align_to) & ~(align_to - 1): value;
}

#ifndef MIGI_DISABLE_ASSERTS

#ifdef __GNUC__
#   define migi_crash() __builtin_trap()
#elif _MSC_VER
#   define migi_crash() __debugbreak()
#else
#   define migi_crash() (*(volatile int *)0 = 0)
#endif

#   define assert(expr)                                                           \
        (!(expr))?                                                                \
            (printf("%s:%d: assertion `%s` failed\n", __FILE__, __LINE__, #expr), \
            fflush(NULL),                                                         \
            migi_crash())                                                         \
        : (void)0

#   define assertf(expr, ...)                                                       \
        (!(expr))?                                                                  \
            (printf("%s:%d: assertion `%s` failed: \"", __FILE__, __LINE__, #expr), \
            printf(__VA_ARGS__),                                                    \
            putchar('"'),                                                           \
            putchar('\n'),                                                          \
            fflush(NULL),                                                           \
            migi_crash())                                                           \
        : (void)0

#else
#   define assert(expr) ((void)(expr))
#   define assertf(expr, ...) ((void)(expr))
#endif

#define static_assert _Static_assert

#define crash_with_message(...)             \
    (printf("%s:%d: ", __FILE__, __LINE__), \
    printf(__VA_ARGS__),                    \
    putchar('\n'),                          \
    fflush(NULL),                           \
    migi_crash())

#define todo() crash_with_message("%s: not yet implemented!", __func__)
#define todof crash_with_message

// todo() which can return any expression instead of the regular void expression
// Eg.: `int x = todo_expr(x);` will compile but crash at runtime
// Eg.: `int x = todo_exprf(x, "`%s` doesnt have a value!", "x");` is also the same
#define todo_expr(x) (todo(), (x))
#define todo_exprf(x, ...) (todof(__VA_ARGS__), (x))

#define migi_unreachable() crash_with_message("%s: unreachable!", __func__)
#define migi_unreachablef crash_with_message

#define unused(a) ((void)a)

// Incrementally shift command line arguments
#define shift_args(argc, argv) ((argc--), *(argv)++)

#define array_shift(array) (*(array)++)

#define array_len(array) (sizeof(array) / sizeof(*array))

// Print an array with a pointer and a length. The printf
// format string for the type needs to be passed in as well
#define array_print(arr, length, fmt)           \
    printf("[");                                \
    for (size_t i = 0; i < (length) - 1; i++) { \
        printf(fmt", ", ((arr)[i]));            \
    }                                           \
    printf(fmt"]\n", (arr)[(length) - 1]);


#define ARRAY_INIT_CAPACITY 4

#define array_reserve(arr, len)                                                    \
    (((arr)->length + (len) > (arr)->capacity)                                     \
    ? ((arr)->capacity =                                                           \
            (((arr)->capacity == 0 && (arr)->length + (len) < ARRAY_INIT_CAPACITY) \
            ? ARRAY_INIT_CAPACITY                                                  \
            : next_power_of_two((arr)->length + (len))),                           \
       (arr)->data = realloc((arr)->data, sizeof(*(arr)) * (arr)->capacity),       \
       assertf(((arr))->data, "array_reserve: allocation failed!"),                \
       (arr)->data + (arr)->length)                                                \
    : (arr)->data + (arr)->length)


#define array_add(array, item) \
    (*array_reserve((array), 1) = item, (array)->length += 1)

#define array_extend(array, items)                          \
    (memcpy(array_reserve((array), (items)->length),        \
        (items)->data, sizeof(*(items)) * (items)->length), \
    (array)->length += (items)->length)

#define array_swap_remove(array, index)                                           \
    (assertf((array)->length > 0, "array_swap_remove: remove from empty array"),  \
    assertf((index) < (array)->length, "array_swap_remove: index out of bounds"), \
    (array)->data[(index)] = (array)->data[(array)->length - 1],                  \
    (array)->length -= 1)                                                         \

#define migi_mem_eq(a, b, length) \
    (memcmp((a), (b), (length)) == 0)


// Iterate over a dynamic array *by reference*
// Should be called like array_foreach(&array, int, i) { ... }
#define array_foreach(array, type, item)        \
    for (type *(item) = (array)->data;          \
        item < (array)->data + (array)->length; \
        item++)






// Slightly cursed macros that probably shouldn't be used much

// Defers execution of the passed in expression until the
// end of the block. 
// NOTE: Using `break` in this version of defer will return early
// from the block, skipping execution of the deferred expression. Using
// `return` will also have a similar effect, so use with care.
#define defer_block(expr)              \
    for (int _DEFER##__LINE__ = 0;     \
            _DEFER##__LINE__ != 1;     \
            _DEFER##__LINE__++, expr)

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

#endif // MIGI_H
