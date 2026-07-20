#ifndef MIGI_FILESYSTEM_H
#define MIGI_FILESYSTEM_H

#include "file.h"

// TODO: add file_info() for basic file info (size/times/etc.)

typedef struct {
    bool error;
    bool is_directory;
    bool is_symbolic_link;
} FileType;

typedef struct {
    bool replace_existing;
} FileOpt;

static bool file_touch(Str filepath);
static bool file_copy_opt(Str from, Str to, FileOpt opt);
#define file_copy(from, to, ...) file_copy_opt((from), (to), (FileOpt){__VA_ARGS__})
static bool file_move_opt(Str from, Str to, FileOpt opt);
#define file_move(from, to, ...) file_move_opt((from), (to), (FileOpt){__VA_ARGS__})
static bool file_delete(Str filepath);
static bool file_exists(Str filepath);
static FileType file_type(Str filepath);

typedef struct {
    bool recursive;
} DirDeleteOpt;

static bool dir_make_if_not_exists(Str dirpath);
static bool dir_copy(Str from, Str to);

static bool dir_move(Str from, Str to);
static bool dir_delete_opt(Str filepath, DirDeleteOpt opt);
#define dir_delete(filepath, ...) dir_delete_opt((filepath), (DirDeleteOpt){__VA_ARGS__})

static Str get_cwd(Arena *arena);
static bool set_cwd(Str cwd);
static Str get_cwd_executable(Arena *arena);
static Str get_executable_path(Arena *a);


#if OS_WINDOWS

#include <windows.h>
#include "filepath.h"
#include "file.h"
#include "dir_walker.h"

static FileType file_type(Str filepath) {
    FileType result = {0};
    Temp tmp = arena_temp();

    int attrs = GetFileAttributes(str_to_cstr(tmp.arena, filepath));
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        Str err_str = str_last_error(tmp.arena);
        migi_log(Log_Error, "Failed to get file type for: '%.*s': %.*s", SArg(filepath), SArg(err_str));
        result.error = true;
    } else {
        result.is_directory       = attrs & FILE_ATTRIBUTE_DIRECTORY;
        result.is_symbolic_link   = attrs & FILE_ATTRIBUTE_REPARSE_POINT;
    }

    arena_temp_release(tmp);
    return result;
}


static bool file__exists(const char *filepath) {
    int attrs = GetFileAttributes(filepath);
    return attrs != INVALID_FILE_ATTRIBUTES;
}

static bool file_exists(Str filepath) {
    Temp tmp = arena_temp();
    bool exists = file__exists(str_to_cstr(tmp.arena, filepath));
    arena_temp_release(tmp);
    return exists;
}

static bool file_touch(Str filepath) {
    bool result = true;
    Temp tmp = arena_temp();

    HANDLE file = CreateFileA(str_to_cstr(tmp.arena, filepath),
            GENERIC_WRITE, 0, NULL, OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL, NULL);

    if (file == INVALID_HANDLE_VALUE) {
        Str err_str = str_last_error(tmp.arena);
        migi_log(Log_Error, "Failed to touch file: '%.*s': %.*s", SArg(filepath), SArg(err_str));
        goto_end_with(false);
    }

    SYSTEMTIME sys_time;
    GetSystemTime(&sys_time);

    FILETIME file_time;
    if (!SystemTimeToFileTime(&sys_time, &file_time)) {
        Str err_str = str_last_error(tmp.arena);
        migi_log(Log_Error, "Failed to convert system time to file time: '%.*s': %.*s", SArg(filepath), SArg(err_str));
        goto_end_with(false);
    }

    if (!SetFileTime(file, &file_time, &file_time, &file_time)) {
        Str err_str = str_last_error(tmp.arena);
        migi_log(Log_Error, "Failed to set file time: '%.*s': %.*s", SArg(filepath), SArg(err_str));
        goto_end_with(false);
    }

end:
    CloseHandle(file);
    arena_temp_release(tmp);
    return result;
}


