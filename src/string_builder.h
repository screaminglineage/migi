#ifndef MIGI_STRING_BUILDER_H
#define MIGI_STRING_BUILDER_H

#include "migi.h"
#include "migi_string.h"
#include "arena.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

typedef struct {
    Arena *arena;
} StringBuilder;

migi_printf_format(2, 3) static void sb_pushf(StringBuilder *sb, const char *fmt, ...);

static StringBuilder sb_init() {
    Arena *a = arena_init(.type = Arena_Linear);
    return (StringBuilder){
        .arena = a
    };
}

static StringBuilder sb_init_static(char *buf, size_t buf_size) {
    Arena *a = arena_init_static(buf, buf_size);
    return (StringBuilder){
        .arena = a
    };
}

static size_t sb_length(StringBuilder *sb) {
    return sb->arena->position - sizeof(Arena);
}

static void sb_push(StringBuilder *sb, char to_push) {
    *arena_push(sb->arena, char, 1) = to_push;
}

static void sb_push_string(StringBuilder *sb, String string) {
    memcpy(arena_push(sb->arena, char, string.length), string.data, string.length);
}

static void sb_push_cstr(StringBuilder *sb, char *cstr) {
    size_t length = strlen(cstr);
    arena_copy(sb->arena, char, cstr, length);
}

static void sb_push_buffer(StringBuilder *sb, char *buf, size_t length) {
    arena_copy(sb->arena, char, buf, length);
}

// NOTE: sb_pushf doesnt append a null terminator at the end
// of the format string unlike regular sprintf
static void sb_pushf(StringBuilder *sb, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    string__format(sb->arena, fmt, args);
    va_end(args);
}

static void sb_push_file(StringBuilder *sb, String filename) {
    string_from_file(sb->arena, filename);
}

void sb_reset(StringBuilder *sb) {
    arena_reset(sb->arena);
}

void sb_free(StringBuilder *sb) {
    arena_free(sb->arena);
}

static StringBuilder sb_from_string(String string) {
    StringBuilder sb = {0};
    sb_push_string(&sb, string);
    return sb;
}

static String sb_to_string(StringBuilder *sb) {
    return (String){
        .data = (char *)sb->arena->data,
        .length = sb_length(sb)
    };
}

// NOTE: The cstring returned from this function is not separately allocated
// and is destroyed after a subsequent push onto the string builder
static const char *sb_to_cstr(StringBuilder *sb) {
    sb_push(sb, 0);
    const char *cstr = (const char *)sb->arena->data;
    arena_pop(sb->arena, char, 1);
    return cstr;
}

static bool sb_to_file(StringBuilder *sb, String filename) {
    return string_to_file(sb_to_string(sb), filename);
}

#endif // MIGI_STRING_BUILDER_H
