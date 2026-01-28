#ifndef FILEPATH_H
#define FILEPATH_H

#include "arena.h"
#include "migi_string.h"
#include "migi_list.h"

static String path_dirname(String path);
static String path_basename(String path);
static String path_cannonicalize(Arena *arena, String path, String dir_sep);

static String path_basename(String path) {
    StrCut cut = str_cut_ex(path, S("/\\"), Cut_AsChars|Cut_Reverse);
    return cut.head;
}

static String path_dirname(String path) {
    StrCut cut = str_cut_ex(path, S("/\\"), Cut_AsChars|Cut_Reverse);
    return cut.found? cut.tail: S("/");
}

typedef struct StringNodeDll StringNodeDll;
struct StringNodeDll {
    String string;
    StringNodeDll *next, *prev;
};

static String path_cannonicalize(Arena *a, String path, String dir_sep) {
    Temp tmp = arena_temp_excl(a);

    StringNodeDll *head = NULL, *tail = NULL;
    strcut_foreach(path, dir_sep, 0, comp) {
        if (comp.split.length == 0 || str_eq(comp.split, S("."))) continue;

        if (str_eq(comp.split, S(".."))) {
            dll_pop_tail(head, tail);
            continue;
        }

        StringNodeDll *node = arena_new(tmp.arena, StringNodeDll);
        node->string = comp.split;
        dll_push_tail(head, tail, node);
    }

    String result = S("/");

    // Handling Windows drive separator
    // TODO: what will happen if a unix-like path has a ":\" in it (wtf?)
    String drive_sep = S(":\\");
    StrCut cut = str_cut(path, drive_sep);
    if (cut.found) {
        result = cut.head;
        result = str_cat(a, result, drive_sep);
    }

    list_foreach(head, StringNodeDll, comp) {
        result = str_cat(a, result, comp->string);
        result = str_cat(a, result, dir_sep);
    }
    arena_temp_release(tmp);

    if (result.length == 0) {
        result = S("/");
    }

    return result;
}

#endif // ifndef FILEPATH_H
