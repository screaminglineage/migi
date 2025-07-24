#ifndef MIGI_C
#define MIGI_C

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

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

#define todof(...)                          \
    (printf("%s:%d: ", __FILE__, __LINE__), \
    printf(__VA_ARGS__),                    \
    putchar('\n'),                          \
    fflush(NULL),                           \
    __builtin_trap())

#define todo() todof("%s: not yet implemented!", __func__)

#define unreachable() todof("%s: unreachable!", __func__)
#define unreachablef todof

#define min(a, b) ((a) < (b)? (a): (b))
#define max(a, b) ((a) > (b)? (a): (b))
#define between(value, start, end) ((start) <= (value) && (value) <= (end))

// Return value if value lies in [low, high], otherwise return the respective end (low or high)
#define clamp(value, low, high) (max(min((value), (high)), (low)))

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


#define array_reserve_old(arr, len) \
do {                                                                         \
    size_t new_length = arr->length + len;                                   \
    if (new_length < (arr)->capacity) return (arr)->data + (arr)->length;    \
                                                                             \
    size_t new_capacity = ARRAY_INIT_CAPACITY;                               \
    if ((arr)->capacity > 0 || new_length >= ARRAY_INIT_CAPACITY) {          \
        new_capacity = next_power_of_two(new_length);                        \
    }                                                                        \
                                                                             \
    (arr)->data = realloc(sb->data, new_capacity);                           \
    assert((arr)->data, "array_reserve: allocation failed!");                \
    (arr)->capacity = new_capacity;                                          \
    return (arr)->data + (arr)->length;                                      \
} while (0)


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

#define array_swap_remove(array, index)                                          \
    (assertf((array)->length > 0, "array_swap_remove: remove from empty array"),  \
    assertf((index) < (array)->length, "array_swap_remove: index out of bounds"), \
    (array)->data[(index)] = (array)->data[(array)->length - 1],                 \
    (array)->length -= 1)                                                        \


#endif // MIGI_C
