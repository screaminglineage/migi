#ifndef MIGI_POOL_ALLOC_H
#define MIGI_POOL_ALLOC_H

#include "arena.h"
#include "migi_core.h"

#define POOL_ALLOC_ACTIVE (PoolItem *)0x1

#ifndef POOL_ALLOC_DEFAULT_CAP
    #define POOL_ALLOC_DEFAULT_CAP 256
#endif

typedef struct PoolItem PoolItem;
struct PoolItem {
    PoolItem *next;
    byte data[];
};

// Used to create a pool allocator for storing a particular type
#define PoolAlloc(T)             \
    union {                      \
        T *_item;                \
        struct {                 \
            PoolItem *data;      \
            size_t length;       \
            size_t capacity;     \
            PoolItem *free_list; \
        };                       \
    }

static void *pool_alloc_bytes(Arena *arena, size_t capacity, size_t elem_size,
                       PoolItem **free_list, size_t *length, PoolItem **data);

#define pool_alloc(arena, pool)                                                             \
    (type_of((pool)->_item)) pool__alloc((arena), sizeof(*(pool)->_item), &(pool)->capacity, \
                                        &(pool)->free_list, &(pool)->length, &(pool)->data)

#define pool_dealloc(pool, elem) \
    *((type_of((pool)->_item)) pool__dealloc(elem, &(pool)->free_list, &(pool)->length))

#define pool_reset(pool) \
    pool__reset((byte *)(pool)->data, sizeof(*(pool)->_item), (pool)->capacity, &(pool)->free_list, &(pool)->length)

#define pool_foreach(pool, elem)                                                                         \
    for (type_of((pool)->_item) elem = (type_of((pool)->_item))(pool)->data->data;                         \
        pool__item(elem) < pool__item_index((pool)->data, sizeof(*(pool)->_item), (pool)->capacity);     \
        elem = (type_of((pool)->_item))(pool__item_next(pool__item(elem), sizeof(*(pool)->_item)))->data) \
        if ((pool__item(elem))->next == POOL_ALLOC_ACTIVE)

// Internal implementation macros
#define pool__item(elem)                          (PoolItem *)((uintptr_t)(elem) - offsetof(PoolItem, data))
#define pool__item_size(elem_size)                align_up_pow2(sizeof(PoolItem) + (elem_size), align_of(PoolItem))
#define pool__item_index(start, elem_size, index) (PoolItem *)((uintptr_t)(start) + pool__item_size((elem_size))*(index)) 
#define pool__item_next(pool_item, elem_size)     (PoolItem *)((uintptr_t)(pool_item) + pool__item_size((elem_size)))


static void *pool__alloc(Arena *arena, size_t elem_size, size_t *capacity,
                              PoolItem **free_list, size_t *length, PoolItem **data) {
    if (*data == NULL) {
        if (*capacity == 0) {
            *capacity = POOL_ALLOC_DEFAULT_CAP;
        }
        *data = arena_push_bytes(arena, pool__item_size(elem_size) * *capacity, align_of(PoolItem), .zeroed=false);
    }

    assertf(*length < *capacity, "pool_alloc_bytes: pool out of capacity");

    PoolItem *item = NULL;
    if (*free_list) {
        item = *free_list;
        *free_list = (*free_list)->next;
    } else {
        item = pool__item_index(*data, elem_size, *length);
    }
    // used in pool_foreach to check whether the `pool_item` is active or not
    item->next = POOL_ALLOC_ACTIVE;

    (*length)++;
    mem_clear_array(item->data, elem_size);
    return item->data;
}

static void *pool__dealloc(void *elem, PoolItem **free_list, size_t *length) {
    if (elem == NULL || *length == 0) {
        return NULL;
    }

    PoolItem *pool_item = pool__item(elem);
    assertf(pool_item->next == POOL_ALLOC_ACTIVE, "pool_dealloc_bytes: double free");

    pool_item->next = *free_list;
    *free_list = pool_item;
    (*length)--;

    return pool_item->data;
}

static void pool__reset(byte *data, size_t elem_size, size_t capacity, PoolItem **free_list, size_t *length) {
    *free_list = NULL;
    *length = 0;
    mem_clear_array(data, pool__item_size(elem_size)*capacity);
}



#endif // MIGI_POOL_ALLOC_H
