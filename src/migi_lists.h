#ifndef MIGI_LISTS_H
#define MIGI_LISTS_H

#include "migi.h"
#include "migi_string.h"
#include "arena.h"
#include <stddef.h>
#include <string.h>

// TODO: forward declare all functions

// Singly Linked Stack
#define stack_push(head, node)     \
    ((head)                        \
        ? ((node)->next = (head)), \
          ((head) = (node))        \
        : ((head) = (node)))

#define stack_pop(head)         \
    ((head)                     \
        ? (head) = (head)->next \
        : (head))

// Singly Linked Queue
#define queue_push(head, tail, node) \
    ((tail)                          \
        ? ((tail)->next = (node)),   \
          ((tail) = (node))          \
        : ((tail) = (head) = (node)))

#define queue_pop(head, tail)     \
    (((head) != (tail))           \
        ? ((head) = (head)->next) \
        : ((head) = (tail) = NULL))


// Doubly Linked Deque
#define dll_push_head(head, tail, node) \
    ((head)                             \
        ? ((node)->next = (head)),      \
          ((head)->prev = (node)),      \
          ((head) = (node))             \
        : ((head) = (tail) = (node)))

#define dll_push_tail(head, tail, node) \
    ((tail)                             \
        ? ((node)->prev = (tail)),      \
          ((tail)->next = (node)),      \
          ((tail) = (node))             \
        : ((tail) = (head) = (node)))

#define dll_pop_head(head, tail)  \
    (((head) != (tail))           \
        ? ((head) = (head)->next) \
        : ((head) = (tail) = NULL))

#define dll_pop_tail(head, tail)  \
    (((head) != (tail))           \
        ? ((tail) = (tail)->prev) \
        : ((head) = (tail) = NULL))


// Insert node after `after`, inserts after tail if `after` is NULL
// Returns the inserted `node`
#define dll_insert_after(head, tail, after, node)     \
    ((after)                                          \
        ? ((node)->next = (after)->next),             \
          ((node)->prev = (after)),                   \
                                                      \
          (after)->next                               \
            ? ((after)->next->prev = (node)), (void)0 \
            : (void)0,                                \
          ((after)->next = (node)),                   \
                                                      \
          ((after) == (tail))                         \
            ? ((tail) = (node)), (node)               \
            : (node)                                  \
        : dll_push_tail((head), (tail), (node)))


// Insert node before `before`, inserts at head if `before` is NULL
// Returns the inserted `node`
#define dll_insert_before(head, tail, before, node)    \
    ((before)                                          \
        ? ((node)->next = (before)),                   \
          ((node)->prev = (before)->prev),             \
                                                       \
          (before)->prev                               \
            ? ((before)->prev->next = (node)), (void)0 \
            : (void)0,                                 \
          ((before)->prev = (node)),                   \
                                                       \
          ((before) == (head))                         \
            ? ((head) = (node)), (node)                \
            : (node)                                   \
        : dll_push_head((head), (tail), (node)))


// Replace `node` with `node_new`
// NOTE: doesn't modify the next/prev pointers of `node`
#define dll_replace(head, tail, node, node_new)      \
    (node_new)->next = (node)->next,                 \
    (node_new)->prev = (node)->prev,                 \
    (node)->next                                     \
        ? ((node)->next->prev = (node_new)), (void)0 \
        : (void)0,                                   \
    (node)->prev                                     \
        ? ((node)->prev->next = (node_new)), (void)0 \
        : (void)0


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
    queue_push(list->head, list->tail, node);
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
    va_list args;
    va_start(args, fmt);
    String string = string__format(a, fmt, args);
    va_end(args);
    strlist_push_string(a, list, string);
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
