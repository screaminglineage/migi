#ifndef MIGI_LISTS_H
#define MIGI_LISTS_H

#include "migi.h"
#include "migi_string.h"
#include "arena.h"
#include <stddef.h>
#include <string.h>

// TODO: add basic stack and queue macros
// TODO: forward declare all functions

typedef struct StringNode StringNode;
struct StringNode {
    String string;
    StringNode *next;
};

typedef struct {
    StringNode *head;
    StringNode *tail;
    size_t size;
    size_t length;
} StringList;

migi_printf_format(3, 4) static void strlist_pushf(Arena *a, StringList *list, const char *fmt, ...);

static void strlist_push_string(Arena *a, StringList *list, String str) {
    StringNode *node = arena_new(a, StringNode);
    node->string = str;
    node->next = NULL;
    if (!list->tail) {
        list->head = node;
        list->tail = node;
    } else {
        list->tail->next = node;
        list->tail = node;
    }
    list->size += str.length;
    list->length += 1;
}

static void strlist_push(Arena *a, StringList *list, char ch) {
    char *data = arena_new(a, char);
    *data = ch;
    strlist_push_string(a, list, (String){data, 1});
}

static void strlist_push_cstr(Arena *a, StringList *list, char *cstr) {
    strlist_push_string(a, list, string_from_cstr(cstr));
}

static void strlist_push_buffer(Arena *a, StringList *list, char *str, size_t length) {
    char *data = arena_copy(a, char, str, length);
    strlist_push_string(a, list, (String){data, length});
}

// NOTE: strlist_pushf doesnt append a null terminator at the end
// of the format string unlike regular sprintf
static void strlist_pushf(Arena *a, StringList *list, const char *fmt, ...) {
    va_list args1;
    va_start(args1, fmt);

    va_list args2;
    va_copy(args2, args1);

    int reserved = 1024;
    char *mem = arena_push(a, char, reserved);
    int actual = vsnprintf(mem, reserved, fmt, args1);
    // vsnprintf doesnt count the null terminator
    actual += 1;

    if (actual > reserved) {
        arena_pop(a, char, reserved);
        mem = arena_push(a, char, actual);
        vsnprintf(mem, actual, fmt, args2);
    } else if (actual < reserved) {
        arena_pop(a, char, abs_difference(actual, reserved));
    }
    // pop off the null terminator
    arena_pop(a, char, 1);

    // actual includes the null terminator
    strlist_push_buffer(a, list, mem, actual - 1);
    va_end(args2);
    va_end(args1);
}

static String strlist_to_string(Arena *a, StringList *list) {
    // TODO: no need to clear `mem` as it will be overwritten anyway
    char *mem = arena_push(a, char, list->size);
    char *mem_start = mem;
    for (StringNode *node = list->head; node != NULL; node = node->next) {
        memcpy(mem, node->string.data, node->string.length);
        mem += node->string.length;
    }
    return (String){mem_start, list->size};
}

static String strlist_join(Arena *a, StringList *list, String join_with) {
    size_t total_size = list->size + (list->length - 1) * join_with.length;
    // TODO: no need to clear `mem` as it will be overwritten anyway
    char *mem = arena_push(a, char, total_size);
    char *mem_start = mem;

    StringNode *node = list->head;
    while (node->next != NULL) {
        memcpy(mem, node->string.data, node->string.length);
        mem += node->string.length;

        memcpy(mem, join_with.data, join_with.length);
        mem += join_with.length;

        node = node->next;
    }
    memcpy(mem, node->string.data, node->string.length);
    return (String){mem_start, total_size};
}

typedef enum {
    // Skip empty strings
    Split_SkipEmpty = 1 << 0,

    // Treat delimiter as a list of characters, where
    // splitting is done any time one of them appear
    Split_AsChars   = 1 << 1,
} SplitOpt;

// Splits a string by delimiter, pushing each chunk onto a StringList
static StringList string_split_ex(Arena *a, String str, String delimiter, SplitOpt flags) {
    StringList strings = {0};
    if (delimiter.length == 0) return strings;

    SplitIterator next = {0};
    while (!next.is_over) {
        next = (flags & Split_AsChars)
            ? string_split_chars_next(&str, delimiter)
            : string_split_next(&str, delimiter);

        if (next.string.length != 0 || !(flags & Split_SkipEmpty)) {
            strlist_push_string(a, &strings, next.string);
        }
    }
    return strings;
}

// Convenience macro with some flags set to 0
#define string_split(arena, str, delimiter) \
    (string_split_ex((arena), (str), (delimiter), 0))




#endif // MIGI_LISTS_H
