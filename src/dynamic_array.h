#ifndef MIGI_DYNAMIC_ARRAY_H
#define MIGI_DYNAMIC_ARRAY_H

#ifndef DYNAMIC_ARRAY_INIT_CAP
    #define DYNAMIC_ARRAY_INIT_CAP 4
#endif

#include "migi_core.h"

#ifdef DYNAMIC_ARRAY_USE_ARENA

#include "arena.h"

#define array_reserve(arena, arr, len)                                     \
do {                                                                       \
    size_t new_length = (arr)->length + (len);                             \
    if (new_length > (arr)->capacity) {                                    \
        size_t old_capacity = (arr)->capacity;                             \
        if ((arr)->capacity == 0 && new_length < DYNAMIC_ARRAY_INIT_CAP) { \
            (arr)->capacity = DYNAMIC_ARRAY_INIT_CAP;                      \
        } else {                                                           \
            (arr)->capacity = next_power_of_two(new_length);               \
        }                                                                  \
        (arr)->data = arena_realloc_bytes((arena), (arr)->data,            \
            (old_capacity)*sizeof((arr)->data[0]),                         \
            ((arr)->capacity)*sizeof((arr)->data[0]),                      \
            align_of(void *));                                             \
        assertf((arr)->data, "array_reserve: allocation failed");          \
    }                                                                      \
} while(0)


#define array_push(arena, array, item)         \
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

#define array_push(array, item)                \
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

#define array_pop(array)                                                 \
    (assertf((array)->length > 0, "array_pop: remove from empty array"), \
     (array)->data[--(array)->length])

#define array_last(array) ((array)->data[(array)->length - 1])

#define array_swap_remove(array, index)                                                 \
    ((void)(assertf((array)->length > 0, "array_swap_remove: remove from empty array"), \
    assertf((index) < (array)->length, "array_swap_remove: index out of bounds"),       \
    (array)->data[(index)] = array_last((array)),                                       \
    (array)->length -= 1))                                                              \


#endif // MIGI_DYNAMIC_ARRAY_H
