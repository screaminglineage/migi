#ifndef MIGI_LINEAR_ARENA_H
#define MIGI_LINEAR_ARENA_H

// Linear Arena using virtual address space mapping
//
// | => page boundary
// * => already allocated
// # => new allocation
// x => "non-comitted"
//
// |*********|***********|#######-------|xxxxxxxxxxx|xxxxxxxxx|xxxxxxxxxx|
// ^data                 ^new   ^length ^capacity                        ^total

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>


#include "migi.h"

#define PROFILER_H_IMPLEMENTATION
// #define ENABLE_PROFILING
#include "profiler.h"

#ifdef _WIN32
#error "windows not yet supported!"
#else

#include <sys/mman.h>

#define OS_PAGE_SIZE 4*KB

static byte *memory_reserve(size_t size) {
    TIME_FUNCTION;
    byte *mem = mmap(0, size, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    assertf(mem != MAP_FAILED, "%s: failed to map memory: %s", __func__, strerror(errno));
    return mem;
}

static void memory_release(byte *mem, size_t size) {
    TIME_FUNCTION;
    int ret = munmap(mem, size);
    assertf(ret != -1, "%s: failed to unmap memory: %s", __func__, strerror(errno));
}

// TODO: benchmark commit and decommit to see how long they actually take
// NOTE: memory_commit and memory_decommit must be passed in addresses aligned to the OS page boundary
static byte *memory_commit(byte *mem, size_t size) {
    TIME_FUNCTION;
    byte *allocation = mmap(mem, size, PROT_READ|PROT_WRITE, MAP_FIXED|MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    assertf(allocation != MAP_FAILED, "%s: failed to commit memory: %s", __func__, strerror(errno));
    return allocation;
}

// NOTE: memory_commit and memory_decommit must be passed in addresses aligned to the OS page boundary
static void memory_decommit(byte *mem, size_t size) {
    TIME_FUNCTION;
    byte *new_mem = mmap(mem, size, PROT_NONE, MAP_FIXED|MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    assertf(new_mem != MAP_FAILED, "%s: failed to decommit memory: %s", __func__, strerror(errno));
}

#endif

#define LINEAR_ARENA_DEFAULT_CAP 32*GB

typedef struct {
    uint8_t *data;
    size_t length;
    size_t capacity;
    size_t total;
} LinearArena;


#define align_page_size(n) (((n) & (OS_PAGE_SIZE - 1))? ((n + OS_PAGE_SIZE) & ~(OS_PAGE_SIZE - 1)) :(n))
// #define align_down_page_size(n) ((n) & OS_PAGE_SIZE)

static byte *lnr_arena_push_bytes(LinearArena *arena, size_t size) {
    if (size == 0) return arena->data;

    size_t alloc_end = arena->length + size;
    if (alloc_end > arena->capacity) {
        if (arena->capacity == 0) {
            arena->total = (arena->total == 0)? LINEAR_ARENA_DEFAULT_CAP: arena->total;
            arena->data = memory_reserve(arena->total);
        }
        size_t new_capacity = align_page_size(alloc_end);
        assertf(new_capacity <= arena->total, "arena_push: virtual address space mapping of %zu bytes exhausted", arena->total);

        size_t extra_length = new_capacity - arena->capacity; 
        memory_commit(arena->data + arena->capacity, extra_length);
        arena->capacity = new_capacity;
    }
    byte *mem = arena->data + arena->length;
    arena->length = alloc_end;
    return mem;
}


// TODO: Since this function can potentially decommit memory, it is probably 
// not a good idea to return a pointer to the beginning of the popped region
static byte *lnr_arena_pop_bytes(LinearArena *arena, size_t size) {
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

static byte *lnr_arena_memdup_bytes(LinearArena *arena, byte *source, size_t size) {
    return memcpy(lnr_arena_push_bytes(arena, size), source, size);
}

byte *lnr_arena_realloc_bytes(LinearArena *arena, byte *old, size_t old_size, size_t new_size) {
    TIME_FUNCTION;
    if (old == NULL || old_size == 0) return lnr_arena_push_bytes(arena, new_size);
    if (new_size < old_size) return old;

    assertf(old_size <= arena->length, "%s: old_size is greater than arena length", __func__);
    // Extend previous allocation if it was the same as `old`
    if ((arena->data + arena->length) - old_size == old) {
        lnr_arena_push_bytes(arena, new_size - old_size);
        return old;
    }
    // TODO: look into mremap
    return memcpy(lnr_arena_push_bytes(arena, new_size), old, old_size);
}

void lnr_arena_free(LinearArena *arena) {
    if (arena->total > 0) memory_release(arena->data, arena->total);
    memset(arena, 0, sizeof(*arena));
}

// Convenience macros which multiply size with the sizeof(type)

#define lnr_arena_push(arena, type, size) \
    ((type *)lnr_arena_push_bytes((arena), sizeof(type)*(size)))

#define lnr_arena_pop(arena, type, size) \
    ((type *)lnr_arena_pop_bytes((arena), sizeof(type)*(size)))

#define lnr_arena_memdup(arena, type, source, size) \
    ((type *)lnr_arena_memdup_bytes((arena), (byte *)(source), sizeof(type)*(size)))

#define lnr_arena_strdup(arena, source, size) \
    ((char *)lnr_arena_memdup_bytes((arena), (byte *)(source), (size)))

#define lnr_arena_realloc(arena, type, old, old_size, new_size) \
    ((type *)lnr_arena_realloc_bytes((arena), (byte *)(old), sizeof(type)*(old_size), sizeof(type)*(new_size)))

#endif // MIGI_LINEAR_ARENA_H



