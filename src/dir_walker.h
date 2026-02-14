#ifndef MIGI_DIR_WALKER_H
#define MIGI_DIR_WALKER_H

#include "migi_core.h"
#include "migi_string.h"
#include "migi_list.h"
#include "dynamic_string.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

#ifdef _WIN32
    typedef HANDLE Directory;
    #define DIRECTORY_INVALID INVALID_HANDLE_VALUE
#else
    typedef DIR *Directory;
    #define DIRECTORY_INVALID NULL
#endif // ifdef _WIN32

#ifdef _WIN32
    #define DIRECTORY_SEPARATOR S("\\")
#else
    #define DIRECTORY_SEPARATOR S("/")
#endif // ifdef _WIN32

typedef struct {
    Str path;
    Str name;
    size_t size;    // 0 for available (eg: for directories)
    uint32_t depth;

    // in unix epoch, 0 if not available (eg: time_created on linux)
    int64_t time_modified;
    int64_t time_accessed;
    int64_t time_created;

    // TODO: convert the booleans into bit-flags
    bool is_dir;
    bool is_symlink;
    bool is_hidden;

    // iteration specific
    bool over;
    bool error;
} DirIter;


typedef enum {
    DirWalkerMode_Recurse,
    DirWalkerMode_PopStack,
    DirWalkerMode_NextFile,
    DirWalkerMode_Return,
} DirWalkerMode;

typedef struct DirectoryNode DirectoryNode;
struct DirectoryNode {
    DirectoryNode *next;
    Directory dir;
};

typedef struct {
    DirectoryNode *dir_handles;
    // TODO: it might be possible to not have the temp_str, look more into it
    // seems like it might be possible to only modify current_dir with the path each time
    // at the start of walker__next, check if a entry exists and if so, subtract the length
    // of entry->name from current_dir to get back the directory again
    DStr current_dir;
    DStr temp_str;
    uint32_t depth;
    bool stop_on_error;
    bool follow_symlinks;
    DirWalkerMode mode;
    DirWalkerMode next_mode;

    Directory dir;
    DirIter entry;
} DirWalker;


typedef struct {
    bool stop_on_error;
    bool follow_symlinks;
} WalkerInitOpt;

static DirWalker walker_init_opt(Str filepath, WalkerInitOpt opt);
#define walker_init(filepath, ...) walker_init_opt((filepath), (WalkerInitOpt){ __VA_ARGS__ })
static void walker_free(DirWalker *w);

typedef struct {
    bool dont_recurse;
} WalkerNextOpt;
static DirIter walker_next_opt(Arena *arena, DirWalker *w, WalkerNextOpt opt);

// NOTE: the entry returned contains temporary data that will be freed on
// a subsequent call to `walker_next`, or the next loop iteration of `dir_foreach_*`
#define walker_next(arena, walker, ...) \
    walker_next_opt((arena), (walker), (WalkerNextOpt){ __VA_ARGS__ })

#define dir_foreach(arena, walker, it)                                        \
    for (DirIter it = walker_next_opt((arena), (walker), (WalkerNextOpt){0}); \
         !it.over;                                                            \
         it = walker_next_opt((arena), (walker), (WalkerNextOpt){0}))

#define dir_foreach_opt(arena, walker, it, opt)                   \
    for (DirIter it = walker_next_opt((arena), (walker), *(opt)); \
         !it.over;                                                \
         it = walker_next_opt((arena), (walker), *(opt)), *(opt) = (WalkerNextOpt){0})


typedef struct DirIterNode DirIterNode;
struct DirIterNode {
    DirIter entry;
    DirIterNode *next;
};

// Get all sub-directories and files in `dirpath`
// Does not recurse into sub-directories
static DirIterNode *dir_get_all_children(Arena *arena, Str dirpath);



#ifdef _WIN32
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
#endif


// TODO: maybe move these into an os specific header file?
#ifdef _WIN32
static void win32__os_to_entry(WIN32_FIND_DATA *file_info, DStr *parent_dir, uint32_t depth, DirIter *entry) {
    size_t end = parent_dir->length;
    dstr_pushf(parent_dir, "\\%s", file_info->cFileName);
    entry->path = parent_dir->as_string;
    entry->name = str_skip(parent_dir->as_string, end + 1);

    entry->depth = depth;

    entry->is_dir     = file_info->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
    entry->is_hidden  = file_info->dwFileAttributes & FILE_ATTRIBUTE_HIDDEN;
    entry->is_symlink = file_info->dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT;

    if (!entry->is_dir) {
        LARGE_INTEGER filesize = {
            .LowPart = file_info->nFileSizeLow,
            .HighPart = file_info->nFileSizeHigh
        };
        entry->size = filesize.QuadPart;
    }

    entry->time_modified = win32__system_time_to_unix(file_info->ftLastWriteTime);
    entry->time_accessed = win32__system_time_to_unix(file_info->ftLastAccessTime);
    entry->time_created  = win32__system_time_to_unix(file_info->ftCreationTime);
}
#else
static bool posix__os_to_entry(DStr *parent_dir, char *filename, uint32_t depth, DirEntry *entry) {
    entry->name = str_from_cstr(filename);
    dstr_pushf(parent_dir, "/%.*s", SArg(entry->name));
    struct stat statbuf;
    int res = stat(dstr_to_temp_cstr(parent_dir), &statbuf);
    if (res == -1) {
        migi_log(Log_Error, "failed to get file info for: `%.*s`: %s",
                SArg(parent_dir->as_string), strerror(errno));
        return false;
    }

    entry->path = parent_dir->as_string;
    entry->depth = depth;

    entry->is_dir     = S_ISDIR(statbuf.st_mode);
    entry->is_symlink = S_ISLNK(statbuf.st_mode);
    entry->is_hidden = entry->name.length != 0 && entry->name.data[0] == '.';

    if (!entry->is_dir) {
        entry->size = statbuf.st_size;
    }
    entry->time_modified = statbuf.st_mtim.tv_sec;
    entry->time_accessed = statbuf.st_atim.tv_sec;
    return true;
}
#endif // ifdef _WIN32


