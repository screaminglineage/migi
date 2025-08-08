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

static void *lnr_arena_push_bytes(LinearArena *arena, size_t size, size_t align) {
    byte *alloc_start = arena->data + arena->length;
    size_t alignment = align_up_padding((uintptr_t)alloc_start, align);
    size_t alloc_end = arena->length + alignment + size;

    if (alloc_end > arena->capacity) {
        if (arena->capacity == 0) {
            size_t default_capacity = 32*GB;
            arena->total = (arena->total == 0)? default_capacity: arena->total;
            arena->data = memory_reserve(arena->total);
        }
        size_t new_capacity = align_page_size(alloc_end);
        avow(new_capacity <= arena->total, "arena_push: reserved virtual address space of %zu bytes exhausted", arena->total);

        size_t extra_length = new_capacity - arena->capacity; 
        memory_commit(arena->data + arena->capacity, extra_length);
        arena->capacity = new_capacity;
    }

    void *mem = arena->data + arena->length + alignment;
    arena->length = alloc_end;
    return mem;
}


static void *lnr_arena_pop_bytes(LinearArena *arena, size_t size) {
    TIME_FUNCTION;
    if (arena->capacity == 0) return arena->data;
    size_t extra = clamp_top(size, arena->length);
    arena->length -= extra;

    // TODO: when should decommitting be done and how much memory should be decommitted?
    // NOTE: currently decommits when 4 empty pages are present between length and capacity
    if ((arena->capacity - arena->length) >= 4*OS_PAGE_SIZE) {
        size_t new_capacity = align_page_size(arena->length);
        size_t extra_length = arena->capacity - new_capacity;
        memory_decommit(arena->data + new_capacity, extra_length);
        arena->capacity = new_capacity;
    }

    return arena->data + arena->length;
}

// If `old` is NULL, then simply allocates `size` bytes in the arena
static void *lnr_arena_memdup_bytes(LinearArena *arena, void *old, size_t size, size_t align) {
    if (!old) {
        return lnr_arena_push_bytes(arena, size, align);
    }
    return memcpy(lnr_arena_push_bytes(arena, size, align), old, size);
}

// TODO: check if alignment needs to be factored in anywhere
static void *lnr_arena_realloc_bytes(LinearArena *arena, void *old, size_t old_size, size_t new_size, size_t align) {
    TIME_FUNCTION;
    if (old == NULL || old_size == 0) return lnr_arena_push_bytes(arena, new_size, align);
    if (new_size <= old_size) return old;

    assertf(old_size <= arena->length, "%s: old_size is greater than arena length", __func__);
    // Extend previous allocation if it was the same as `old`
    if ((arena->data + arena->length) - old_size == old) {
        lnr_arena_push_bytes(arena, new_size - old_size, align);
        return old;
    }
    return memcpy(lnr_arena_push_bytes(arena, new_size, align), old, old_size);
}

static void lnr_arena_free(LinearArena *arena) {
    if (arena->total > 0) memory_release(arena->data, arena->total);
    migi_mem_clear(arena, 1);
}

static uint64_t lnr_arena_save(LinearArena *arena) {
    return arena->length;
}

static void lnr_arena_rewind(LinearArena *arena, uint64_t length) {
    lnr_arena_pop_bytes(arena, abs_difference(arena->length, length));
}

// Convenience macros which multiply size with the sizeof(type)

#define lnr_arena_new(arena, type) \
    ((type *)lnr_arena_push_bytes((arena), sizeof(type), _Alignof(type)))

#define lnr_arena_push(arena, type, size) \
    ((type *)lnr_arena_push_bytes((arena), sizeof(type)*(size), _Alignof(type)))

#define lnr_arena_pop(arena, type, size) \
    ((type *)lnr_arena_pop_bytes((arena), sizeof(type)*(size)))

#define lnr_arena_memdup(arena, type, source, size) \
    ((type *)lnr_arena_memdup_bytes((arena), (void *)(source), sizeof(type)*(size), _Alignof(type)))

#define lnr_arena_strdup(arena, source, size) \
    ((char *)lnr_arena_memdup_bytes((arena), (void *)(source), (size), _Alignof(char)))

// NOTE: arena_realloc should be passed in the same type as the original allocation
#define lnr_arena_realloc(arena, type, old, old_size, new_size) \
    ((type *)lnr_arena_realloc_bytes((arena), (void *)(old), sizeof(type)*(old_size), sizeof(type)*(new_size), _Alignof(type)))

#endif // MIGI_LINEAR_ARENA_H



