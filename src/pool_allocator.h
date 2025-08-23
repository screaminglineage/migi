#ifndef MIGI_POOL_ALLOC_H
#define MIGI_POOL_ALLOC_H


// POOL_ALLOC_COUNT_ALLOCATIONS can be defined before including count the current number of allocated items

#include <stddef.h>

#include "arena.h"
#include "migi.h"


typedef struct PoolItem PoolItem;
struct PoolItem {
    PoolItem *next;
    byte data[];
};

typedef struct {
    Arena arena;
    PoolItem *free_list;
#ifdef POOL_ALLOC_COUNT_ALLOCATIONS
    size_t length;
#endif
} PoolAllocator;


static byte *pool_alloc_bytes(PoolAllocator *p, size_t count);
static void pool_free_bytes(PoolAllocator *p, byte *item);

// Free all allocations
static void pool_reset(PoolAllocator *p);


static byte *pool_alloc_bytes(PoolAllocator *p, size_t size) {
    PoolItem *item = NULL;
    if (p->free_list) {
        item = p->free_list;
        p->free_list = p->free_list->next;
    } else {
        item = arena_push_bytes(&p->arena, sizeof(PoolItem) + size, _Alignof(PoolItem));
    }
    item->next = NULL;

#ifdef POOL_ALLOC_COUNT_ALLOCATIONS
    p->length++;
#endif
    return item->data;
}

static void pool_free_bytes(PoolAllocator *p, byte *item) {
    PoolItem *pool_item = (PoolItem *)((uintptr_t)item - offsetof(PoolItem, data));
    pool_item->next = p->free_list;
    p->free_list = pool_item;
#ifdef POOL_ALLOC_COUNT_ALLOCATIONS
    p->length--;
#endif
}

static void pool_reset(PoolAllocator *p) {
    arena_reset_ex(&p->arena, true);
    p->free_list = NULL;
#ifdef POOL_ALLOC_COUNT_ALLOCATIONS
    p->length = 0;
#endif
}

// Used to create a pool allocator for storing a particular type
#define PoolAllocator(type) \
    union {                 \
        PoolAllocator p;    \
        type *type_var;     \
    }

#define pool_alloc(pool) \
    ((__typeof__((pool)->type_var)) pool_alloc_bytes(&(pool)->p, sizeof(*(pool)->type_var)))

#define pool_free(pool, item) \
    (pool_free_bytes(&(pool)->p, (byte *)(1? (item): (pool)->type_var)))


#endif // MIGI_POOL_ALLOC_H
