#ifndef FILESYSTEM_WINDOWS_H
#define FILESYSTEM_WINDOWS_H

// TODO: implement this

#include "filesystem/filesystem_interface.h"
#include "filepath.h"
#include "file.h"


static bool dir_copy(Str from, Str to)                     { unused(from),     unused(to);  return todo_expr(bool); }
static bool dir_move(Str from, Str to)                     { unused(from),     unused(to);  return todo_expr(bool); }
static bool dir_delete_opt(Str filepath, DirDeleteOpt opt) { unused(filepath), unused(opt); return todo_expr(bool); }

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
