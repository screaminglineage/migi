#ifndef MIGI_LISTS_H
#define MIGI_LISTS_H

#include "migi_core.h"
#include "migi_string.h"
#include "arena.h"
#include <stddef.h>
#include <string.h>

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


// Single Linked List
// Insert node after `after` and returns the inserted `node`
#define sll_insert_after(tail, after, node) \
    (((node)->next = (after)->next),        \
    ((after)->next = (node)),               \
    ((after) == (tail))                     \
        ? ((tail) = (node)), (node)         \
        : (node))


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


// Insert node after `after` and returns the inserted `node`
#define dll_insert_after(head, tail, after, node) \
    (((node)->next = (after)->next),               \
    ((node)->prev = (after)),                     \
                                                  \
    (after)->next                                 \
        ? ((after)->next->prev = (node)), (void)0 \
        : (void)0,                                \
    ((after)->next = (node)),                     \
                                                  \
    ((after) == (tail))                           \
        ? ((tail) = (node)), (node)               \
        : (node))


// Insert node before `before` and returns the inserted `node`
#define dll_insert_before(head, tail, before, node) \
    (((node)->next = (before)),                      \
    ((node)->prev = (before)->prev),                \
                                                    \
    (before)->prev                                  \
        ? ((before)->prev->next = (node)), (void)0  \
        : (void)0,                                  \
    ((before)->prev = (node)),                      \
                                                    \
    ((before) == (head))                            \
        ? ((head) = (node)), (node)                 \
        : (node))



// Replace `node` with `node_new`
// NOTE: doesn't modify the next/prev pointers of `node`
#define dll_replace(head, tail, node, node_new)      \
    ((node_new)->next = (node)->next,                \
    (node_new)->prev = (node)->prev,                 \
    (node)->next                                     \
        ? ((node)->next->prev = (node_new)), (void)0 \
        : (void)0,                                   \
    (node)->prev                                     \
        ? ((node)->prev->next = (node_new)), (void)0 \
        : (void)0)


// Remove `node` from linked list
// NOTE: doesn't modify the next/prev pointers of `node`
#define dll_remove(head, tail, node)                   \
    ((node)->next                                      \
        ? ((node)->next->prev = (node)->prev), (void)0 \
        : (void)0,                                     \
    (node)->prev                                       \
        ? ((node)->prev->next = (node)->next), (void)0 \
        : (void)0,                                     \
    ((node) == (head))                                 \
        ? ((head) = (node)->next), (void)0             \
        : (void)0,                                     \
    ((node) == (tail))                                 \
        ? ((tail) = (node)->prev), (void)0             \
        : (void)0)


// Iterate over a linked list
#define list_foreach(head, type, item) \
    for (type *(item) = (head); (item); (item) = (item)->next)


// StringList (Linked List of Strings)
typedef struct StringNode StringNode;
struct StringNode {
    String string;
    StringNode *next;
};

typedef struct {
    StringNode *head, *tail;
    size_t length;
    size_t total_size;
} StringList;

static void strlist_push(Arena *a, StringList *list, String str);
static void strlist_push_char(Arena *a, StringList *list, char ch);
static void strlist_push_cstr(Arena *a, StringList *list, const char *cstr);
static void strlist_push_buffer(Arena *a, StringList *list, char *str, size_t length);
migi_printf_format(3, 4) static void strlist_pushf(Arena *a, StringList *list, const char *fmt, ...);
static void strlist_extend(StringList *list, StringList *extend_with);
static String strlist_pop(StringList *list);

#define strlist_foreach(strlist, node) list_foreach((strlist)->head, StringNode, (node))

static String strlist_to_string(Arena *a, StringList *list);
static String strlist_join(Arena *a, StringList *list, String join_with);

typedef enum {
    // Skip empty strings
    Split_SkipEmpty = (1 << 0),

    // Treat delimiter as a list of characters, where
    // splitting is done any time one of them appear
    Split_AsChars   = (1 << 1),
} SplitOpt;

static StringList string_split_ex(Arena *a, String str, String delimiter, SplitOpt flags);
static StringList strlist_split_ex(Arena *a, StringList *list, String delimiter, SplitOpt flags);

// Convenience macros with all flags set to 0
#define string_split(arena, str, delimiter)      string_split_ex((arena), (str), (delimiter), 0)
#define strlist_split(arena, strlist, delimiter) strlist_split_ex((arena), (strlist), (delimiter), 0)


// ArrayList (Chunked Linked List)

#define ARRAYLIST_DEFAULT_CAP 64
#define ARRAYLIST_ALIGN 8

