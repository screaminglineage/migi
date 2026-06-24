#ifndef FILESYSTEM_WINDOWS_H
#define FILESYSTEM_WINDOWS_H

#include "filesystem/filesystem_interface.h"
#include "filepath.h"
#include "file.h"
#include "dir_walker.h"

static FileType file_type(Str filepath) {
    FileType result = {0};
    Temp tmp = arena_temp();

    int attrs = GetFileAttributes(str_to_cstr(tmp.arena, filepath));
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        Str err_str = str_last_error(tmp.arena);
        migi_log(Log_Error, "failed to get file type for: '%.*s': %.*s", SArg(filepath), SArg(err_str));
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
        migi_log(Log_Error, "failed to touch file: '%.*s': %.*s", SArg(filepath), SArg(err_str));
        return_with(false);
    }

    SYSTEMTIME sys_time;
    GetSystemTime(&sys_time);

    FILETIME file_time;
    if (!SystemTimeToFileTime(&sys_time, &file_time)) {
        Str err_str = str_last_error(tmp.arena);
        migi_log(Log_Error, "failed to convert system time to file time: '%.*s': %.*s", SArg(filepath), SArg(err_str));
        return_with(false);
    }

    if (!SetFileTime(file, &file_time, &file_time, &file_time)) {
        Str err_str = str_last_error(tmp.arena);
        migi_log(Log_Error, "failed to set file time: '%.*s': %.*s", SArg(filepath), SArg(err_str));
        return_with(false);
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
        migi_log(Log_Error, "failed to copy file: '%.*s' => '%.*s': %.*s", SArg(from), SArg(to), SArg(err_str));
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
        migi_log(Log_Error, "failed to move file: '%.*s' => '%.*s': %.*s", SArg(from), SArg(to), SArg(err_str));
        result = false;
    }

    arena_temp_release(tmp);
    return result;
}


