#include "migi.h"
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
    typedef HANDLE DirHandle;
    #define INVALID_DIR_HANDLE INVALID_HANDLE_VALUE
#else
    typedef DIR *DirHandle;
    #define INVALID_DIR_HANDLE NULL
#endif // ifdef _WIN32

#ifdef _WIN32
    #define DIRECTORY_SEPARATOR S("\\")
#else
    #define DIRECTORY_SEPARATOR S("/")
#endif // ifdef _WIN32

typedef enum {
    DirWalkerMode_Recurse,
    DirWalkerMode_PopStack,
    DirWalkerMode_NextFile,
    DirWalkerMode_Return,
} DirWalkerMode;

typedef struct DirHandleNode DirHandleNode;
struct DirHandleNode {
    DirHandleNode *next;
    DirHandle dir;
};

typedef struct {
    Str path;
    Str name;
    size_t size;    // 0 for directories
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
} DirEntry;


typedef struct {
    DirHandleNode *dir_handles;
    // TODO: it might be possible to not have the temp_str, look more into it
    // seems like it might be possible to only modify current_dir with the path each time
    // at the start of walker__next, check if a entry exists and if so, subtract the length
    // of entry->name from current_dir to get back the directory again
    DStr current_dir;
    DStr temp_str;
    uint32_t depth;
    uint32_t max_depth;
    bool stop_on_error;
    bool traverse_symlinks;
    DirWalkerMode mode;
    DirWalkerMode next_mode;

    DirHandle dir;
    DirEntry entry;
} DirWalker;


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