#define ArrayList(type)  \
struct {                 \
    type *head;          \
    type *tail;          \
    size_t total_length; \
}

#define arrlist_init_capacity(arena, list, node_type, cap)              \
do {                                                                    \
    node_type *next = arena_new((arena), node_type);                    \
    next->data = arena_push_bytes((arena), sizeof(next->data[0])*(cap), \
                                 ARRAYLIST_ALIGN, true);                \
    next->capacity = (cap);                                             \
    queue_push((list)->head, (list)->tail, next);                       \
} while(0)


#define arrlist_add(arena, list, node_type, n)                       \
do {                                                                 \
    node_type *tail = (list)->tail;                                  \
                                                                     \
    if (!tail || tail->length >= tail->capacity) {                   \
        size_t capacity = ARRAYLIST_DEFAULT_CAP;                     \
        if (tail && tail->capacity != 0) capacity = tail->capacity;  \
                                                                     \
        arrlist_init_capacity((arena), (list), node_type, capacity); \
        tail = (list)->tail;                                         \
    }                                                                \
    tail->data[tail->length++] = n;                                  \
    (list)->total_length++;                                          \
} while (0)


static void strlist_push(Arena *a, StringList *list, String str) {
    StringNode *node = arena_new(a, StringNode);
    node->string = str;
    queue_push(list->head, list->tail, node);
    list->total_size += str.length;
    list->length += 1;
}

static void strlist_push_char(Arena *a, StringList *list, char ch) {
    char *data = arena_new(a, char);
    *data = ch;
    strlist_push(a, list, (String){data, 1});
}

static void strlist_push_cstr(Arena *a, StringList *list, const char *cstr) {
    strlist_push(a, list, string_from_cstr(cstr));
}

static void strlist_push_buffer(Arena *a, StringList *list, char *str, size_t length) {
    char *data = arena_copy(a, char, str, length);
    strlist_push(a, list, (String){data, length});
}

// NOTE: strlist_pushf doesnt append a null terminator at the end
// of the format string unlike regular sprintf
static void strlist_pushf(Arena *a, StringList *list, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    String string = string__format(a, fmt, args);
    va_end(args);
    strlist_push(a, list, string);
}

static String strlist_to_string(Arena *a, StringList *list) {
    char *mem = arena_push_nonzero(a, char, list->total_size);
    char *dest = mem;
    for (StringNode *node = list->head; node != NULL; node = node->next) {
        memcpy(dest, node->string.data, node->string.length);
        dest += node->string.length;
    }
    return (String){mem, list->total_size};
}

static void strlist_extend(StringList *list, StringList *extend_with) {
    // Update the head as well for an empty StringList
    if (list->length == 0) {
        list->head = extend_with->head;
    } else {
        list->tail->next = extend_with->head;
    }
    list->tail = extend_with->tail;

    list->length += extend_with->length;
    list->total_size += extend_with->total_size;
}

static String strlist_pop(StringList *list) {
    if (!list->head) return (String){0};

    String popped = list->head->string;
    queue_pop(list->head, list->tail);
    list->length -= 1;
    list->total_size -= popped.length;
    return popped;
}

static String strlist_join(Arena *a, StringList *list, String join_with) {
    size_t total_size = list->total_size + (list->length - 1) * join_with.length;
    char *mem = arena_push_nonzero(a, char, total_size);

    char *dest = mem;
    StringNode *node = list->head;
    for (; node->next; node = node->next) {
        memcpy(dest, node->string.data, node->string.length);
        dest += node->string.length;

        memcpy(dest, join_with.data, join_with.length);
        dest += join_with.length;
    }
    memcpy(dest, node->string.data, node->string.length);
    return (String){mem, total_size};
}


// Splits a string by delimiter, pushing each chunk onto a StringList
static StringList string_split_ex(Arena *a, String str, String delimiter, SplitOpt flags) {
    StringList strings = {0};
    if (delimiter.length == 0) return strings;

    StringCutOpt cut_flags = (flags & Split_AsChars)? Cut_AsChars: 0;
    strcut_foreach(str, delimiter, cut_flags, cut) {
        if (cut.split.length != 0 || !(flags & Split_SkipEmpty)) {
            strlist_push(a, &strings, cut.split);
        }
    }
    return strings;
}

static StringList strlist_split_ex(Arena *a, StringList *list, String delimiter, SplitOpt flags) {
    StringList strings = {0};
    if (delimiter.length == 0) return strings;

    strlist_foreach(list, node) {
        StringList splits = string_split_ex(a, node->string, delimiter, flags);
        strlist_extend(&strings, &splits);
    }
    return strings;
}


#endif // MIGI_LISTS_H