static bool file_copy_opt(Str from, Str to, FileOpt opt) {
    bool result = true;
    Temp tmp = arena_temp();

    const char *from_cstr = str_to_cstr(tmp.arena, from);
    const char *to_cstr = str_to_cstr(tmp.arena, to);

    if (!CopyFileA(from_cstr, to_cstr, !opt.replace_existing)) {
        Str err_str = str_last_error(tmp.arena);
        migi_log(Log_Error, "Failed to copy file: '%.*s' => '%.*s': %.*s", SArg(from), SArg(to), SArg(err_str));
        result = false;
    }

    arena_temp_release(tmp);
    return result;
}

static bool file_move_opt(Str from, Str to, FileOpt opt) {
    bool result = true;
    Temp tmp = arena_temp();

    const char *from_cstr = str_to_cstr(tmp.arena, from);
    const char *to_cstr = str_to_cstr(tmp.arena, to);

    DWORD flags = MOVEFILE_COPY_ALLOWED|MOVEFILE_WRITE_THROUGH;
    if (opt.replace_existing) {
        flags |= MOVEFILE_REPLACE_EXISTING;
    }

    if (result && !MoveFileExA(from_cstr, to_cstr, flags)) {
        Str err_str = str_last_error(tmp.arena);
        migi_log(Log_Error, "Failed to move file: '%.*s' => '%.*s': %.*s", SArg(from), SArg(to), SArg(err_str));
        result = false;
    }

    arena_temp_release(tmp);
    return result;
}

static bool file__delete(const char *filepath) {
    return DeleteFileA(filepath);
}

static bool file_delete(Str filepath) {
    bool result = true;
    Temp tmp = arena_temp();
    if (!file__delete(str_to_cstr(tmp.arena, filepath))) {
        Str err_str = str_last_error(tmp.arena);
        migi_log(Log_Error, "Failed to delete file: '%.*s': %.*s", SArg(filepath), SArg(err_str));
        result = false;
    }
    arena_temp_release(tmp);
    return result;
}

static bool dir_make_if_not_exists(Str dirpath) {
    bool result = true;
    Temp tmp = arena_temp();

    if (!CreateDirectoryA(str_to_cstr(tmp.arena, dirpath), NULL)) {
        if (GetLastError() != ERROR_ALREADY_EXISTS) {
            Str err_str = str_last_error(tmp.arena);
            migi_log(Log_Error, "Failed to create directory: '%.*s': %.*s", SArg(dirpath), SArg(err_str));
            result = false;
        } else {
            Migi_log(Log_Info, "Directory: '%.*s' already exists", SArg(dirpath));
        }
    }

    arena_temp_release(tmp);
    return result;
}

static bool dir_copy(Str from, Str to) {
    bool result = false;
    Temp tmp = arena_temp();

    DirWalker walker = walker_init(from);

    if (!dir_make_if_not_exists(to)) goto end;

    bool error = false;
    dir_foreach(tmp.arena, &walker, file) {
        if (file.error) {
            migi_log(Log_Error, "Failed to copy file: '%.*s': ", SArg(file.path));
            error = true;
        }

        Str sub_file_path = str_skip(file.path, from.length);
        if (file.is_dir) {
            Str dest = strf(tmp.arena, "%.*s/%.*s", SArg(to), SArg(sub_file_path));
            if (!dir_make_if_not_exists(dest)) goto end;
        } else {
            Str source = strf(tmp.arena, "%.*s", SArg(file.path));
            Str dest = strf(tmp.arena, "%.*s/%.*s", SArg(to), SArg(sub_file_path));
            if (!file_copy_opt(source, dest, (FileOpt){0})) goto end;
        }
    }

    if (!error) result = true;
end:
    walker_free(&walker);
    arena_temp_release(tmp);
    return result;
}

static bool dir__delete_empty(const char *dirpath) {
    return RemoveDirectoryA(dirpath);
}

