#ifndef MIGI_LINEAR_ARENA_H
#define MIGI_LINEAR_ARENA_H

// Fully contiguous Linear Arena using virtual address space mapping
//
// This works by reserving an extremely large part of the virtual address space
// without any permissions to either read or write. This effectively reserves the
// area for the current allocation without consuming any actual memory.
// When required, the memory is committed, by adding read/write permissions to the
// necessary pages. Similarly popping off allocations decommits the memory and removes
// these permissions.
//
// A lower total capacity can be used by creating the arena like so
// `LinearArena arena = { .total = 16*MB };`
//
// | => page boundary
// * => already allocated
// # => new allocation
// - => unused
// x => uncomitted (no permissions)
//
// |*********|***********|#######-------|xxxxxxxxxxx|xxxxxxxxx|xxxxxxxxxx|
// ^data                 ^new   ^length ^capacity                        ^total


#include <stddef.h>
#include <stdint.h>
#include <string.h>


#include "migi.h"
#include "migi_memory.h"

#define PROFILER_H_IMPLEMENTATION
#include "profiler.h"

typedef struct {
    byte *data;
    size_t length;
    size_t capacity;
    size_t total;
} LinearArena;


#define align_page_size(n) (align_up((n), OS_PAGE_SIZE))

static void *lnr_arena_push_bytes(LinearArena *arena, size_t size) {
    size_t alloc_end = arena->length + size;
    if (alloc_end > arena->capacity) {
        if (arena->capacity == 0) {
            size_t default_capacity = 32*GB;
            arena->total = (arena->total == 0)? default_capacity: arena->total;
            arena->data = memory_reserve(arena->total);
        }
        size_t new_capacity = align_page_size(alloc_end);
        avow(new_capacity <= arena->total, "arena_push: virtual address space mapping of %zu bytes exhausted", arena->total);

        size_t extra_length = new_capacity - arena->capacity; 
        memory_commit(arena->data + arena->capacity, extra_length);
        arena->capacity = new_capacity;
    }
    void *mem = arena->data + arena->length;
    arena->length = alloc_end;
    return mem;
}


// TODO: Since this function can potentially decommit memory, it is probably 
// not a good idea to return a pointer to the beginning of the popped region
static void *lnr_arena_pop_bytes(LinearArena *arena, size_t size) {
    TIME_FUNCTION;
    if (arena->capacity == 0) return arena->data;
    arena->length -= clamp_top(size, arena->length);

    // TODO: when should decommitting be done and how much memory should be decommitted?
    // NOTE: Currently removes everything past the next page boundary after arena->length
    // Maybe only decommit when `(capacity - length) > (0.5 * capacity)` (half empty)
    size_t new_capacity = align_page_size(arena->length);
    size_t extra_length = arena->capacity - new_capacity;
    if (extra_length > 0) {
        memory_decommit(arena->data + new_capacity, extra_length);
        arena->capacity = new_capacity;
    }

    return arena->data + arena->length;
}

static void *lnr_arena_memdup_bytes(LinearArena *arena, void *source, size_t size) {
    return memcpy(lnr_arena_push_bytes(arena, size), source, size);
}

void *lnr_arena_realloc_bytes(LinearArena *arena, void *old, size_t old_size, size_t new_size) {
    TIME_FUNCTION;
    if (old == NULL || old_size == 0) return lnr_arena_push_bytes(arena, new_size);
    if (new_size < old_size) return old;

    assertf(old_size <= arena->length, "%s: old_size is greater than arena length", __func__);
    // Extend previous allocation if it was the same as `old`
    if ((arena->data + arena->length) - old_size == old) {
        lnr_arena_push_bytes(arena, new_size - old_size);
        return old;
    }
    return memcpy(lnr_arena_push_bytes(arena, new_size), old, old_size);
}

void lnr_arena_free(LinearArena *arena) {
    if (arena->total > 0) memory_release(arena->data, arena->total);
    memset(arena, 0, sizeof(*arena));
}

uint64_t lnr_arena_save(LinearArena *arena) {
    return arena->length;
}

void lnr_arena_rewind(LinearArena *arena, uint64_t length) {
    lnr_arena_pop_bytes(arena, abs_difference(arena->length, length));
    arena->length = length;
}

// Convenience macros which multiply size with the sizeof(type)

#define lnr_arena_new(arena, type) \
    ((type *)arena_push_bytes((arena), sizeof(type)))

#define lnr_arena_push(arena, type, size) \
    ((type *)lnr_arena_push_bytes((arena), sizeof(type)*(size)))

#define lnr_arena_pop(arena, type, size) \
    ((type *)lnr_arena_pop_bytes((arena), sizeof(type)*(size)))

#define lnr_arena_memdup(arena, type, source, size) \
    ((type *)lnr_arena_memdup_bytes((arena), (void *)(source), sizeof(type)*(size)))

#define lnr_arena_strdup(arena, source, size) \
    ((char *)lnr_arena_memdup_bytes((arena), (void *)(source), (size)))

#define lnr_arena_realloc(arena, type, old, old_size, new_size) \
    ((type *)lnr_arena_realloc_bytes((arena), (void *)(old), sizeof(type)*(old_size), sizeof(type)*(new_size)))

#endif // MIGI_LINEAR_ARENA_H



