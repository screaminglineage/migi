

#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

#include "migi.h"
#include "dynamic_string.h"

#ifdef _WIN32
    typedef HANDLE DirHandle;
    #define INVALID_DIR_HANDLE INVALID_HANDLE_VALUE
#else
    typedef DIR *DirHandle;
    #define INVALID_DIR_HANDLE NULL
#endif // ifdef _WIN32

#ifdef _WIN32
    #define DIRECTORY_SEPARATOR '\\'
#else
    #define DIRECTORY_SEPARATOR '/'
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
    String path;
    String name;
    size_t size;    // 0 for directories

    // in unix epoch, 0 if not available (eg: time_created on linux)
    int64_t time_modified;
    int64_t time_accessed;
    int64_t time_created;

    // TODO: convert the booleans into bit-flags
    bool is_dir;
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
    DString current_dir;
    DString temp_str;
    uint32_t depth;
    uint32_t max_depth;
    bool stop_on_error;
    DirWalkerMode mode;
    DirWalkerMode next_mode;

    DirHandle dir;
    DirEntry entry;
} DirWalker;

static bool walker_os_to_entry(DString *parent_dir, char *filename, DirEntry *entry) {
#ifdef _WIN32
    todo();
#else
    entry->name = string_from_cstr(filename);
    dstring_pushf(parent_dir, "/%.*s", SV_FMT(entry->name));
    struct stat statbuf;
    int res = stat(dstring_to_temp_cstr(parent_dir), &statbuf);
    if (res == -1) {
        migi_log(Log_Error, "failed to get file info for: `%.*s`: %s",
                SV_FMT(parent_dir->as_string), strerror(errno));
        return false;
    }

    entry->path = parent_dir->as_string;
    entry->is_dir = S_ISDIR(statbuf.st_mode);
    entry->size = entry->is_dir? 0: statbuf.st_size;
    entry->time_modified = statbuf.st_mtim.tv_sec;
    entry->time_accessed = statbuf.st_atim.tv_sec;
    entry->is_hidden = entry->name.length != 0 && entry->name.data[0] == '.';

    return true;
#endif // ifdef _WIN32
}


static bool walker_open_dir(DirWalker *w) {
#ifdef _WIN32
    // TODO: also prepend "\\?\" to the path to extend the file path size from MAX_PATH
    // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-findfirstfilea
    // TODO: come up with a better name
    // Search with wildcard appended to end of file
    dstring_pushf(w->temp_str, "%.*s\\*", SV_FMT(directory));
    *dir = FindFirstFile(dstring_to_temp_cstr(temp_str), file_info);
    if (*dir == INVALID_HANDLE_VALUE) {
        // DisplayErrorBox(TEXT("FindFirstFile"));
        // TODO: print the error here
        // https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-formatmessage
        migi_log(Log_Error, "failed to open directory: `%s`: %d", SV_FMT(directory), GetLastError());
        // FindClose(w->find); // TODO: does the handle still need to be closed?
        return false;
    }
    return true;
#else
    DString *parent_dir = &w->temp_str;
    dstring_push(parent_dir, w->current_dir.as_string);
    w->dir = opendir(dstring_to_temp_cstr(parent_dir));
    if (!w->dir) {
        migi_log(Log_Error, "failed to open directory: `%.*s`: %s",
                SV_FMT(parent_dir->as_string), strerror(errno));
        return false;
    }

    struct dirent *entry = readdir(w->dir);
    assertf(entry, "walker_open_dir: readdir cannot fail since dir was just opened");
    return walker_os_to_entry(parent_dir, entry->d_name, &w->entry);
#endif // ifdef _WIN32
}

typedef enum {
    Read_Error,
    Read_Over,
    Read_Ok,
} ReadDirResult;

