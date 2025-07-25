#ifndef MIGI_H
#define MIGI_H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#ifndef MIGI_DISABLE_ASSERTS
#define assert(expr)                                                            \
    (!(expr))?                                                                  \
        (printf("%s:%d: assertion `%s` failed\n", __FILE__, __LINE__, #expr),   \
        fflush(NULL),                                                           \
        __builtin_trap())                                                       \
    : (void)0

#define assertf(expr, ...)                                                        \
    (!(expr))?                                                                    \
        (printf("%s:%d: assertion `%s` failed: \"", __FILE__, __LINE__, #expr),   \
        printf(__VA_ARGS__),                                                      \
        putchar('"'),                                                             \
        putchar('\n'),                                                            \
        fflush(NULL),                                                             \
        __builtin_trap())                                                         \
    : (void)0
#else
#define assert(expr)
#define assertf(...)
#endif

#define static_assert _Static_assert

#define crash_with_message(...)             \
    (printf("%s:%d: ", __FILE__, __LINE__), \
    printf(__VA_ARGS__),                    \
    putchar('\n'),                          \
    fflush(NULL),                           \
    __builtin_trap())

#define todo() crash_with_message("%s: not yet implemented!", __func__)
#define todof crash_with_message

#define migi_unreachable() crash_with_message("%s: unreachable!", __func__)
#define migi_unreachablef crash_with_message

// Incrementally shift command line arguments
#define shift_args(argc, argv) ((argc--), *(argv)++)

#define min(a, b) ((a) < (b)? (a): (b))
#define max(a, b) ((a) > (b)? (a): (b))
#define between(value, start, end) ((start) <= (value) && (value) <= (end))

#define clamp_top(a, b) (min(a, b))
#define clamp_bottom(a, b) (max(a, b))

// Return value if it lies in [low, high], otherwise return the respective end (low or high)
#define clamp(value, low, high) (clamp_bottom(clamp_top((value), (high)), (low)))

// modulo that wraps-around to b - 1 if result is negative
#define modulo(a, b) ((a) - (b) * ((a) / (b)))

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

#define array_len(array) (sizeof(array) / sizeof(*array))

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

#define array_foreach(array, type, item)        \
    for (type *(item) = (array)->data;          \
        item < (array)->data + (array)->length; \
        item++)

#define defer_block(expr)          \
    for (int _DEFER##__LINE__ = 0; \
        _DEFER##__LINE__ != 1;     \
        _DEFER##__LINE__++, expr)  \



#define KB 1024ull
#define MB (1024ull*KB)
#define GB (1024ull*MB)
#define TB (1024ull*GB)

#define MS (1000ull)
#define US (1000ull*MS)
#define NS (1000ull*US)


#endif // MIGI_H
