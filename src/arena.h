#ifndef MIGI_ARENA_H
#define MIGI_ARENA_H

#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <sys/select.h>

#include "migi.h"
#include "migi_memory.h"

#ifndef ARENA_DEFAULT_ZONE_CAP
   #define ARENA_DEFAULT_ZONE_CAP 16*KB
#endif

// TODO: define functions as static
// TODO: use a flag to optionally clear/not clear the newly allocated memory in arena_push_bytes

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


void *arena_push_bytes(Arena *arena, size_t size, size_t align);

#define arena_new(arena, type) \
    ((type *)arena_push_bytes((arena), sizeof(type), _Alignof(type)))

#define arena_push(arena, type, size) \
    ((type *)arena_push_bytes((arena), sizeof(type)*(size), _Alignof(type)))

void *arena_pop_current_bytes(Arena *arena, size_t size);
#define arena_pop_current(arena, type, size) \
    ((type *)arena_pop_current_bytes((arena), sizeof(type)*(size)))

// NOTE: arena_realloc should be passed in the same type as the original allocation
void *arena_realloc_bytes(Arena *arena, void *old, size_t old_size, size_t new_size, size_t align);
#define arena_realloc(arena, type, old, old_size, new_size) \
    ((type *)arena_realloc_bytes((arena), (void *)(old), sizeof(type)*(old_size), sizeof(type)*(new_size), _Alignof(type)))

void *arena_memdup_bytes(Arena *arena, void *old, size_t size, size_t align);

#define arena_memdup(arena, type, old, size) \
    ((type *)arena_memdup_bytes((arena), (void *)(old), sizeof(type)*(size), _Alignof(type)))

#define arena_strdup(arena, old, size) \
    ((char *)arena_memdup_bytes((arena), (void *)(old), (size), _Alignof(char)))

#define arena_reset(arena) (arena_reset_ex((arena), false))
void arena_reset_ex(Arena *arena, bool clear_all);

void arena_free(Arena *arena);

ArenaCheckpoint arena_save(Arena *arena);
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


void *arena_push_bytes(Arena *a, size_t size, size_t align) {
    if (!a->tail) {
        size_t new_capacity = (size <= ARENA_DEFAULT_ZONE_CAP)? ARENA_DEFAULT_ZONE_CAP: size;
        a->tail = arena_new_zone(new_capacity, a->tail);
        a->head = a->tail;
    } else {
        size_t alignment = align_up_padding((uintptr_t)(a->tail->data + a->tail->length), align);
        size_t alloc_end = a->tail->length + alignment + size;
        if (alloc_end > a->tail->capacity) {
            size_t new_capacity = (alloc_end <= ARENA_DEFAULT_ZONE_CAP)? ARENA_DEFAULT_ZONE_CAP: alloc_end;
            a->tail->next = arena_new_zone(new_capacity, a->tail->next);
            a->tail = a->tail->next;
        }
    }

    size_t alignment = align_up_padding((uintptr_t)(a->tail->data + a->tail->length), align);
    void *mem = a->tail->data + a->tail->length + alignment;
    a->tail->length += alignment + size;
    return mem;
}


// Remove and return a pointer to the last `size` bytes of the current Zone
// WARNING: The returned pointer will be invalidated after a subsequent
// allocation to the arena
void *arena_pop_current_bytes(Arena *arena, size_t size) {
    if (!arena->tail) return NULL;
    arena->tail->length -= clamp_top(size, arena->tail->length);
    return arena->tail->data + arena->tail->length;
}


// Reallocates an already allocated piece of memory
// If `old` is NULL then it simply behaves like calling arena_push_bytes with `new_size`
void *arena_realloc_bytes(Arena *a, void *old, size_t old_size, size_t new_size, size_t align) {
    // allocate new arena if it doesnt already exist
    if (!a->tail || !old) return arena_push_bytes(a, new_size, align);
    if (new_size <= old_size) return old;

    // extend the old allocation if it was the last one
    if (old_size <= a->tail->length) {
        size_t old_offset = a->tail->length - old_size;
        if (a->tail->data + old_offset == old && old_offset + new_size <= a->tail->capacity) {
            a->tail->length += new_size - old_size;
            return old;
        }
    }

    return memcpy(arena_push_bytes(a, new_size, align), old, old_size);
}


// Copy `old` into the arena
// If `old` is NULL, then simply allocates `size` bytes in the arena
void *arena_memdup_bytes(Arena *arena, void *old, size_t size, size_t align) {
    if (!old) {
        return arena_push_bytes(arena, size, align);
    }
    return memcpy(arena_push_bytes(arena, size, align), old, size);
}

// Saves the current state of the arena
// This can be later used to rewind back to this point
ArenaCheckpoint arena_save(Arena *arena) {
    if (!arena->tail) {
        arena->tail = arena_new_zone(ARENA_DEFAULT_ZONE_CAP, arena->tail);
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