static Str get_cwd(Arena *a) {
    Str result = {0};
    Temp tmp = arena_temp_excl(a);

    int size = MAX_PATH;
    char *buf = arena_push(a, char, size);
    int ret = GetCurrentDirectory(size, buf);
    if (ret == 0) {
        Str err_str = str_last_error(tmp.arena);
        migi_log(Log_Error, "Failed to get working directory: %.*s", SArg(err_str));
        arena_pop(a, char, size);
        goto end;
    } else if (ret > size) {
        arena_pop(a, char, size);
        size = ret;
        arena_push(a, char, size);
        ret = GetCurrentDirectory(size, buf);

        // null terminator is not included in return value if it succeeds
        if (ret != size - 1) {
            Str err_str = str_last_error(tmp.arena);
            migi_log(Log_Error, "Failed to get working directory: %.*s", SArg(err_str));
            arena_pop(a, char, size);
            goto end;
        }
    } else if (ret < size) {
        // name requires less than 1024 characters, so its popped off
        arena_pop(a, char, size - ret);
    }

    size = ret;
    result = str_from(buf, size);

end:
    arena_temp_release(tmp);
    return result;
}

static bool set_cwd(Str cwd) {
    bool result = true;
    Temp tmp = arena_temp();
    if (!SetCurrentDirectoryA(str_to_cstr(tmp.arena, cwd))) {
        Str err_str = str_last_error(tmp.arena);
        migi_log(Log_Error, "Failed to set working directory to '%.*s': %.*s", SArg(cwd), SArg(err_str));
        result = false;
    }
    arena_temp_release(tmp);
    return result;
}

static Str get_executable_path(Arena *a) {
    Str result = {0};
    Temp tmp = arena_temp_excl(a);

    int size = MAX_PATH;
    char *buf = arena_push(a, char, size);
    int ret = GetModuleFileNameA(NULL, buf, size);
    if (ret == 0 || ret >= size) {
        // GetModuleFileNameA doesnt return the actually needed size
        // for the buffer, so this shit needs to be done here
        while (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            arena_pop(a, char, size);
            size *= 2;
            buf = arena_push(a, char, size);
            ret = GetModuleFileNameA(NULL, buf, size);
        }
        if (ret == 0) {
            Str err_str = str_last_error(tmp.arena);
            migi_log(Log_Error, "Failed to get executable path: %.*s", SArg(err_str));
            arena_pop(a, char, size);
            goto end;
        }
    }

    result = str_from_cstr(buf);
    arena_pop(a, char, size - result.length);

end:
    arena_temp_release(tmp);
    return result;
}

static Str get_cwd_executable(Arena *a) {
    Str result = get_executable_path(a);
    if (result.length != 0) {
        result = path_dirname(result, S("\\"));
    }
    return result;
}

#elif OS_LINUX

#include <sys/sendfile.h>
#include <sys/stat.h>
#include <limits.h>
#include <time.h>
#include <utime.h>
#include <fcntl.h>
#include <unistd.h>

#include "filepath.h"
#include "dir_walker.h"

static FileType file_type(Str filepath) {
    FileType result = {.error=true};
    Temp tmp = arena_temp();

    int fd = open(str_to_cstr(tmp.arena, filepath), O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        migi_log(Log_Error, "File: '%.*s' doesn't exist", SArg(filepath));
        goto end;
    }

    struct stat file_stat;
    if (fstat(fd, &file_stat) != 0) {
        migi_log(Log_Error, "Failed to stat file: '%.*s'", SArg(filepath));
        close(fd);
        goto end;
    }

    result.error              = false;
    result.is_directory       = S_ISDIR(file_stat.st_mode);
    result.is_symbolic_link   = S_ISLNK(file_stat.st_mode);

    close(fd);
end:
    arena_temp_release(tmp);
    return result;
}

static bool file__exists(const char *filepath) {
    int fd = access(filepath, F_OK);
    bool exists = fd == 0;
    close(fd);
    return exists;
}

static bool file_exists(Str filepath) {
    Temp tmp = arena_temp();
    bool exists = file__exists(str_to_cstr(tmp.arena, filepath));
    arena_temp_release(tmp);
    return exists;
}