static bool file_delete(Str filepath) {
    bool result = true;
    Temp tmp = arena_temp();
    if (!DeleteFileA(str_to_cstr(tmp.arena, filepath))) {
        Str err_str = str_last_error(tmp.arena);
        migi_log(Log_Error, "failed to delete file: '%.*s': %.*s", SArg(filepath), SArg(err_str));
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
            migi_log(Log_Error, "failed to create directory: '%.*s': %.*s", SArg(dirpath), SArg(err_str));
            result = false;
        } else {
            migi_log(Log_Info, "directory: '%.*s' already exists", SArg(dirpath));
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
            migi_log(Log_Error, "failed to copy file: '%.*s': ", SArg(file.path));
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
    FS__PathNode *node = arena_new(tmp.arena, FS__PathNode);
    node->path = from;
    node->depth = 1;
    stack_push(dirs_to_delete, node);

    // TODO: maybe reset the temp arena some time in the loop
    DirWalker walker = walker_init(from);

    if (!dir_make_if_not_exists(to)) goto end;

    bool error = false;
    uint32_t prev_depth = 0;
    dir_foreach(tmp.arena, &walker, file) {
        if (file.error) {
            migi_log(Log_Error, "failed to move file: '%.*s': ", SArg(file.path));
            error = true;
        }
        if (file.depth < prev_depth) {
            FS__PathNode *prev = dirs_to_delete;
            for (FS__PathNode *dir = dirs_to_delete; dir; prev = dir, dir = dir->next) {
                if (dir->depth >= file.depth) continue;
                if (!RemoveDirectoryA(str_to_cstr(tmp.arena, dir->path))) {
                    Str err_str = str_last_error(tmp.arena);
                    migi_log(Log_Error, "failed to delete directory: '%.*s': %.*s", SArg(dir->path), SArg(err_str));
                    goto end;
                }
                prev->next = dir->next;
                dir = prev;
            }
        }
        prev_depth = file.depth;

        Str sub_file_path = str_skip(file.path, from.length);
        if (file.is_dir) {
            FS__PathNode *dir = arena_new(tmp.arena, FS__PathNode);
            dir->path = strf(tmp.arena, "./%.*s", SArg(file.path));
            dir->depth = file.depth;
            stack_push(dirs_to_delete, dir);

            Str dest = strf(tmp.arena, "%.*s/%.*s", SArg(to), SArg(sub_file_path));
            if (!dir_make_if_not_exists(dest)) goto end;
        } else {
            Str source = strf(tmp.arena, "%.*s", SArg(file.path));
            Str dest = strf(tmp.arena, "%.*s/%.*s", SArg(to), SArg(sub_file_path));
            file_move_opt(source, dest, (FileOpt){.replace_existing=true});
        }
    }

    list_foreach(dirs_to_delete, dir) {
        if (!RemoveDirectoryA(str_to_cstr(tmp.arena, dir->path))) {
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
        if (RemoveDirectoryA(str_to_cstr(tmp.arena, root_path))) {
            result = true;
        } else {
            Str err_str = str_last_error(tmp.arena);
            migi_log(Log_Error, "failed to delete directory: '%.*s': %.*s", SArg(root_path), SArg(err_str));
        }
        arena_temp_release(tmp);
        return result;
    }

    FS__PathNode *dirs_to_delete = NULL;
    FS__PathNode *node = arena_new(tmp.arena, FS__PathNode);
    node->path = root_path;
    node->depth = 1;
    stack_push(dirs_to_delete, node);

    // TODO: maybe reset the temp arena some time in the loop
    DirWalker walker = walker_init(root_path);
    uint32_t prev_depth = 0;
    dir_foreach(tmp.arena, &walker, file) {
        if (file.error) {
            migi_log(Log_Error, "failed to delete file: '%.*s': ", SArg(file.path));
            goto end;
        }
        if (file.depth < prev_depth) {
            FS__PathNode *prev = dirs_to_delete;
            for (FS__PathNode *dir = dirs_to_delete; dir; prev = dir, dir = dir->next) {
                if (dir->depth >= file.depth) continue;
                if (RemoveDirectoryA(str_to_cstr(tmp.arena, dir->path)) == -1) {
                    Str err_str = str_last_error(tmp.arena);
                    migi_log(Log_Error, "failed to delete directory: '%.*s': %.*s", SArg(dir->path), SArg(err_str));
                    goto end;
                }
                prev->next = dir->next;
                dir = prev;
            }
        }
        prev_depth = file.depth;

        if (file.is_dir) {
            FS__PathNode *dir = arena_new(tmp.arena, FS__PathNode);
            dir->path = strf(tmp.arena, "./%.*s", SArg(file.path));
            dir->depth = file.depth;
            stack_push(dirs_to_delete, dir);
        } else {
            if (!DeleteFileA(str_to_cstr(tmp.arena, file.path))) {
                Str err_str = str_last_error(tmp.arena);
                migi_log(Log_Error, "failed to delete file: '%.*s': %.*s", SArg(file.path), SArg(err_str));
                goto end;
            }
        }
    }

    list_foreach(dirs_to_delete, dir) {
        if (!RemoveDirectoryA(str_to_cstr(tmp.arena, dir->path))) {
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

static Str get_cwd(Arena *a) {
    Str result = {0};
    Temp tmp = arena_temp_excl(a);

    int size = MAX_PATH;
    char *buf = arena_push(a, char, size);
    int ret = GetCurrentDirectory(size, buf);
    if (ret == 0) {
        Str err_str = str_last_error(tmp.arena);
        migi_log(Log_Error, "failed to get working directory: %.*s", SArg(err_str));
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
            migi_log(Log_Error, "failed to get working directory: %.*s", SArg(err_str));
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
        migi_log(Log_Error, "failed to set working directory to '%.*s': %.*s", SArg(cwd), SArg(err_str));
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
            migi_log(Log_Error, "failed to get executable path: %.*s", SArg(err_str));
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

#endif // ifndef FILESYSTEM_WINDOWS_H