static ReadDirResult walker_read_dir(DirWalker *w) {
#ifdef _WIN32
    BOOL ok = FindNextFile(dir, file_info);
    if (ok) return Read_Ok;

    DWORD error = GetLastError();
    if (error == ERROR_NO_MORE_FILES) {
        return Read_Over;
    } else {
        // DisplayErrorBox(TEXT("FindNextFile"));
        // TODO: print the error here
        // https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-formatmessage
        migi_log(Log_Error, "failed to read file in directory: `%s`: %d", SV_FMT(directory), GetLastError());
        return Read_Error;
    }
#else
    errno = 0;
    struct dirent *entry = readdir(w->dir);
    assertf(errno != EBADF, "open_directory: invalid directory handle");
    if (!entry) {
        return Read_Over;
    }

    DString *parent_dir = &w->temp_str;
    dstring_push(parent_dir, w->current_dir.as_string);
    if (!walker_os_to_entry(parent_dir, entry->d_name, &w->entry)) return Read_Error;
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

// TODO: add an option to disable entering symbolic links
typedef struct {
    // -1 means no limit (default: 1)
    uint32_t max_depth;
    // stop immediately on error rather than continuing (default: false)
    bool stop_on_error;
} WalkerInitOpt;

// TODO: maybe return a pointer to the struct allocated on the arena
static DirWalker walker__init(String filepath, WalkerInitOpt opt) {
    DirWalker walker = {0};

    if (filepath.length == 0) {
        fprintf(stderr, "error: directory path cannot be empty\n");
        return walker;
    }

    // TODO: convert path to an absolute canonical form
    walker.current_dir = dstring_from_string(filepath);
    walker.mode = DirWalkerMode_Recurse;
    walker.max_depth = opt.max_depth;
    walker.stop_on_error = opt.stop_on_error;

    return walker;
}

#define walker_init(filepath, ...) walker__init((filepath), (WalkerInitOpt){ .max_depth = 1, __VA_ARGS__ })

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


static void walker_update(Arena *arena, DirWalker *w) {
#ifdef _WIN32
    todof("put these into walker_os_to_entry instead");
    if (w->file_info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        // skip `.` and `..`
        if (strcmp(w->file_info.cFileName, ".") == 0 || strcmp(w->file_info.cFileName, "..") == 0) {
            w->mode = DirWalkerMode_NextFile;
            return;
        }

        FindHandles *find_handle = arena_new(arena, FindHandles);
        find_handle->find = w->find;
        stack_push(w->find_handles, find_handle);
        w->depth += 1;

        todof("use current_dir to build the directory path rather than stringf here")
        w->current_dir = stringf(arena, "%.*s\\%s", SV_FMT(w->current_dir), w->file_info.cFileName);
        entry->name = string_from_cstr(w->file_info.cFileName);
        entry->path = w->current_dir;
        entry->is_dir = true;
        if (w->depth < max_depth) {
            w->next_mode = DirWalkerMode_Recurse;
        } else {
            w->next_mode = DirWalkerMode_PopStack;
        }
    } else {
        LARGE_INTEGER filesize = {
            .LowPart = w->file_info.nFileSizeLow,
            .HighPart = w->file_info.nFileSizeHigh
        };
        entry->size = filesize.QuadPart;
        dstring_pushf(&w->temp_str, "%.*s\\%s", SV_FMT(w->current_dir), w->file_info.cFileName);
        entry->path = w->temp_str.as_string;
        entry->name = string_from_cstr(w->file_info.cFileName);
        w->next_mode = DirWalkerMode_NextFile;
    }
    entry->time_modified = win32__system_time_to_unix(w->file_info.ftLastWriteTime);
    entry->time_accessed = win32__system_time_to_unix(w->file_info.ftLastAccessTime);
    entry->time_created  = win32__system_time_to_unix(w->file_info.ftCreationTime);
    entry->is_hidden     = w->file_info.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN;

#else
    if (w->entry.is_dir) {
        // skip `.` and `..`
        if (string_eq(w->entry.name, SV(".")) || string_eq(w->entry.name, SV(".."))) {
            w->mode = DirWalkerMode_NextFile;
            return;
        }

        DirHandleNode *dir_handle = arena_new(arena, DirHandleNode);
        dir_handle->dir = w->dir;
        stack_push(w->dir_handles, dir_handle);
        w->depth += 1;

        dstring_pushf(&w->current_dir, "/%.*s", SV_FMT(w->entry.name));
        if (w->depth < w->max_depth) {
            w->next_mode = DirWalkerMode_Recurse;
        } else {
            w->next_mode = DirWalkerMode_PopStack;
        }
    } else {
        w->next_mode = DirWalkerMode_NextFile;
    }
#endif // ifdef _WIN32

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
                    int64_t parent_end = string_find_char_rev(w->current_dir.as_string, DIRECTORY_SEPARATOR);
                    assertf(parent_end != -1, "there must be atleast one directory separator");
                    w->current_dir.as_string = string_take(w->current_dir.as_string, parent_end);

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
    migi_log(Log_Debug, "Temp String Allocated: %zu bytes", w->temp_str.capacity);
    migi_log(Log_Debug, "Current Directory Allocated: %zu bytes", w->current_dir.capacity);
    if (w->stop_on_error) {
        list_foreach(w->dir_handles, DirHandleNode, dir_handle) {
            close_directory(dir_handle->dir);
        }
        close_directory(w->dir);
    }
    dstring_free(&w->temp_str);
    dstring_free(&w->current_dir);
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


int main(int argc, char **argv) {
    if (argc <= 1) {
        migi_log(Log_Error, "expected a path as first argument");
        return 1;
    }
    String path = string_from_cstr(argv[1]);

    Arena *arena = arena_init();
    DirWalker walker = walker_init(path, .max_depth = -1, .stop_on_error = false);

#if 0
    DirEntry file = walker_next(arena, &walker, false);

    while (!file.over) {
        printf("Path: %.*s\n", SV_FMT(file.path));
        printf("Name: %.*s\n", SV_FMT(file.name));
        printf("Size: %zu bytes\n", file.size);
        printf("Time Created: %zu\n", file.time_created);
        printf("Time Modified: %zu\n", file.time_modified);
        printf("Time Accessed: %zu\n", file.time_accessed);
        printf("Directory: %s\n", file.is_dir? "yes": "no");
        printf("Hidden: %s\n", file.is_hidden? "yes": "no");
        printf("------------------------------------------\n");

        if (file.is_dir && string_eq(file.name, SV(".git"))) {
            file = walker_next(arena, &walker, .dont_recurse = true);
        } else {
            file = walker_next(arena, &walker, .dont_recurse = false);
        }
    }
#else
    WalkerOptions opt = {0};
    dir_foreach_opt(arena, &walker, file, &opt) {
        if (file.error) continue;

        printf("Path: %.*s\n", SV_FMT(file.path));
        printf("Name: %.*s\n", SV_FMT(file.name));
        printf("Size: %zu bytes\n", file.size);
        printf("Time Created: %zu\n", file.time_created);
        printf("Time Modified: %zu\n", file.time_modified);
        printf("Time Accessed: %zu\n", file.time_accessed);
        printf("Directory: %s\n", file.is_dir? "yes": "no");
        printf("Hidden: %s\n", file.is_hidden? "yes": "no");
        printf("------------------------------------------\n");

        if (file.is_dir && string_eq(file.name, SV(".git"))) {
            opt.dont_recurse = true;
        }
    }
#endif
    walker_free(&walker);
    arena_free(arena);
    return 0;
}

