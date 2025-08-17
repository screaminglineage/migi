#ifndef MIGI_DYNAMIC_ARRAY_H
#define MIGI_DYNAMIC_ARRAY_H

#ifndef DYNAMIC_ARRAY_INIT_CAP
    #define DYNAMIC_ARRAY_INIT_CAP 4
#endif

#if defined(DYNAMIC_ARRAY_USE_ARENA) || defined(DYNAMIC_ARRAY_USE_LINEAR_ARENA)

#include "migi.h"

#if defined(DYNAMIC_ARRAY_USE_LINEAR_ARENA)
    #include "linear_arena.h"
    #define ARRAY_ARENA_REALLOC lnr_arena_realloc
#elif defined(DYNAMIC_ARRAY_USE_ARENA)
    #include "arena.h"
    #define ARRAY_ARENA_REALLOC arena_realloc
#endif

#define array_reserve(arena, arr, len)                                         \
do {                                                                           \
    size_t new_length = (arr)->length + (len);                                 \
    if (new_length > (arr)->capacity) {                                        \
        size_t old_capacity = (arr)->capacity;                                 \
        if ((arr)->capacity == 0 && new_length < DYNAMIC_ARRAY_INIT_CAP) {     \
            (arr)->capacity = DYNAMIC_ARRAY_INIT_CAP;                          \
        } else {                                                               \
            (arr)->capacity = next_power_of_two(new_length);                   \
        }                                                                      \
        (arr)->data = ARRAY_ARENA_REALLOC((arena), __typeof__((arr)->data[0]), \
                (arr)->data, old_capacity, (arr)->capacity);                   \
        assertf((arr)->data, "array_reserve: allocation failed");              \
    }                                                                          \
} while(0)                                                                     \


#define array_add(arena, array, item)          \
do {                                           \
    array_reserve((arena), (array), 1);        \
    (array)->data[(array)->length++] = (item); \
} while (0)

#define array_extend(arena, array, items)                  \
do {                                                       \
    array_reserve((arena), (array), (items)->length);      \
    memcpy((array)->data + (array)->length, (items)->data, \
            sizeof((items)->data[0]) * (items)->length);   \
    (array)->length += (items)->length;                    \
} while (0)

#else  // not using arena for allocations

#include <stdlib.h>  // needed for realloc
#include "migi.h"

#define array_reserve(arr, len)                                                       \
do {                                                                                  \
    size_t new_length = (arr)->length + (len);                                        \
    if (new_length > (arr)->capacity) {                                               \
        if ((arr)->capacity == 0 && new_length < DYNAMIC_ARRAY_INIT_CAP) {            \
            (arr)->capacity = DYNAMIC_ARRAY_INIT_CAP;                                 \
        } else {                                                                      \
            (arr)->capacity = next_power_of_two(new_length);                          \
        }                                                                             \
        (arr)->data = realloc((arr)->data, sizeof((arr)->data[0]) * (arr)->capacity); \
        assertf((arr)->data, "array_reserve: allocation failed");                     \
    }                                                                                 \
} while(0)                                                                            \

#define array_add(array, item)                 \
do {                                           \
    array_reserve((array), 1);                 \
    (array)->data[(array)->length++] = (item); \
} while (0)

#define array_extend(array, items)                         \
do {                                                       \
    array_reserve((array), (items)->length);               \
    memcpy((array)->data + (array)->length, (items)->data, \
            sizeof((items)->data[0]) * (items)->length);   \
    (array)->length += (items)->length;                    \
} while (0)

#endif // defined(DYNAMIC_ARRAY_USE_ARENA) || defined(DYNAMIC_ARRAY_USE_LINEAR_ARENA)


#define array_last(array) ((array)->data[(array)->length - 1])

#define array_swap_remove(array, index)                                                 \
    ((void)(assertf((array)->length > 0, "array_swap_remove: remove from empty array"), \
    assertf((index) < (array)->length, "array_swap_remove: index out of bounds"),       \
    (array)->data[(index)] = array_last((array)),                                       \
    (array)->length -= 1))                                                              \


#endif // MIGI_DYNAMIC_ARRAY_H
