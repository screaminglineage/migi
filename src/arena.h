#ifndef MIGI_ARENA_H
#define MIGI_ARENA_H

#include <stddef.h>
#include <string.h>
#include <stdbool.h>

#include "migi.h"
#include "migi_memory.h"

#ifndef ARENA_DEFAULT_CAP
#   define ARENA_DEFAULT_CAP 16*KB
#endif

typedef struct Zone ArenaZone;
struct Zone {
    ArenaZone *next;
    size_t length;
    size_t capacity;
    byte data[];
};

typedef struct {
    ArenaZone *head;
    ArenaZone *tail;
} Arena;

typedef struct {
    ArenaZone *zone;
    size_t length;
} ArenaCheckpoint;


#define arena_new(arena, type) \
    ((type *)arena_push_bytes((arena), sizeof(type)))

#define arena_push(arena, type, size) \
    ((type *)arena_push_bytes((arena), sizeof(type)*(size)))

#define arena_realloc(arena, type, old, old_size, new_size) \
    ((type *)arena_realloc_bytes((arena), (void *)(old), sizeof(type)*(old_size), sizeof(type)*(new_size)))

#define arena_memdup(arena, type, old, size) \
    ((type *)arena_memdup_bytes((arena), (void *)(old), sizeof(type)*(size)))

#define arena_strdup(arena, old, size) \
    ((char *)arena_memdup_bytes((arena), (void *)(old), (size)))

void *arena_push_bytes(Arena *arena, size_t size);
void *arena_realloc_bytes(Arena *arena, void *old, size_t old_size, size_t new_size);
void *arena_memdup_bytes(Arena *arena, void *old, size_t size);

#define arena_reset(arena) (arena_reset_ex((arena), false))
void arena_reset_ex(Arena *arena, bool clear_all);

void arena_free(Arena *arena);

ArenaCheckpoint arena_checkpoint(Arena *arena);
#define arena_rewind(arena, checkpoint) (arena_rewind_ex((arena), (checkpoint), false))
void arena_rewind_ex(Arena *arena, ArenaCheckpoint checkpoint, bool clear_all);



static inline ArenaZone *arena_new_zone(size_t capacity, ArenaZone *next) {
    ArenaZone *zone = memory_alloc(sizeof(ArenaZone) + capacity);
    assertf(zone, "arena_zone_new: failed to allocated memory");
    zone->length = 0;
    zone->capacity = capacity;
    zone->next = next;
    return zone;
}

void arena_free(Arena *arena) {
    ArenaZone *zone = arena->head;
    while (zone) {
        ArenaZone *tmp = zone;
        zone = zone->next;
        memory_free(tmp, tmp->capacity);
    }
    arena->head = arena->tail = NULL;
}

// Free all zones after the current one
void arena_trim(Arena *arena) {
    ArenaZone *zone = arena->tail->next;
    while (zone) {
        ArenaZone *tmp = zone;
        zone = zone->next;
        memory_free(tmp, tmp->capacity);
    }
    arena->tail->next = NULL;
}

// Clear all zones without deallocating the memory with optional memset to 0
void arena_reset_ex(Arena *arena, bool clear_all) {
    for (ArenaZone *zone = arena->head; zone != NULL; zone = zone->next) {
        if (clear_all) memset(zone->data, 0, zone->capacity);
        zone->length = 0;
    }
    arena->tail = arena->head;
}


void *arena_push_bytes(Arena *a, size_t size) {
    if (!a->tail) {
        size_t new_capacity = (size <= ARENA_DEFAULT_CAP)? ARENA_DEFAULT_CAP: size;
        a->tail = arena_new_zone(new_capacity, a->tail);
        a->head = a->tail;
    } else {
        size_t alloc_end = a->tail->length + size;
        if (alloc_end > a->tail->capacity) {
            size_t new_capacity = (alloc_end <= ARENA_DEFAULT_CAP)? ARENA_DEFAULT_CAP: alloc_end;
            a->tail->next = arena_new_zone(new_capacity, a->tail->next);
            a->tail = a->tail->next;
        }
    }

    void *mem = a->tail->data + a->tail->length;
    a->tail->length += size;
    return mem;
}


// Reallocates an already allocated piece of memory
// If `old` is NULL then it simply behaves like calling arena_push_bytes with `new_size`
void *arena_realloc_bytes(Arena *a, void *old, size_t old_size, size_t new_size) {
    // allocate new arena if it doesnt already exist
    if (!a->tail || !old) return arena_push_bytes(a, new_size);
    if (new_size <= old_size) return old;

    // extend the old allocation if it was the last one
    if (old_size <= a->tail->length) {
        size_t old_offset = a->tail->length - old_size;
        if (a->tail->data + old_offset == old && old_offset + new_size <= a->tail->capacity) {
            a->tail->length += new_size - old_size;
            return old;
        }
    }

    return memcpy(arena_push_bytes(a, new_size), old, old_size);
}

void *arena_memdup_bytes(Arena *arena, void *old, size_t size) {
    return memcpy(arena_push_bytes(arena, size), old, size);
}

// Saves the current state of the arena
// This can be later used to rewind back to this point
ArenaCheckpoint arena_checkpoint(Arena *arena) {
    if (!arena->tail) {
        arena->tail = arena_new_zone(ARENA_DEFAULT_CAP, arena->tail);
        arena->head = arena->tail;
    }
    return (ArenaCheckpoint){
        .zone = arena->tail,
        .length = arena->tail->length
    };
}

// Rewinds the state of the arena to the checkpoint passed in
// The checkpoint *must* refer to a "smaller" arena than what is
// passed in to this function, as otherwise the behaviour will be incorrect
void arena_rewind_ex(Arena *arena, ArenaCheckpoint checkpoint, bool clear_all) {
    if (!arena->tail) return;
    arena->tail = checkpoint.zone;
    for (ArenaZone *zone = arena->tail->next; zone != NULL; zone = zone->next) {
        if (clear_all) memset(zone->data, 0, zone->capacity);
        zone->length = 0;
    }
    arena->tail->length = checkpoint.length;
}

#endif // MIGI_ARENA_H