static bool walker__open_dir(DirWalker *w) {
#ifdef _WIN32
    // TODO: also prepend "\\?\" to the path to extend the file path size from MAX_PATH
    // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-findfirstfilea

    // Search with wildcard appended to end of file
    DStr *parent_dir = &w->temp_str;
    dstr_pushf(parent_dir, "%.*s\\*", SArg(w->current_dir.as_string));

    WIN32_FIND_DATA file_info;
    w->dir = FindFirstFile(dstr_to_temp_cstr(&w->temp_str), &file_info);
    if (w->dir == DIRECTORY_INVALID) {
        migi_log(Log_Error, "failed to open directory: `%.*s`: %ld", SArg(w->current_dir), GetLastError());
        return false;
    }

    // Remove the `\*` added at the end
    parent_dir->as_string = str_drop(parent_dir->as_string, 2);
    win32__os_to_entry(&file_info, parent_dir, w->depth, &w->entry);
    return true;
#else
    DStr *parent_dir = &w->temp_str;
    dstr_push(parent_dir, w->current_dir.as_string);
    w->dir = opendir(dstr_to_temp_cstr(parent_dir));
    if (w->dir == DIRECTORY_INVALID) {
        migi_log(Log_Error, "failed to open directory: `%.*s`: %s",
                SArg(parent_dir->as_string), strerror(errno));
        return false;
    }

    struct dirent *entry = readdir(w->dir);
    assertf(entry, "walker_open_dir: readdir cannot fail since dir was just opened");
    return posix__os_to_entry(parent_dir, entry->d_name, w->depth, &w->entry);
#endif // ifdef _WIN32
}

typedef enum {
    Read_Error,
    Read_Over,
    Read_Ok,
} ReadDirResult;

static ReadDirResult walker__read_dir(DirWalker *w) {
#ifdef _WIN32
    WIN32_FIND_DATA file_info;
    if (!FindNextFile(w->dir, &file_info)) {
        DWORD error = GetLastError();
        if (error == ERROR_NO_MORE_FILES) {
            return Read_Over;
        } else {
            migi_log(Log_Error, "failed to read file in directory: `%.*s`: %ld", SArg(w->current_dir), GetLastError());
            return Read_Error;
        }
    }

    DStr *parent_dir = &w->temp_str;
    dstr_push(parent_dir, w->current_dir.as_string);
    win32__os_to_entry(&file_info, parent_dir, w->depth, &w->entry);
    return Read_Ok;
#else
    errno = 0;
    struct dirent *entry = readdir(w->dir);
    assertf(errno != EBADF, "open_directory: invalid directory handle");
    if (!entry) {
        return Read_Over;
    }

    DStr *parent_dir = &w->temp_str;
    dstr_push(parent_dir, w->current_dir.as_string);
    if (!posix__os_to_entry(parent_dir, entry->d_name, w->depth, &w->entry)) return Read_Error;
    return Read_Ok;
#endif // ifdef _WIN32
}

static void walker__close_dir(Directory dir) {
#ifdef _WIN32
    FindClose(dir);
#else
    if (dir != DIRECTORY_INVALID) closedir(dir);
#endif // ifdef _WIN32
}

static DirWalker walker_init_opt(Str filepath, WalkerInitOpt opt) {
    DirWalker walker = {0};

    if (filepath.length == 0) {
        fprintf(stderr, "error: directory path cannot be empty\n");
        return walker;
    }

    // TODO: convert path to an absolute canonical form
    walker.current_dir = dstr_from_string(filepath);
    walker.mode = DirWalkerMode_Recurse;
    walker.stop_on_error = opt.stop_on_error;
    walker.follow_symlinks = opt.follow_symlinks;

    return walker;
}

