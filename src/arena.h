#ifndef MIGI_ARENA_H
#define MIGI_ARENA_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "migi.h"
#include "migi_memory.h"

#ifndef ARENA_DEFAULT_RESERVE_SIZE
    #define ARENA_DEFAULT_RESERVE_SIZE 1*GB
#endif

#ifndef ARENA_DEFAULT_COMMIT_SIZE
    #define ARENA_DEFAULT_COMMIT_SIZE 1*MB
#endif


// NOTE: The default type is Linear
typedef enum {
    Arena_Linear = 0,
    Arena_Chained,
    Arena_Static,
} ArenaType;

typedef struct Arena Arena;
struct Arena {
    ArenaType type;

    size_t position;
    size_t committed;
    size_t reserved;

    size_t commit_size;
    size_t reserve_size;

    Arena *prev;
    Arena *current;
    byte data[];
};

typedef struct {
    size_t commit_size;
    size_t reserve_size;
    size_t type;
} ArenaOptions;

typedef struct {
    Arena *arena;
    Arena *current;
    size_t position;
} Checkpoint;

#define arena_init(...)                             \
    arena__init((ArenaOptions){                     \
        .commit_size = ARENA_DEFAULT_COMMIT_SIZE,   \
        .reserve_size = ARENA_DEFAULT_RESERVE_SIZE, \
        .type = Arena_Linear,                       \
        __VA_ARGS__                                 \
    }, NULL, 0)

static inline Arena *arena_init_static(void *backing_buffer, size_t backing_buffer_size);
static void arena_free(Arena *arena);

#define arena_new(arena, type) \
    (type *)arena_push_bytes((arena), sizeof(type), _Alignof(type), true)

#define arena_push(arena, type, length) \
    (type *)arena_push_bytes((arena), (length)*sizeof(type), _Alignof(type), true)

#define arena_push_nonzero(arena, type, length) \
    (type *)arena_push_bytes((arena), (length)*sizeof(type), _Alignof(type), false)

#define arena_pop(arena, type, length) \
    arena_pop_bytes((arena), (length)*sizeof(type));

static void *arena_push_bytes(Arena *arena, size_t size, size_t align, bool clear_mem);
static void arena_pop_bytes(Arena *arena, size_t size);

#define arena_realloc(arena, type, old, old_length, new_length) \
    (type *)arena_realloc_bytes((arena), (old), (old_length)*sizeof(type), (new_length)*sizeof(type), _Alignof(type))

static void *arena_realloc_bytes(Arena *arena, void *old, size_t old_size, size_t new_size, size_t align);

#define arena_copy(arena, type, mem, length) \
    (type *)arena_copy_bytes((arena), (void *)(mem), (length)*sizeof(type), _Alignof(type))

static inline void *arena_copy_bytes(Arena *arena, void *mem, size_t size, size_t align);

static void arena_reset(Arena *arena);

static inline Checkpoint arena_save(Arena *arena);
static void arena_rewind(Checkpoint checkpoint);

// Create a temporary region within the arena
// NOTE: Using `break` will return early without rewinding the checkpoint
// Using `continue` will also return early but will rewind the checkpoint
#define arena_temp(a, temp)                 \
    for (Checkpoint temp = arena_save((a)); \
        temp.arena;                         \
        arena_rewind(temp), temp.arena = NULL)

static Arena *arena__init(ArenaOptions opt, void *backing_buffer, size_t backing_buffer_size) {
    byte *mem = backing_buffer;
    size_t reserved = backing_buffer_size;
    size_t committed = backing_buffer_size;
    size_t reserve_size = backing_buffer_size;
    size_t commit_size = backing_buffer_size;

    // backing buffer was not provided
    if (!mem) {
        // always align to page size
        reserve_size = align_up(opt.reserve_size, OS_PAGE_SIZE);
        // commit_size must not be greater than reserve_size
        commit_size = clamp_top(align_up(opt.commit_size, OS_PAGE_SIZE), reserve_size);

        mem = memory_reserve(reserve_size);
        reserved = reserve_size;
        memory_commit(mem, commit_size);
        committed = commit_size;
    }

    // ensures that the contents are always aligned properly (64 should always be aligned)
    static_assert(sizeof(Arena) == 64, "arena header size should be 64 bytes");
    assertf(committed >= sizeof(Arena), "arena header must fit within allocation");

    // plant the header at the beginning of the allocation
    Arena *arena = (Arena *)mem;
    arena->current = arena;
    arena->position = sizeof(Arena);
    arena->prev = NULL;

    arena->reserved = reserved;
    arena->committed = committed;

    // TODO: Change the default reserve sizes depending on if the arena is
    // chained or linear. Linear arenas for example should reserve much more
    // than 1*GB
    arena->commit_size  = commit_size;
    arena->reserve_size = reserve_size;

    arena->type = opt.type;
    return arena;
}

static inline Arena *arena_init_static(void *backing_buffer, size_t backing_buffer_size) {
    return arena__init((ArenaOptions){.type = Arena_Static}, backing_buffer, backing_buffer_size);
}

