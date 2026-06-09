#ifndef MIGI_FILEPATH_H
#define MIGI_FILEPATH_H

#include "arena.h"
#include "migi_string.h"
#include "migi_list.h"

static Str path_dirname(Str path);
static Str path_basename(Str path);
static Str path_push(Arena *a, Str path, Str dir_sep, Str new_elem);
static Str path_cannonicalize(Arena *arena, Str path, Str dir_sep);

static Str path_basename(Str path) {
    StrCut cut = str_cut_ex(path, S("/\\"), Cut_Any|Cut_Reverse);
    return cut.head;
}

static Str path_dirname(Str path) {
    StrCut cut = str_cut_ex(path, S("/\\"), Cut_Any|Cut_Reverse);
    return cut.found? cut.tail: S("/");
}

static Str path_push(Arena *a, Str path, Str dir_sep, Str new_elem) {
    path = str_cat(a, path, dir_sep);
    return str_cat(a, path, new_elem);
}

typedef struct StrNodeDll StrNodeDll;
struct StrNodeDll {
    Str string;
    StrNodeDll *next, *prev;
};

static Str path_cannonicalize(Arena *a, Str path, Str dir_sep) {
    if (path.length == 0) return path;

    Str header = {0};        // Windows drive letter, or `file:`, `http:`, etc.
    Str drive_sep = S(":");  // Windows drive seperator, should also work for URLs
    for (size_t i = 0; i < path.length; i++) {
        if (path.data[i] == dir_sep.data[0]) break;

        if (path.data[i] == drive_sep.data[0]) {
            header = str_take(path, i + drive_sep.length);
            path = str_skip(path, i + drive_sep.length);
            break;
        }
    }

    bool trailing_slash = str_ends_with(path, dir_sep);

    int leading_slashes = 0;
    while (path.length > 0 && path.data[0] == dir_sep.data[0]) {
        leading_slashes += 1;
        path = str_skip(path, 1);
    }

    Temp tmp = arena_temp_excl(a);

    StrNodeDll *head = NULL, *tail = NULL;
    strcut_foreach(path, dir_sep, 0, comp) {
        if (comp.split.length == 0 || str_eq(comp.split, S("."))) continue;

        if (str_eq(comp.split, S(".."))) {
            dll_pop_tail(head, tail);
            continue;
        }

        StrNodeDll *node = arena_new(tmp.arena, StrNodeDll);
        node->string = comp.split;
        dll_push_tail(head, tail, node);
    }

    Str result = header;
    for (int i = 0; i < leading_slashes; i++) {
        result = str_cat(a, result, dir_sep);
    }

    list_foreach(head, comp) {
        result = str_cat(a, result, comp->string);
        if (comp->next || trailing_slash) {
            result = str_cat(a, result, dir_sep);
        }
    }
    arena_temp_release(tmp);

    if (result.length == 0) {
        result = S("/");
    }

    return result;
}

#endif // ifndef MIGI_FILEPATH_H