static void walker__update(Arena *arena, DirWalker *w) {
    if (w->entry.is_dir) {
        // skip `.` and `..`
        if (str_eq(w->entry.name, S(".")) || str_eq(w->entry.name, S(".."))) {
            w->mode = DirWalkerMode_NextFile;
            return;
        }

        // skip symlinks if not explicitly asked to follow them
        if (w->entry.is_symlink && !w->follow_symlinks) {
            w->mode = DirWalkerMode_Return;
            return;
        }

        DirectoryNode *dir_handle = arena_new(arena, DirectoryNode);
        dir_handle->dir = w->dir;
        stack_push(w->dir_handles, dir_handle);
        w->depth += 1;
        w->entry.depth += 1; // TODO: do this when creating entry if its a dir

        dstr_pushf(&w->current_dir, "%.*s%.*s", SArg(DIRECTORY_SEPARATOR), SArg(w->entry.name));
        w->next_mode = DirWalkerMode_Recurse;
    } else {
        w->next_mode = DirWalkerMode_NextFile;
    }

    w->mode = DirWalkerMode_Return;
}

static DirIter walker_next_opt(Arena *arena, DirWalker *w, WalkerNextOpt opt) {
    w->entry = (DirIter){0};

    if (opt.dont_recurse) {
        w->mode = DirWalkerMode_PopStack;
    }

    while (true) {
        w->temp_str.length = 0;

        switch (w->mode) {
            case DirWalkerMode_Recurse: {
                bool ok = walker__open_dir(w);
                if (!ok) {
                    w->entry.over = w->stop_on_error;
                    w->entry.error = true;
                    walker__close_dir(w->dir);
                    w->dir = DIRECTORY_INVALID;
                    w->next_mode = DirWalkerMode_PopStack;
                    w->mode = DirWalkerMode_Return;
                } else {
                    walker__update(arena, w);
                }
            } break;

            case DirWalkerMode_PopStack: {
                if (w->dir_handles == NULL) {
                    walker__close_dir(w->dir);
                    w->dir = DIRECTORY_INVALID;
                    w->entry.over = true;
                    w->next_mode = DirWalkerMode_NextFile;
                    w->mode = DirWalkerMode_Return;
                } else {
                    w->dir = w->dir_handles->dir;
                    stack_pop(w->dir_handles);
                    w->depth -= 1;

                    // Remove the last directory from current_dir
                    int64_t parent_end = str_find_ex(w->current_dir.as_string, DIRECTORY_SEPARATOR, Find_Reverse);
                    assertf(parent_end != -1, "there must be atleast one directory separator");
                    w->current_dir.as_string = str_take(w->current_dir.as_string, parent_end);

                    w->mode = DirWalkerMode_NextFile;
                }
            } break;

            case DirWalkerMode_NextFile: {
                switch (walker__read_dir(w)) {
                    case Read_Error: {
                        w->entry.over = w->stop_on_error;
                        w->entry.error = true;
                        w->next_mode = DirWalkerMode_NextFile;
                        w->mode = DirWalkerMode_Return;
                    } break;
                    case Read_Over: {
                        walker__close_dir(w->dir);
                        w->dir = DIRECTORY_INVALID;
                        w->mode = DirWalkerMode_PopStack;
                    } break;
                    case Read_Ok: {
                        walker__update(arena, w);
                    } break;
                }
            } break;

            case DirWalkerMode_Return: {
                w->mode = w->next_mode;
                return w->entry;
            } break;

            default: migi_unreachable();
        }
    }
}

static void walker_free(DirWalker *w) {
    migi_log(Log_Debug, "Temp Str Allocated: %zu bytes", w->temp_str.capacity);
    migi_log(Log_Debug, "Current Directory Allocated: %zu bytes", w->current_dir.capacity);

    // if stop_on_error was enabled, then all open directory handles might not have been closed
    if (w->stop_on_error) {
        list_foreach(w->dir_handles, DirectoryNode, dir_handle) {
            walker__close_dir(dir_handle->dir);
        }
        walker__close_dir(w->dir);
    }
    dstr_free(&w->temp_str);
    dstr_free(&w->current_dir);
    mem_clear(w);
}

static DirIterNode *dir_get_all_children(Arena *arena, Str dir_path) {
    DirWalker walker = walker_init(dir_path, .stop_on_error = false);

    Temp tmp = arena_temp_excl(arena);
    DirIterNode *head = NULL;

    WalkerNextOpt opt = {0};
    dir_foreach_opt(tmp.arena, &walker, entry, &opt) {
        if (entry.error) continue;

        if (entry.depth >= 1) {
            opt.dont_recurse = true;
        }

        DirIterNode *node = arena_new(arena, DirIterNode);
        node->entry = entry;
        node->entry.path = str_copy(arena, node->entry.path);
        node->entry.name = str_skip(node->entry.path,
            str_find_ex(node->entry.path, DIRECTORY_SEPARATOR, Find_Reverse) + 1);
        stack_push(head, node);
    }

    walker_free(&walker);
    arena_temp_release(tmp);
    return head;
}

#endif // ifndef MIGI_DIR_WALKER_H
