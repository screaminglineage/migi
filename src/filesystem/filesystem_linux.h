#ifndef FILESYSTEM_LINUX_H
#define FILESYSTEM_LINUX_H

#include <sys/sendfile.h>
#include <sys/stat.h>
#include <limits.h>
#include <unistd.h>

#include "migi_core.h"
#include "filesystem/filesystem_inc.h"
#include "filepath.h"

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
    int fd = access(filepath, F_OK);
    bool exists = fd == 0;
    close(fd);
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


Str get_cwd(Arena *a) {
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
        migi_log(Log_Error, "failed to get working directory: %s", strerror(errno));
        arena_pop(a, char, size);
        return (Str){0};
    }

    Str cwd = str_from_cstr(buf);
    arena_pop(a, char, size - cwd.length);
    return cwd;
}

bool set_cwd(Str cwd) {
    bool result = true;
    Temp tmp = arena_temp();
    if (chdir(str_to_cstr(tmp.arena, cwd)) == -1) {
        migi_log(Log_Error, "failed to set working directory to '%.*s': %s", SArg(cwd), strerror(errno));
        result = false;
    }
    arena_temp_release(tmp);
    return result;
}

Str get_executable_path(Arena *a) {
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

    Str cwd = (Str){
        .data = buf,
        .length = n
    };
    arena_pop(a, char, size - cwd.length);
    return cwd;

err:
    migi_log(Log_Error, "failed to get executable path: %s", strerror(errno));
    arena_pop(a, char, size);
    return (Str){0};
}

Str get_cwd_executable(Arena *a) {
    Str result = get_executable_path(a);
    if (result.length != 0) {
        result = path_dirname(result, S("/"));
    }
    return result;
}


#endif // #ifndef FILESYSTEM_LINUX_H
