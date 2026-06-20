#ifndef FILESYSTEM_LINUX_H
#define FILESYSTEM_LINUX_H

#include <sys/sendfile.h>
#include <sys/stat.h>
#include <limits.h>

#include "migi_core.h"
#include "filesystem/filesystem_inc.h"

FileType file_type(Str filepath) {
    FileType result = {0};
    Temp tmp = arena_temp();

    int fd = open(str_to_cstr(tmp.arena, filepath), O_CREAT | S_IRUSR | S_IWUSR);
    if (fd == -1) {
        migi_log(Log_Error, "file: '%.*s' doesn't exist", SArg(filepath));
        return_with((FileType){.error = true});
    }

    struct stat file_stat;
    if (fstat(fd, &file_stat) != 0) {
        migi_log(Log_Error, "failed to stat file: '%.*s'", SArg(filepath));
        return_with((FileType){.error = true});
    }

    result.is_directory       = S_ISDIR(file_stat.st_mode);
    result.is_symbolic_link   = S_ISLNK(file_stat.st_mode);

    close(fd);
end:
    arena_temp_release(tmp);
    return result;
}

bool file__exists(const char *filepath) {
    Temp tmp = arena_temp();
    int fd = access(filepath, F_OK);
    bool exists = fd == 0;
    close(fd);
    arena_temp_release(tmp);
    return exists;
}

bool file_exists(Str filepath) {
    Temp tmp = arena_temp();
    bool exists = file__exists(str_to_cstr(tmp.arena, filepath));
    arena_temp_release(tmp);
    return exists;
}

bool file_touch(Str filepath) {
    bool result = true;
    Temp tmp = arena_temp();

    int fd = open(str_to_cstr(tmp.arena, filepath), O_CREAT | S_IRUSR | S_IWUSR);

    if (fd == -1) {
        migi_log(Log_Error, "failed to touch file: '%.*s': %s", SArg(filepath), strerror(errno));
        return_with(false);
    }

    close(fd);

end:
    arena_temp_release(tmp);
    return result;
}

bool file__copy(const char *from, const char *to, bool replace_existing, int *from_fd, int *to_fd) {
    *from_fd = open(from, O_RDONLY);
    if (*from_fd == -1) {
        migi_log(Log_Error, "failed to open file: '%s': %s", from, strerror(errno));
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
        migi_log(Log_Error, "failed to create file: '%s': %s", to, strerror(errno));
        close(*from_fd);
        return false;
    }

    struct stat from_stat;
    if (fstat(*from_fd, &from_stat) != 0) {
        migi_log(Log_Error, "failed to stat file: '%s': %s", from, strerror(errno));
        close(*from_fd);
        close(*to_fd);
        return false;
    }

    int64_t copied = 0;
    while (copied < from_stat.st_size) {
        ssize_t sent = sendfile(*to_fd, *from_fd, NULL, SSIZE_MAX);
        if (sent == -1) {
            migi_log(Log_Error, "failed to copy file: '%s': %s", from, strerror(errno));
            close(*from_fd);
            close(*to_fd);
            return false;
        }
        copied += sent;
    }
    return true;
}

bool file_copy_opt(Str from, Str to, FileOpt opt) {
    Temp tmp = arena_temp();

    const char *from_cstr = str_to_cstr(tmp.arena, from);
    const char *to_cstr = str_to_cstr(tmp.arena, to);
    int from_fd, to_fd;

    if (!file__copy(from_cstr, to_cstr, opt.replace_exisiting, &from_fd, &to_fd)) {
        arena_temp_release(tmp);
        return false;
    }

    close(from_fd);
    close(to_fd);
    arena_temp_release(tmp);
    return true;
}

bool file_move_opt(Str from, Str to, FileOpt opt) {
    bool result = true;
    Temp tmp = arena_temp();

    const char *from_cstr = str_to_cstr(tmp.arena, from);
    const char *to_cstr = str_to_cstr(tmp.arena, to);
    int from_fd, to_fd;

    if (!file__copy(from_cstr, to_cstr, opt.replace_exisiting, &from_fd, &to_fd)) {
        result = false;
        goto end;
    }

    if (remove(from_cstr) != 0) {
        migi_log(Log_Error, "failed to delete source file: '%.*s': %s", SArg(from), strerror(errno));
        result = false;
    }

    close(from_fd);
    close(to_fd);
end:
    arena_temp_release(tmp);
    return result;
}

bool file_delete(Str filepath) {
    bool result = true;
    Temp tmp = arena_temp();
    if (remove(str_to_cstr(tmp.arena, filepath)) != 0) {
        migi_log(Log_Error, "failed to delete file: '%.*s': %s", SArg(filepath), strerror(errno));
        result = false;
    }
    arena_temp_release(tmp);
    return result;
}

bool dir_make_if_not_exists(Str dirpath) {
    bool result = true;
    Temp tmp = arena_temp();

    if (mkdir(str_to_cstr(tmp.arena, dirpath), 0700) < 0) {
        if (errno != EEXIST) {
            migi_log(Log_Error, "failed to create directory: '%.*s': %s", SArg(dirpath), strerror(errno));
            result = false;
        } else {
            migi_log(Log_Info, "directory: '%.*s' already exists", SArg(dirpath));
        }
    }

    arena_temp_release(tmp);
    return result;
}

bool dir_copy(Str from, Str to) { todo(); }
bool dir_move(Str from, Str to) { todo(); }
bool dir_delete_opt(Str filepath, DirDeleteOpt opt) { todo(); }

Str get_cwd(Arena *arena) { todo(); }
Str get_cwd_executable(Arena *arena) { todo(); }
Str get_executable_path(Arena *a) { todo(); }

#endif // #ifndef FILESYSTEM_LINUX_H
