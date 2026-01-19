#define _CRT_SECURE_NO_WARNINGS

#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <windows.h>

#include "migi_string.h"
#include "arena.h"
#include "migi_list.h"
#include "dynamic_string.h"

typedef struct FindHandles FindHandles;
struct FindHandles {
    FindHandles *next;

    // WIN32 specific
    HANDLE find;
};

typedef enum {
    DirWalkerMode_Recurse,
    DirWalkerMode_PopStack,
    DirWalkerMode_NextFile,
    DirWalkerMode_Return,
} DirWalkerMode;

typedef struct {
    FindHandles *find_handles;
    String current_dir;
    DString temp_str;
    uint32_t depth;
    uint32_t max_depth;         // -1 for no limit
    DirWalkerMode mode;
    DirWalkerMode next_mode;

    // WIN32 specific
    HANDLE find;
    WIN32_FIND_DATA file_data;
} DirWalker;


// TODO: convert the booleans into bit-flags
typedef struct {
    String path;
    String name;
    size_t size;          // 0 for directories
    size_t last_modified; // in unix epoch
    bool is_dir;
    bool is_hidden;
    bool over;
    bool error;
} DirEntry;


// TODO: also prepend "\\?\" to the path to extend the file path size from MAX_PATH
// https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-findfirstfilea
// TODO: come up with a better name
// Search with wildcard appended to end of file
static HANDLE find_first_file(String dir, WIN32_FIND_DATA *file_data, DString *temp_str) {
    HANDLE find = INVALID_HANDLE_VALUE;
    temp_str->string.length = 0;
    dstring_pushf(temp_str, "%.*s\\*", SV_FMT(dir));
    find = FindFirstFile(dstring_to_temp_cstr(temp_str), file_data);
    return find;
}

// TODO: maybe return a pointer to an opaque struct allocated on the arena
// DirWalker is a bit too big (392 bytes) to be allocated on the stack anyway 
static DirWalker walker_init(String filepath, uint32_t max_depth) {
    DirWalker walker = {0};

    if (filepath.length == 0) {
        fprintf(stderr, "error: directory path cannot be empty\n");
        return walker;
    }

    walker.current_dir = filepath;
    walker.mode = DirWalkerMode_Recurse;
    walker.max_depth = max_depth;

    return walker;
}

// Taken from https://stackoverflow.com/a/46024468
static int64_t win32__system_time_to_unix(FILETIME ft) {
   int64_t unix_time_start  = 0x019DB1DED53E8000; // January 1, 1970 (start of Unix epoch) in "ticks"
   int64_t ticks_per_second = 10000000;           // a tick is 100ns

   LARGE_INTEGER li = {
       .LowPart  = ft.dwLowDateTime,
       .HighPart = ft.dwHighDateTime
   };
   return (li.QuadPart - unix_time_start) / ticks_per_second;
}



// TODO: come up with a better name since it also updates the mode of the walker itself rather than simply filling the entry
static void walker_fill_entry(Arena *arena, DirWalker *w, DirEntry *entry, uint32_t max_depth) {
    if (w->file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        // skip `.` and `..`
        if (strcmp(w->file_data.cFileName, ".") == 0 || strcmp(w->file_data.cFileName, "..") == 0) {
            w->mode = DirWalkerMode_NextFile;
            return;
        }

        FindHandles *find_handle = arena_new(arena, FindHandles);
        find_handle->find = w->find;
        // frame->dir_name = w->current_dir;
        stack_push(w->find_handles, find_handle);
        w->depth += 1;

        w->current_dir = stringf(arena, "%.*s\\%s", SV_FMT(w->current_dir), w->file_data.cFileName);
        entry->name = string_from_cstr(w->file_data.cFileName);
        entry->path = w->current_dir;
        entry->is_dir = true;
        if (w->depth < max_depth) {
            w->next_mode = DirWalkerMode_Recurse;
        } else {
            w->next_mode = DirWalkerMode_PopStack;
        }
    } else {
        LARGE_INTEGER filesize = {
            .LowPart = w->file_data.nFileSizeLow,
            .HighPart = w->file_data.nFileSizeHigh
        };
        entry->size = filesize.QuadPart;
        dstring_pushf(&w->temp_str, "%.*s\\%s", SV_FMT(w->current_dir), w->file_data.cFileName);
        entry->path = w->temp_str.string;
        entry->name = string_from_cstr(w->file_data.cFileName);
        w->next_mode = DirWalkerMode_NextFile;
    }

    entry->last_modified = win32__system_time_to_unix(w->file_data.ftLastWriteTime);
    entry->is_hidden = w->file_data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN;
    w->mode = DirWalkerMode_Return;
}


typedef struct {
    bool dont_recurse;
} WalkerOptions;


