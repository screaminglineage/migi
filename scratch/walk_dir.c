#define _CRT_SECURE_NO_WARNINGS

#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <windows.h>

#include "migi_string.h"
#include "arena.h"
#include "migi_lists.h"

typedef struct FindFrame FindFrame;
struct FindFrame {
    String dir_name;
    FindFrame *next;

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
    Arena *arena;
    Arena *temp_arena;
    FindFrame *find_frames;
    String current_dir;
    int depth;
    DirWalkerMode mode;
    DirWalkerMode next_mode;

    // WIN32 specific
    HANDLE find;
    WIN32_FIND_DATA file_data;
} DirWalker;

typedef struct {
    String path;
    String name;
    size_t size;
    bool is_dir;
    bool is_hidden;
    bool over;
    bool error;
} DirEntry;

// TODO: also prepend "\\?\" to the path to extend the file path size from MAX_PATH
// https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-findfirstfilea
// TODO: come up with a better name
// Search with wildcard appended to end of file
static HANDLE find_first_file(String dir, WIN32_FIND_DATA *file_data, Arena *temp) {
    HANDLE find = INVALID_HANDLE_VALUE;
    String dir_with_wildcard = stringf(temp, "%.*s\\*", SV_FMT(dir));
    *arena_new(temp, char) = 0;
    find = FindFirstFile(dir_with_wildcard.data, file_data);
    return find;
}

// TODO: maybe return a pointer to an opaque struct allocated on the arena
// DirWalker is a bit too big (384 bytes) to be allocated on the stack anyway 
static DirWalker walker_init(Arena *arena, String filepath) {
    DirWalker walker = {0};

    if (filepath.length == 0) {
        fprintf(stderr, "error: directory path cannot be empty\n");
        return walker;
    }

    walker.arena = arena;
    walker.current_dir = filepath;
    walker.mode = DirWalkerMode_Recurse;

    // should be greater than (MAX_PATH * 2) since find_first_file also uses it
    size_t temp_path_buf_size = 2048;
    char *temp_path_buf = arena_push(walker.arena, char, temp_path_buf_size);
    walker.temp_arena = arena_init_static(temp_path_buf, temp_path_buf_size);

    return walker;
}

// TODO: come up with a better name since it also updates the mode of the walker itself rather than simply filling the entry
static void walker_fill_entry(DirWalker *w, DirEntry *entry, int max_depth) {
    if (w->file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        // skip `.` and `..`
        if (strcmp(w->file_data.cFileName, ".") == 0 || strcmp(w->file_data.cFileName, "..") == 0) {
            w->mode = DirWalkerMode_NextFile;
            return;
        }

        FindFrame *frame = arena_new(w->arena, FindFrame);
        frame->find = w->find;
        frame->dir_name = w->current_dir;
        stack_push(w->find_frames, frame);
        w->depth += 1;

        w->current_dir = stringf(w->arena, "%.*s\\%s", SV_FMT(w->current_dir), w->file_data.cFileName);
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
        entry->path = stringf(w->temp_arena, "%.*s\\%s", SV_FMT(w->current_dir), w->file_data.cFileName);
        entry->name = string_from_cstr(w->file_data.cFileName);
        w->next_mode = DirWalkerMode_NextFile;
    }
    entry->is_hidden = w->file_data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN;
    w->mode = DirWalkerMode_Return;
}

static DirEntry walker_next(DirWalker *w, int max_depth) {
    DirEntry entry = {0};

    while (true) {
        arena_reset(w->temp_arena);

        switch (w->mode) {
            case DirWalkerMode_Recurse: {
                w->find = find_first_file(w->current_dir, &w->file_data, w->temp_arena);
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
                    walker_fill_entry(w, &entry, max_depth);
                }
            } break;

            case DirWalkerMode_PopStack: {
                if (w->find_frames == NULL) {
                    entry.over = true;
                    w->next_mode = DirWalkerMode_NextFile;
                    w->mode = DirWalkerMode_Return;
                } else {
                    w->find = w->find_frames->find;
                    w->current_dir = w->find_frames->dir_name;
                    stack_pop(w->find_frames);
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
                    walker_fill_entry(w, &entry, max_depth);
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

int main() {
    String path = SV("C:\\Users\\Aditya\\Programming");

    Arena *arena = arena_init();
    DirWalker walker = walker_init(arena, path);
    int max_depth = 2;
    DirEntry file = walker_next(&walker, max_depth);
    while (!file.over) {
        printf("Path: %.*s\n", SV_FMT(file.path));
        printf("Name: %.*s\n", SV_FMT(file.name));
        printf("Directory: %s\n", file.is_dir? "yes": "no");
        printf("Hidden: %s\n", file.is_hidden? "yes": "no");
        printf("------------------------------------------\n");

        file = walker_next(&walker, max_depth);
    }
    arena_free(arena);
    return 0;
}

