#ifndef MIGI_TEMP_ALLOC_H
#define MIGI_TEMP_ALLOC_H

#include "migi.h"
#include <stdarg.h>

#ifndef ARENA_DEFAULT_ZONE_CAP
    #define ARENA_DEFAULT_ZONE_CAP 4*KB
#endif
#include "arena.h"
#include "migi_string.h"

// TODO: maybe make this thread local
static Arena *temp_allocator_global_arena;

#define temp_alloc(type, size) \
    ((type *)arena_push_bytes(temp_allocator_global_arena, sizeof(type) * size, _Alignof(type), true))

migi_printf_format(1, 2) static String temp_format(const char *fmt, ...);
static inline void temp_init();
static void temp_reset();
static Checkpoint temp_save();
static void temp_rewind(Checkpoint checkpoint);

static inline void temp_init() {
    temp_allocator_global_arena = arena_init();
}

static String temp_format(const char *fmt, ...) {
    va_list args1;
    va_start(args1, fmt);

    va_list args2;
    va_copy(args2, args1);

    int reserved = 1024;
    char *mem = arena_push(temp_allocator_global_arena, char, reserved);
    int actual = vsnprintf(mem, reserved, fmt, args1);
    // vsnprintf doesnt count the null terminator
    actual += 1;

    if (actual > reserved) {
        arena_pop(temp_allocator_global_arena, char, reserved);
        mem = arena_push(temp_allocator_global_arena, char, actual);
        vsnprintf(mem, actual, fmt, args2);
    } else if (actual < reserved) {
        arena_pop(temp_allocator_global_arena, char, abs_difference(actual, reserved));
    }
    // pop off the null terminator
    arena_pop(temp_allocator_global_arena, char, 1);

    va_end(args2);
    va_end(args1);

    // actual includes the null terminator
    return (String){ .data = mem, .length = actual - 1 };
}

static void temp_reset() {
    arena_reset(temp_allocator_global_arena);
}

static Checkpoint temp_save() {
    return arena_save(temp_allocator_global_arena);
}

static void temp_rewind(Checkpoint checkpoint) {
    arena_rewind(checkpoint);
}


#endif // MIGI_TEMP_ALLOC_H