// TODO: on errors, return the error somehow and do not stop entirely
static DirEntry walker__next(Arena *arena, DirWalker *w, WalkerOptions opt) {
    DirEntry entry = {0};

    while (true) {
        w->temp_str.string.length = 0;

        switch (w->mode) {
            case DirWalkerMode_Recurse: {
                if (opt.dont_recurse) {
                    w->mode = DirWalkerMode_PopStack;
                } else {
                    w->find = find_first_file(w->current_dir, &w->file_data, &w->temp_str);
                    if (w->find == INVALID_HANDLE_VALUE) {
                        // DisplayErrorBox(TEXT("FindFirstFile"));
                        // TODO: print the error here
                        // https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-formatmessage
                        printf("Error: %ld\n", GetLastError());
                        FindClose(w->find);
                        entry.over = true;
                        entry.error = true;
                        w->next_mode = DirWalkerMode_NextFile;
                        w->mode = DirWalkerMode_Return;
                    } else {
                        walker_fill_entry(arena, w, &entry, w->max_depth);
                    }
                }
            } break;

            case DirWalkerMode_PopStack: {
                if (w->find_handles == NULL) {
                    entry.over = true;
                    w->next_mode = DirWalkerMode_NextFile;
                    w->mode = DirWalkerMode_Return;
                } else {
                    w->find = w->find_handles->find;

                    // Remove the last directory from current_dir
                    int64_t end = string_find_char_rev(w->current_dir, '\\');
                    assert(end != -1);
                    w->current_dir = string_slice(w->current_dir, 0, end);

                    stack_pop(w->find_handles);
                    w->depth -= 1;
                    w->mode = DirWalkerMode_NextFile;
                }
            } break;

            case DirWalkerMode_NextFile: {
                BOOL ok = FindNextFile(w->find, &w->file_data);
                if (!ok) {
                    DWORD error = GetLastError();
                    if (error == ERROR_NO_MORE_FILES) {
                        w->mode = DirWalkerMode_PopStack;
                    } else {
                        // DisplayErrorBox(TEXT("FindFirstFile"));
                        // TODO: print the error here
                        // https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-formatmessage
                        printf("Error: %ld\n", error);

                        // TODO: close all the handles in the stack here
                        entry.over = true;
                        entry.error = true;
                        w->next_mode = DirWalkerMode_NextFile;
                        w->mode = DirWalkerMode_Return;
                    }
                    FindClose(w->find);
                } else {
                    walker_fill_entry(arena, w, &entry, w->max_depth);
                }
            } break;

            case DirWalkerMode_Return: {
                w->mode = w->next_mode;
                return entry;
            } break;

            default: migi_unreachable();
        }
    }
}

#define walker_next(arena, walker, ...) \
    walker__next((arena), (walker), (WalkerOptions){ __VA_ARGS__ })


#define dir_foreach(arena, walker, entry)                                      \
    for (DirEntry entry = walker__next((arena), (walker), (WalkerOptions){0}); \
         !entry.over;                                                          \
         entry = walker__next((arena), (walker), (WalkerOptions){0}))


#define dir_foreach_opt(arena, walker, entry, opt)                 \
    for (DirEntry entry = walker__next((arena), (walker), *(opt)); \
         !entry.over;                                              \
         entry = walker__next((arena), (walker), *(opt)), *(opt) = (WalkerOptions){0})


int main() {
    String path = SV("C:\\Users\\Aditya\\Programming");

    Arena *arena = arena_init();
    uint32_t max_depth = (uint32_t)-1;
    DirWalker walker = walker_init(path, max_depth);

#if 0
    DirEntry file = walker_next(arena, &walker);

    while (!file.over) {
        printf("Path: %.*s\n", SV_FMT(file.path));
        printf("Name: %.*s\n", SV_FMT(file.name));
        printf("Size: %zu bytes\n", file.size);
        printf("Last Modified: %zu\n", file.last_modified);
        printf("Directory: %s\n", file.is_dir? "yes": "no");
        printf("Hidden: %s\n", file.is_hidden? "yes": "no");
        printf("------------------------------------------\n");

        if (file.is_dir && string_eq(file.name, SV(".git"))) {
            file = walker_next(arena, &walker, .dont_recurse = true);
        } else {
            file = walker_next(arena, &walker);
        }
    }
#else
    WalkerOptions opt = {0};
    dir_foreach_opt(arena, &walker, file, &opt) {
        printf("Path: %.*s\n", SV_FMT(file.path));
        printf("Name: %.*s\n", SV_FMT(file.name));
        printf("Size: %zu bytes\n", file.size);
        printf("Last Modified: %zu\n", file.last_modified);
        printf("Directory: %s\n", file.is_dir? "yes": "no");
        printf("Hidden: %s\n", file.is_hidden? "yes": "no");
        printf("------------------------------------------\n");

        if (file.is_dir && string_eq(file.name, SV(".git"))) {
            opt.dont_recurse = true;
        }
    }
#endif
    arena_free(arena);
    return 0;
}

