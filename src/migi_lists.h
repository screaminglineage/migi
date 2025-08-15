#ifndef MIGI_LISTS_H
#define MIGI_LISTS_H

#include "migi.h"
#include "migi_string.h"
#include "arena.h"
#include <stddef.h>
#include <string.h>

// TODO: add basic stack and queue macros
// TODO: add strlist_pushf

typedef struct StringNode StringNode;
struct StringNode {
    String string;
    StringNode *next;
};

typedef struct {
    StringNode *head;
    StringNode *tail;
    size_t size;
} StringList;

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
    char *data = arena_strdup(a, str, length);
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
        arena_pop_current(a, char, reserved);
        mem = arena_push(a, char, actual);
        vsnprintf(mem, actual, fmt, args2);
    } else if (actual < reserved) {
        arena_pop_current(a, char, abs_difference(actual, reserved));
    }
    // pop off the null terminator
    arena_pop_current(a, char, 1);

    strlist_push_buffer(a, list, mem, actual);
    va_end(args2);
    va_end(args1);
}

static String strlist_to_string(Arena *a, StringList *list) {
    char *mem = arena_push(a, char, list->size);
    char *mem_start = mem;
    for (StringNode *node = list->head; node != NULL; node = node->next) {
        memcpy(mem, node->string.data, node->string.length);
        mem += node->string.length;
    }
    return (String){mem_start, list->size};
}


#define SPLIT_SKIP_EMPTY 0x1

// TODO: the string_split functions below dont move the actual string data onto the arena
// However the caller may expect that to be the case since an arena parameter is passed in
// More experimentation is needed before it can be said for sure though
static StringList string_split_ex(Arena *a, String str, String delimiter, int flags) {
    StringList strings = {0};
    if (delimiter.length == 0) return strings;

    int64_t index = string_find(str, delimiter);
    while (index != -1 && str.length > 0) {
        String substr = string_slice(str, 0, index);
        if (!(flags & SPLIT_SKIP_EMPTY) || substr.length != 0) {
            strlist_push_string(a, &strings, substr);
        }
        str = string_skip(str, index + delimiter.length);
        index = string_find(str, delimiter);
    }

    // Only include empty strings if the flag is set
    if (str.length != 0 || (str.length == 0 && !(flags & SPLIT_SKIP_EMPTY))) {
        strlist_push_string(a, &strings, str);
    }
    return strings;
}

static StringList string_split_chars_ex(Arena *a, String str, String delims, int flags) {
    StringList strings = {0};
    size_t start = 0;
    size_t length = 0;
    for (size_t i = 0; i < str.length; i++) {
        bool delim_found = false;
        for (size_t j = 0; j < delims.length; j++) {
            if (delims.data[j] == str.data[i]) {
                String substr = (String){
                    .data = str.data + start,
                    .length = length
                };
                if (!(flags & SPLIT_SKIP_EMPTY) || substr.length != 0) {
                    strlist_push_string(a, &strings, substr);
                }
                length = 0;
                start = i + 1;
                delim_found = true;
                break;
            }
        }
        if (!delim_found) length++;
    }
    String remaining_part = (String){
        .data = str.data + start,
        .length = length
    };
    if (!(flags & SPLIT_SKIP_EMPTY) || remaining_part.length != 0) {
        strlist_push_string(a, &strings, remaining_part);
    }
    return strings;
}

// Convenience macros which set flags to 0
#define string_split(arena, str, delimiter) \
    (string_split_ex((arena), (str), (delimiter), 0))

#define string_split_chars(arena, str, delims) \
    (string_split_chars_ex((arena), (str), (delims), 0))



#endif // MIGI_LISTS_H