static bool file_touch(Str filepath) {
    bool result = true;
    Temp tmp = arena_temp();

    const char *filepath_cstr = str_to_cstr(tmp.arena, filepath);
    int fd = open(filepath_cstr, O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    if (fd == -1) {
        migi_log(Log_Error, "Failed to touch file: '%.*s': %s", SArg(filepath), strerror(errno));
        goto_end_with(false);
    }
    close(fd);

    if (utime(filepath_cstr, NULL) == -1) {
        migi_log(Log_Error, "Failed to update file time: '%.*s': %s", SArg(filepath), strerror(errno));
        goto_end_with(false);
    }

end:
    arena_temp_release(tmp);
    return result;
}

static bool file__delete(const char *filepath) {
    return unlink(filepath) == 0;
}

static bool file_delete(Str filepath) {
    bool result = true;
    Temp tmp = arena_temp();
    if (!file__delete(str_to_cstr(tmp.arena, filepath))) {
        migi_log(Log_Error, "Failed to delete file: '%.*s': %s", SArg(filepath), strerror(errno));
        result = false;
    }
    arena_temp_release(tmp);
    return result;
}


static bool file__copy(const char *from, const char *to, bool replace_existing, int *from_fd, int *to_fd) {
    *from_fd = open(from, O_RDONLY);
    if (*from_fd == -1) {
        migi_log(Log_Error, "Failed to open file: '%s': %s", from, strerror(errno));
        return false;
    }

    if (!replace_existing && file__exists(to)) {
        migi_log(Log_Error, "destination file: '%s' already exists", to);
        close(*from_fd);
        return false;
    }

    // TODO: use the constants for the `mode` parameter rather than the number directly
    *to_fd = creat(to, 0660);
    if (*to_fd == -1) {
        migi_log(Log_Error, "Failed to create file: '%s': %s", to, strerror(errno));
        close(*from_fd);
        return false;
    }

    struct stat from_stat;
    if (fstat(*from_fd, &from_stat) != 0) {
        migi_log(Log_Error, "Failed to stat file: '%s': %s", from, strerror(errno));
        close(*from_fd);
        close(*to_fd);
        return false;
    }

    int64_t copied = 0;
    while (copied < from_stat.st_size) {
        ssize_t sent = sendfile(*to_fd, *from_fd, NULL, SSIZE_MAX);
        if (sent == -1) {
            migi_log(Log_Error, "Failed to copy file: '%s': %s", from, strerror(errno));
            close(*from_fd);
            close(*to_fd);
            return false;
        }
        copied += sent;
    }

    // TODO: also copy the file permissions
    return true;
}

static bool file_copy_opt(Str from, Str to, FileOpt opt) {
    Temp tmp = arena_temp();

    const char *from_cstr = str_to_cstr(tmp.arena, from);
    const char *to_cstr = str_to_cstr(tmp.arena, to);
    int from_fd, to_fd;

    if (!file__copy(from_cstr, to_cstr, opt.replace_existing, &from_fd, &to_fd)) {
        arena_temp_release(tmp);
        return false;
    }

    close(from_fd);
    close(to_fd);
    arena_temp_release(tmp);
    return true;
}

static bool file_move_opt(Str from, Str to, FileOpt opt) {
    bool result = true;
    Temp tmp = arena_temp();

    const char *from_cstr = str_to_cstr(tmp.arena, from);
    const char *to_cstr = str_to_cstr(tmp.arena, to);
    int from_fd, to_fd;

    if (!file__copy(from_cstr, to_cstr, opt.replace_existing, &from_fd, &to_fd)) {
        result = false;
        goto end;
    }

    if (!file__delete(from_cstr)) {
        migi_log(Log_Error, "Failed to delete source file: '%.*s': %s", SArg(from), strerror(errno));
        result = false;
    }

    close(from_fd);
    close(to_fd);
end:
    arena_temp_release(tmp);
    return result;
}

static bool dir_make_if_not_exists(Str dirpath) {
    bool result = true;
    Temp tmp = arena_temp();

    if (mkdir(str_to_cstr(tmp.arena, dirpath), 0700) < 0) {
        if (errno != EEXIST) {
            migi_log(Log_Error, "Failed to create directory: '%.*s': %s", SArg(dirpath), strerror(errno));
            result = false;
        } else {
            migi_log(Log_Info, "Directory: '%.*s' already exists", SArg(dirpath));
        }
    }

    arena_temp_release(tmp);
    return result;
}

static bool dir_copy(Str from, Str to) {
    bool result = false;
    Temp tmp = arena_temp();

    DirWalker walker = walker_init(from);

    if (!dir_make_if_not_exists(to)) goto end;

    bool error = false;
    dir_foreach(tmp.arena, &walker, file) {
        if (file.error) {
            migi_log(Log_Error, "Failed to copy file: '%.*s': ", SArg(file.path));
            error = true;
        }

        Str sub_file_path = str_skip(file.path, from.length);
        if (file.is_dir) {
            Str dest = strf(tmp.arena, "%.*s/%.*s", SArg(to), SArg(sub_file_path));
            if (!dir_make_if_not_exists(dest)) goto end;
        } else {
            Str source = strf(tmp.arena, "%.*s", SArg(file.path));
            Str dest = strf(tmp.arena, "%.*s/%.*s", SArg(to), SArg(sub_file_path));
            if (!file_copy_opt(source, dest, (FileOpt){0})) goto end;
        }
    }

    if (!error) result = true;
end:
    walker_free(&walker);
    arena_temp_release(tmp);
    return result;
}

static bool dir__delete_empty(const char *dirpath) {
    return rmdir(dirpath) == 0;
}


static Str get_cwd(Arena *a) {
    size_t size = PATH_MAX;
    char *buf = arena_push(a, char, size);
    char *ret = getcwd(buf, size);

    // getcwd doesnt return the actual needed size so this shit needs to be done here
    // NOTE: Since the arena may not be linear, simply doing another
    // push of the same size to double the previous allocation is not
    // completely correct. Thus a pop is needed.
    while (errno == ERANGE) {
        arena_pop(a, char, size);
        size *= 2;
        buf = arena_push(a, char, size);
        ret = getcwd(buf, size);
    }
    if (ret == NULL) {
        migi_log(Log_Error, "Failed to get working directory: %s", strerror(errno));
        arena_pop(a, char, size);
        return str_zero();
    }

    Str cwd = str_from_cstr(buf);
    arena_pop(a, char, size - cwd.length);
    return cwd;
}

static bool set_cwd(Str cwd) {
    bool result = true;
    Temp tmp = arena_temp();
    if (chdir(str_to_cstr(tmp.arena, cwd)) == -1) {
        migi_log(Log_Error, "Failed to set working directory to '%.*s': %s", SArg(cwd), strerror(errno));
        result = false;
    }
    arena_temp_release(tmp);
    return result;
}

static Str get_executable_path(Arena *a) {
    ssize_t size = PATH_MAX;
    char *buf = arena_push(a, char, size);
    ssize_t n = readlink("/proc/self/exe", buf, size);

    if (n == -1) goto err;

    // getcwd doesnt return the actual needed size so this shit needs to be done here
    // NOTE: Since the arena may not be linear, simply doing another
    // push of the same size to double the previous allocation is not
    // completely correct. Thus a pop is needed.
    while (n >= size) {
        arena_pop(a, char, size);
        size *= 2;
        buf = arena_push(a, char, size);
        n = readlink("/proc/self/exe", buf, size);

        if (n == -1) goto err;
    }

    Str cwd = str_from(buf, n);
    arena_pop(a, char, size - cwd.length);
    return cwd;

err:
    migi_log(Log_Error, "Failed to get executable path: %s", strerror(errno));
    arena_pop(a, char, size);
    return str_zero();
}

static Str get_cwd_executable(Arena *a) {
    Str result = get_executable_path(a);
    if (result.length != 0) {
        result = path_dirname(result, S("/"));
    }
    return result;
}

#else
    #error Unsupported OS
#endif // if OS_WINDOWS


typedef struct FS__PathNode FS__PathNode;
struct FS__PathNode {
    Str path;
    uint32_t depth;
    FS__PathNode *next;
};

static bool dir_move(Str from, Str to) {
    bool result = false;
    Temp tmp = arena_temp();

    FS__PathNode *dirs_to_delete = NULL;
    {
        FS__PathNode *node = arena_new(tmp.arena, FS__PathNode);
        node->path = from;
        node->depth = 1;
        stack_push(dirs_to_delete, node);
    }

    DirWalker walker = walker_init(from);

    if (!dir_make_if_not_exists(to)) goto end;

    bool error = false;
    dir_foreach(tmp.arena, &walker, file) {
        if (file.error) {
            migi_log(Log_Error, "failed to move file: '%.*s': ", SArg(file.path));
            error = true;
        }

        Str sub_file_path = str_skip(file.path, from.length);
        if (file.is_dir) {
            FS__PathNode *node = arena_new(tmp.arena, FS__PathNode);
            node->path = strf(tmp.arena, "%.*s", SArg(file.path));
            node->depth = file.depth;
            stack_push(dirs_to_delete, node);

            Str dest = strf(tmp.arena, "%.*s/%.*s", SArg(to), SArg(sub_file_path));
            if (!dir_make_if_not_exists(dest)) goto end;
        } else {
            Str source = strf(tmp.arena, "%.*s", SArg(file.path));
            Str dest = strf(tmp.arena, "%.*s/%.*s", SArg(to), SArg(sub_file_path));
            file_move_opt(source, dest, (FileOpt){.replace_existing=true});
        }
    }

    // Since `dirs_to_delete` is a stack, it can simply be traversed top to bottom
    // and the directories are deleted in the descending order of their depth
    list_foreach(dirs_to_delete, dir) {
        if (!dir__delete_empty(str_to_cstr(tmp.arena, dir->path))) {
            Str err_str = str_last_error(tmp.arena);
            migi_log(Log_Error, "failed to delete directory: '%.*s': %.*s", SArg(dir->path), SArg(err_str));
            goto end;
        }
    }

    result = !error;
end:
    walker_free(&walker);
    arena_temp_release(tmp);
    return result;
}


static bool dir_delete_opt(Str root_path, DirDeleteOpt opt) {
    bool result = false;
    Temp tmp = arena_temp();

    if (!opt.recursive) {
        if (dir__delete_empty(str_to_cstr(tmp.arena, root_path))) {
            result = true;
        } else {
            Str err_str = str_last_error(tmp.arena);
            migi_log(Log_Error, "failed to delete directory: '%.*s': %.*s", SArg(root_path), SArg(err_str));
        }
        arena_temp_release(tmp);
        return result;
    }

    FS__PathNode *dirs_to_delete = NULL;
    {
        FS__PathNode *node = arena_new(tmp.arena, FS__PathNode);
        node->path = root_path;
        node->depth = 1;
        stack_push(dirs_to_delete, node);
    }

    DirWalker walker = walker_init(root_path);
    dir_foreach(tmp.arena, &walker, file) {
        if (file.error) {
            migi_log(Log_Error, "failed to delete file: '%.*s': ", SArg(file.path));
            goto end;
        }

        if (file.is_dir) {
            FS__PathNode *node = arena_new(tmp.arena, FS__PathNode);
            node->path = strf(tmp.arena, "%.*s", SArg(file.path));
            node->depth = file.depth;
            stack_push(dirs_to_delete, node);
        } else {
            if (!file__delete(str_to_cstr(tmp.arena, file.path))) {
                Str err_str = str_last_error(tmp.arena);
                migi_log(Log_Error, "failed to delete file: '%.*s': %.*s", SArg(file.path), SArg(err_str));
                goto end;
            }
        }
    }

    // Since `dirs_to_delete` is a stack, it can simply be traversed top to bottom
    // and the directories are deleted in the descending order of their depth
    list_foreach(dirs_to_delete, dir) {
        if (!dir__delete_empty(str_to_cstr(tmp.arena, dir->path))) {
            Str err_str = str_last_error(tmp.arena);
            migi_log(Log_Error, "failed to delete directory: '%.*s': %.*s", SArg(dir->path), SArg(err_str));
            goto end;
        }
    }

    result = true;
end:
    walker_free(&walker);
    arena_temp_release(tmp);
    return result;
}



#endif // ifndef MIGI_FILESYSTEM_H