#ifdef _WIN32
static void win32__os_to_entry(WIN32_FIND_DATA *file_info, DStr *parent_dir, DirEntry *entry) {
    size_t end = parent_dir->length;
    dstr_pushf(parent_dir, "\\%s", file_info->cFileName);
    entry->path = parent_dir->as_string;
    entry->name = str_skip(parent_dir->as_string, end + 1);

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
static bool posix__os_to_entry(DString *parent_dir, char *filename, DirEntry *entry) {
    entry->name = str_from_cstr(filename);
    dstring_pushf(parent_dir, "/%.*s", SArg(entry->name));
    struct stat statbuf;
    int res = stat(dstring_to_temp_cstr(parent_dir), &statbuf);
    if (res == -1) {
        migi_log(Log_Error, "failed to get file info for: `%.*s`: %s",
                SArg(parent_dir->as_string), strerror(errno));
        return false;
    }

    entry->path = parent_dir->as_string;

    entry->is_dir     = S_ISDIR(statbuf.st_mode);
    entry->is_symlink = S_ISLNK(statbuf.st_mode);
    entry->is_hidden = entry->name.length != 0 && entry->name.data[0] == '.';

    if (!entry.is_dir) {
        entry->size = statbuf.st_size;
    }
    entry->time_modified = statbuf.st_mtim.tv_sec;
    entry->time_accessed = statbuf.st_atim.tv_sec;
    return true;
}
#endif // ifdef _WIN32


static bool walker_open_dir(DirWalker *w) {
#ifdef _WIN32
    // TODO: also prepend "\\?\" to the path to extend the file path size from MAX_PATH
    // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-findfirstfilea

    // Search with wildcard appended to end of file
    DStr *parent_dir = &w->temp_str;
    dstr_pushf(parent_dir, "%.*s\\*", SArg(w->current_dir.as_string));

    WIN32_FIND_DATA file_info;
    w->dir = FindFirstFile(dstr_to_temp_cstr(&w->temp_str), &file_info);
    if (w->dir == INVALID_HANDLE_VALUE) {
        migi_log(Log_Error, "failed to open directory: `%.*s`: %ld", SArg(w->current_dir), GetLastError());
        return false;
    }

    // Remove the `\*` added at the end
    parent_dir->as_string = str_drop(parent_dir->as_string, 2);
    win32__os_to_entry(&file_info, parent_dir, &w->entry);
    return true;
#else
    DString *parent_dir = &w->temp_str;
    dstring_push(parent_dir, w->current_dir.as_string);
    w->dir = opendir(dstring_to_temp_cstr(parent_dir));
    if (!w->dir) {
        migi_log(Log_Error, "failed to open directory: `%.*s`: %s",
                SArg(parent_dir->as_string), strerror(errno));
        return false;
    }

    struct dirent *entry = readdir(w->dir);
    assertf(entry, "walker_open_dir: readdir cannot fail since dir was just opened");
    return posix__os_to_entry(parent_dir, entry->d_name, &w->entry);
#endif // ifdef _WIN32
}

typedef enum {
    Read_Error,
    Read_Over,
    Read_Ok,
} ReadDirResult;

static ReadDirResult walker_read_dir(DirWalker *w) {
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
    win32__os_to_entry(&file_info, parent_dir, &w->entry);
    w->entry.depth = w->depth;
    return Read_Ok;
#else
    errno = 0;
    struct dirent *entry = readdir(w->dir);
    assertf(errno != EBADF, "open_directory: invalid directory handle");
    if (!entry) {
        return Read_Over;
    }

    DString *parent_dir = &w->temp_str;
    dstring_push(parent_dir, w->current_dir.as_string);
    if (!posix__os_to_entry(parent_dir, entry->d_name, &w->entry)) return Read_Error;
    w->entry.depth = w->depth;
    return Read_Ok;
#endif // ifdef _WIN32
}

static void close_directory(DirHandle dir) {
#ifdef _WIN32
    FindClose(dir);
#else
    if (dir != INVALID_DIR_HANDLE) closedir(dir);
#endif // ifdef _WIN32
}

typedef struct {
    // -1 means no limit (default: 1)
    uint32_t max_depth;
    // stop immediately on error rather than continuing (default: false)
    bool stop_on_error;

    bool traverse_symlinks;
} WalkerInitOpt;

// TODO: maybe return a pointer to the struct allocated on the arena
static DirWalker walker__init(Str filepath, WalkerInitOpt opt) {
    DirWalker walker = {0};

    if (filepath.length == 0) {
        fprintf(stderr, "error: directory path cannot be empty\n");
        return walker;
    }

    // TODO: convert path to an absolute canonical form
    walker.current_dir = dstr_from_string(filepath);
    walker.mode = DirWalkerMode_Recurse;
    walker.max_depth = opt.max_depth;
    walker.stop_on_error = opt.stop_on_error;
    walker.traverse_symlinks = opt.traverse_symlinks;

    return walker;
}

#define WALKER_INFINITE_DEPTH ((uint32_t)-1)

#define walker_init(filepath, ...) walker__init((filepath), (WalkerInitOpt){ .max_depth = 1, __VA_ARGS__ })



static void walker_update(Arena *arena, DirWalker *w) {
    if (w->entry.is_dir) {
        // skip `.` and `..`
        if (str_eq(w->entry.name, S(".")) || str_eq(w->entry.name, S(".."))) {
            w->next_mode = DirWalkerMode_NextFile;
            w->mode = DirWalkerMode_Return;
            return;
        }

        // skip symlinks if not explicitly asked to traverse them
        if (w->entry.is_symlink && !w->traverse_symlinks) {
            w->mode = DirWalkerMode_Return;
            return;
        }

        DirHandleNode *dir_handle = arena_new(arena, DirHandleNode);
        dir_handle->dir = w->dir;
        stack_push(w->dir_handles, dir_handle);
        w->depth += 1;

        dstr_pushf(&w->current_dir, "%.*s%.*s", SArg(DIRECTORY_SEPARATOR), SArg(w->entry.name));
        if (w->depth < w->max_depth) {
            w->next_mode = DirWalkerMode_Recurse;
        } else {
            w->next_mode = DirWalkerMode_PopStack;
        }
    } else {
        w->next_mode = DirWalkerMode_NextFile;
    }

    w->mode = DirWalkerMode_Return;
}


typedef struct {
    bool dont_recurse;
} WalkerOptions;

static DirEntry walker__next(Arena *arena, DirWalker *w, WalkerOptions opt) {
    w->entry = (DirEntry){0};

    while (true) {
        w->temp_str.length = 0;

        switch (w->mode) {
            case DirWalkerMode_Recurse: {
                if (opt.dont_recurse) {
                    w->mode = DirWalkerMode_PopStack;
                } else {
                    bool ok = walker_open_dir(w);
                    if (!ok) {
                        w->entry.over = w->stop_on_error;
                        w->entry.error = true;
                        close_directory(w->dir);
                        w->next_mode = DirWalkerMode_PopStack;
                        w->mode = DirWalkerMode_Return;
                    } else {
                        walker_update(arena, w);
                    }
                }
            } break;

            case DirWalkerMode_PopStack: {
                if (w->dir_handles == NULL) {
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
                switch (walker_read_dir(w)) {
                    case Read_Error: {
                        w->entry.over = w->stop_on_error;
                        w->entry.error = true;
                        w->next_mode = DirWalkerMode_NextFile;
                        w->mode = DirWalkerMode_Return;
                    } break;
                    case Read_Over: {
                        close_directory(w->dir);
                        w->mode = DirWalkerMode_PopStack;
                    } break;
                    case Read_Ok: {
                        walker_update(arena, w);
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
    if (w->stop_on_error) {
        list_foreach(w->dir_handles, DirHandleNode, dir_handle) {
            close_directory(dir_handle->dir);
        }
        close_directory(w->dir);
    }
    dstr_free(&w->temp_str);
    dstr_free(&w->current_dir);
    mem_clear(w);
}

// NOTE: the entry returned contains temporary data that will be freed on
// a subsequent call to `walker_next`, or the next loop iteration of `dir_foreach_*`
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


typedef struct DirEntryNode DirEntryNode;
struct DirEntryNode {
    DirEntry entry;
    DirEntryNode *next;
};


static DirEntryNode *dir_get_all(Arena *arena, Str dir, uint32_t max_depth) {
    DirWalker walker = walker_init(dir, .max_depth = max_depth, .stop_on_error = false);

    Temp tmp = arena_temp_excl(arena);
    DirEntryNode *head = NULL;
    dir_foreach(tmp.arena, &walker, entry) {
        DirEntryNode *node = arena_new(arena, DirEntryNode);
        node->entry = entry;
        node->entry.path = str_copy(arena, node->entry.path);
        node->entry.name = str_skip(node->entry.path,
            str_find_ex(node->entry.path, DIRECTORY_SEPARATOR, Find_Reverse) + 1);
        stack_push(head, node);
    }

    arena_temp_release(tmp);
    walker_free(&walker);
    return head;
}

Str file_type_str(Arena *a, DirEntry entry) {
    Str s = {0};
    if (entry.is_dir) {
        s = str_copy(a, S("Directory, "));
    }

    if (entry.is_symlink) {
        s = str_copy(a, S("Symbolic Link, "));
    }

    if (entry.is_hidden) {
        s = str_copy(a, S("Hidden, "));
    }

    if (s.length == 0) {
        s = str_copy(a, S("Regular, "));
    }
    return s;
}


void test_walk_dir(Str path) {
    DirWalker walker = walker_init(path,
                        .max_depth         = WALKER_INFINITE_DEPTH,
                        .stop_on_error     = false,
                        .traverse_symlinks = true);

    Temp tmp = arena_temp();
#if 0
    DirEntry file = walker_next(tmp.arena, &walker, false);

    while (!file.over) {
        printf("Path: %.*s\n", SArg(file.path));
        printf("Name: %.*s\n", SArg(file.name));
        printf("Size: %zu bytes\n", file.size);
        printf("Time Created: %zu\n", file.time_created);
        printf("Time Modified: %zu\n", file.time_modified);
        printf("Time Accessed: %zu\n", file.time_accessed);
        printf("Type: %.*s\n", SArg(file_type_str(tmp.arena, file)));
        printf("Hidden: %.*s\n", SArg(bool_to_str(file.is_hidden)));
        printf("------------------------------------------\n");

        bool dont_recurse = file.is_dir && str_eq(file.name, S(".git"));
        file = walker_next(tmp.arena, &walker, .dont_recurse = dont_recurse);
    }
#else
    WalkerOptions opt = {0};
    dir_foreach_opt(tmp.arena, &walker, file, &opt) {
        if (file.error) continue;

        printf("Path: %.*s\n", SArg(file.path));
        printf("Name: %.*s\n", SArg(file.name));
        printf("Size: %zu bytes\n", file.size);
        printf("Time Created: %zu\n", file.time_created);
        printf("Time Modified: %zu\n", file.time_modified);
        printf("Time Accessed: %zu\n", file.time_accessed);
        printf("Type: %.*s\n", SArg(file_type_str(tmp.arena, file)));
        printf("Hidden: %.*s\n", SArg(bool_to_str(file.is_hidden)));
        printf("------------------------------------------\n");

        opt.dont_recurse = file.is_dir && str_eq(file.name, S(".git"));
    }
#endif
    walker_free(&walker);
    arena_temp_release(tmp);
}


int main(int argc, char **argv) {
    if (argc <= 1) {
        migi_log(Log_Error, "expected a path as first argument");
        return 1;
    }
    Str path = str_from_cstr(argv[1]);
    test_walk_dir(path);

    Temp tmp = arena_temp();
    uint32_t max_depth = 3;
    DirEntryNode *entries = dir_get_all(tmp.arena, path, max_depth);

    list_foreach(entries, DirEntryNode, node) {
        printf("%.*s\n", SArg(node->entry.name));
    }
    arena_temp_release(tmp);

    return 0;
}
