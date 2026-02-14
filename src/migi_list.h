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


// Singly Linked List
// Insert node after `after` and returns the inserted `node`
// NOTE: after shouldnt be NULL
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


// StrList (Linked List of Strings)
typedef struct StrNode StrNode;
struct StrNode {
    Str string;
    StrNode *next;
};

typedef struct {
    StrNode *head, *tail;
    size_t length;
    size_t total_size;
} StrList;

static StrList strlist_from_str(Arena *a, Str str);

static void strlist_push(Arena *a, StrList *list, Str str);
static void strlist_push_char(Arena *a, StrList *list, char ch);
static void strlist_push_cstr(Arena *a, StrList *list, const char *cstr);
static void strlist_push_buffer(Arena *a, StrList *list, char *str, size_t length);
migi_printf_format(3, 4) static void strlist_pushf(Arena *a, StrList *list, const char *fmt, ...);
static void strlist_extend(StrList *list, StrList *extend_with);
static Str strlist_pop(StrList *list);

#define strlist_foreach(strlist, node) list_foreach((strlist)->head, StrNode, (node))

static Str strlist_to_string(Arena *a, StrList *list);
static Str strlist_join(Arena *a, StrList *list, Str join_with);
static void strlist_replace(Arena *a, StrList *list, Str find, Str replace_with);

typedef enum {
    // Skip empty strings
    Split_SkipEmpty = (1 << 0),

    // Treat delimiter as a list of characters, where
    // splitting is done any time one of them appear
    Split_AsChars   = (1 << 1),
} SplitOpt;

// Splits a string by delimiter, pushing each chunk onto a StrList
static StrList str_split_ex(Arena *a, Str str, Str delimiter, SplitOpt flags);
#define str_split(arena, str, delim) str_split_ex((arena), (str), (delim), 0)

static StrList strlist_split_ex(Arena *a, StrList *list, Str delimiter, SplitOpt flags);
#define strlist_split(arena, strlist, delim) strlist_split_ex((arena), (strlist), (delim), 0)


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


static StrList strlist_from_str(Arena *a, Str str) {
    StrList list = {0};
    strlist_push(a, &list, str);
    return list;
}

static void strlist_push(Arena *a, StrList *list, Str str) {
    StrNode *node = arena_new(a, StrNode);
    node->string = str;
    queue_push(list->head, list->tail, node);
    list->total_size += str.length;
    list->length += 1;
}

static void strlist_push_char(Arena *a, StrList *list, char ch) {
    char *data = arena_new(a, char);
    *data = ch;
    strlist_push(a, list, (Str){data, 1});
}

static void strlist_push_cstr(Arena *a, StrList *list, const char *cstr) {
    strlist_push(a, list, str_from_cstr(cstr));
}

static void strlist_push_buffer(Arena *a, StrList *list, char *str, size_t length) {
    char *data = arena_copy(a, char, str, length);
    strlist_push(a, list, (Str){data, length});
}

// NOTE: strlist_pushf doesnt append a null terminator at the end
// of the format string unlike regular sprintf
static void strlist_pushf(Arena *a, StrList *list, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    Str string = str__format(a, fmt, args);
    va_end(args);
    strlist_push(a, list, string);
}

static Str strlist_to_string(Arena *a, StrList *list) {
    char *mem = arena_push_nonzero(a, char, list->total_size);
    char *dest = mem;
    for (StrNode *node = list->head; node != NULL; node = node->next) {
        memcpy(dest, node->string.data, node->string.length);
        dest += node->string.length;
    }
    return (Str){mem, list->total_size};
}

static void strlist_extend(StrList *list, StrList *extend_with) {
    // Update the head as well for an empty StrList
    if (list->length == 0) {
        list->head = extend_with->head;
    } else {
        list->tail->next = extend_with->head;
    }
    list->tail = extend_with->tail;

    list->length += extend_with->length;
    list->total_size += extend_with->total_size;
}

static Str strlist_pop(StrList *list) {
    if (!list->head) return (Str){0};

    Str popped = list->head->string;
    queue_pop(list->head, list->tail);
    list->length -= 1;
    list->total_size -= popped.length;
    return popped;
}

static Str strlist_join(Arena *a, StrList *list, Str join_with) {
    size_t total_size = list->total_size + (list->length - 1) * join_with.length;
    char *mem = arena_push_nonzero(a, char, total_size);

    char *dest = mem;
    StrNode *node = list->head;
    for (; node->next; node = node->next) {
        memcpy(dest, node->string.data, node->string.length);
        dest += node->string.length;

        memcpy(dest, join_with.data, join_with.length);
        dest += join_with.length;
    }
    memcpy(dest, node->string.data, node->string.length);
    return (Str){mem, total_size};
}


static StrList str_split_ex(Arena *a, Str str, Str delimiter, SplitOpt flags) {
    StrList strings = {0};
    if (delimiter.length == 0) {
        for (size_t i = 0; i < str.length; i++) {
            strlist_push(a, &strings, (Str){.data = &str.data[i], .length = 1});
        }
        return strings;
    }

    StrCutOpt cut_flags = (flags & Split_AsChars)? Cut_AsChars: 0;
    strcut_foreach(str, delimiter, cut_flags, cut) {
        if (cut.split.length != 0 || !(flags & Split_SkipEmpty)) {
            strlist_push(a, &strings, cut.split);
        }
    }
    return strings;
}

// TODO: implement special parsing for empty delimiter like `str_split`
static StrList strlist_split_ex(Arena *a, StrList *list, Str delimiter, SplitOpt flags) {
    StrList strings = {0};
    if (delimiter.length == 0) return strings;

    strlist_foreach(list, node) {
        StrList splits = str_split_ex(a, node->string, delimiter, flags);
        strlist_extend(&strings, &splits);
    }
    return strings;
}

static void strlist_replace(Arena *a, StrList *list, Str find, Str replace_with) {
    size_t total_size = 0;
    size_t length = 0;
    StrNode *prev_node = NULL;

    StrNode *node = list->head;
    while (node) {
        Str string = node->string;
        StrNode *node_next = node->next;

        StrNode *head = NULL;
        StrNode *tail = NULL;
        size_t find_index = str_find(string, find);
        if (find_index == string.length) {
            total_size += string.length;
            length += 1;
            prev_node = node;
            node = node->next;
            continue;
        }

        bool first = true;
        do {
            Str before = str_take(string, find_index);
            string = str_skip(string, find_index + find.length);
            find_index = str_find(string, find);

            if (before.length != 0) {
                StrNode *before_node = arena_new(a, StrNode);
                before_node->string = before;
                queue_push(head, tail, before_node);
                total_size += before.length;
                length += 1;
            }

            // reuse the current node the first time around
            StrNode *replace_node = first? node: arena_new(a, StrNode);
            replace_node->string = replace_with;
            queue_push(head, tail, replace_node);
            total_size += replace_with.length;
            length += 1;

            first = false;
        } while(find_index < string.length);

        if (string.length > 0) {
            StrNode *end = arena_new(a, StrNode);
            end->string = string;
            queue_push(head, tail, end);
            total_size += string.length;
            length += 1;
        }

        if (prev_node) {
            prev_node->next = head;
        } else {
            list->head = head;
        }
        tail->next = node_next;
        prev_node = tail;
        node = node_next;
    }
    list->total_size = total_size;
    list->length = length;
}

#endif // MIGI_LISTS_H