static void *arena_push_bytes(Arena *arena, size_t size, size_t align, bool clear_mem) {
    Arena *current = arena->current;
    size_t alloc_start = align_up(current->position, align);
    size_t alloc_end = alloc_start + size;

    // allocate new block for chained arena if it doesn't fit
    if (current->type == Arena_Chained && alloc_end > current->reserved) {
        // increase the reservation if the allocation size is bigger
        size_t commit_size = current->commit_size;
        size_t reserve_size = current->reserve_size;
        size_t effective_size = sizeof(Arena) + size;
        if (effective_size > reserve_size) {
            reserve_size = align_up(effective_size, align);
            commit_size = align_up(effective_size, align);
        }
        Arena *next = arena__init((ArenaOptions){
            .commit_size = commit_size,
            .reserve_size = reserve_size,
            .type = current->type
        }, NULL, 0);
        next->prev = current;
        current = next;
        arena->current = current;

        // update allocation offsets for the new block
        alloc_start = align_up(current->position, align);
        alloc_end = alloc_start + size;
    }

    // commit memory if needed
    if (current->type != Arena_Static && alloc_end > current->committed) {
        size_t new_committed = clamp_top(align_up(alloc_end, current->commit_size), current->reserved);
        memory_commit((byte *)current + current->committed, new_committed - current->committed);
        current->committed = new_committed;
    }
    avow(alloc_end <= current->reserved, "%s: out of memory", __func__);

    byte *mem = (byte *)current + alloc_start;
    if (clear_mem) mem_clear_array(mem, size);
    current->position = alloc_end;
    return mem;
}

static inline void *arena_copy_bytes(Arena *arena, void *mem, size_t size, size_t align) {
    if (!mem) {
        return arena_push_bytes(arena, size, align, true);
    }
    return memcpy(arena_push_bytes(arena, size, align, false), mem, size);
}

// TODO: try adding a arena_pop_to_bytes which pops to a certain index
// from the beginning of the arena
static void arena_pop_bytes(Arena *arena, size_t size) {
    Arena *current = arena->current;
    if (current->type == Arena_Chained) {
        while (current->prev && size + sizeof(Arena) >= current->position) {
            size = size + sizeof(Arena) - current->position;

            Arena *temp = current;
            current = current->prev;
            memory_release(temp, temp->reserved);
        }
        arena->current = current;
    }

    // account for overflow during pop
    size_t new_position = current->position - size;
    new_position = clamp(new_position, sizeof(Arena), current->position);

    // decommit excess region
    if (current->type != Arena_Static) {
        size_t new_committed = align_up(new_position, current->commit_size);
        memory_decommit((byte *)current + new_committed, current->committed - new_committed);
        current->committed = new_committed;
    }
    current->position = new_position;
}

static void *arena_realloc_bytes(Arena *arena, void *old, size_t old_size, size_t new_size, size_t align) {
    // behave like arena_push_bytes if old doesnt exist
    if (old == NULL || old_size == 0) return arena_push_bytes(arena, new_size, align, true);
    if (new_size <= old_size) return old;

    Arena *current = arena->current;
    // extend previous allocation if it was the same as `old`
    // TODO: maybe see if there are other ways to do this
    if (old_size <= current->position) {
        size_t old_offset = current->position - old_size;
        if ((byte *)current + old_offset == old && old_offset + new_size <= current->reserved) {
            current->position += new_size - old_size;

            if (current->position > current->committed) {
                size_t new_committed = align_up(current->position, current->commit_size);
                memory_commit((byte *)current + current->committed, new_committed - current->committed);
                current->committed = new_committed;
            }
            return old;
        }
    }
    return memcpy(arena_push_bytes(arena, new_size, align, false), old, old_size);
}

static void arena_reset(Arena *arena) {
    Arena *current = arena->current;
    while (current->prev) {
        Arena *temp = current;
        current = current->prev;
        memory_release(temp, temp->reserved);
    }
    current->position = sizeof(Arena);
    arena->current = current;
}

static void arena_free(Arena *arena) {
    Arena *current = arena->current;
    // no need to free the static arena as the buffer was provided by the user
    if (current->type == Arena_Static) {
        mem_clear_array((byte *)current, current->reserved);
    } else {
        while (current) {
            Arena *temp = current;
            current = current->prev;
            memory_release(temp, temp->reserved);
        }
    }
}

static inline Checkpoint arena_save(Arena *arena) {
    return (Checkpoint) {
        .arena = arena,
        .current = arena->current,
        .position = arena->current->position
    };
}

static void arena_rewind(Checkpoint checkpoint) {
    // TODO: assert that the current position is after the checkpoint
    assert(checkpoint.arena);
    assert(checkpoint.current);

    Arena *current = checkpoint.arena->current;
    // newer blocks may need to be freed for chained arenas
    while (current && current != checkpoint.current) {
        Arena *temp = current;
        current = current->prev;
        memory_release(temp, temp->reserved);
    }
    checkpoint.arena->current = current;

    size_t new_position = checkpoint.position;
    // decommit excess region
    if (current->type != Arena_Static) {
        size_t new_committed = align_up(new_position, current->commit_size);
        memory_decommit((byte *)current + new_committed, current->committed - new_committed);
        current->committed = new_committed;
    }
    current->position = new_position;
}

#endif // MIGI_ARENA_H
