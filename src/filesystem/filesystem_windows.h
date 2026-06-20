#ifndef FILESYSTEM_WINDOWS_H
#define FILESYSTEM_WINDOWS_H

// TODO: implement this

#include "filesystem/filesystem_inc.h"
#include "filepath.h"

// TODO: double check the logic
Str get_cwd(Arena *a) {
    int len = 1024;
    char *buf = arena_push(a, char, len);
    int res = GetCurrentDirectory(len, buf);
    if (res == 0) {
        todof("handle error");
    } else if (res > len) {
        int actual_size = res;
        arena_push(a, char, actual_size - len);
        // TODO: currently doesnt seem to resize the allocation correctly if the 1st buffer is not large enough
        res = GetCurrentDirectory(actual_size, buf);
        if (res != actual_size) {
            todof("handle error");
        }
        len = actual_size;
    } else {
        // name requires less than 1024 characters, so its popped off
        arena_pop(a, char, len - res);
    }

    return (Str){
        .data   = buf,
        .length = len - 1,  // remove the null terminator
    };
}

Str get_executable_path(Arena *a) {
    int len = 1024;
    char *buf = arena_push(a, char, len);
    int res = GetModuleFileNameA(NULL, buf, len);
    if (res < len) {
        arena_pop(a, char, len - res);
    } else {
        todof("handle error");
        // windows doesnt even seem to return the actual size required for the buffer
        // so nothing can really be done to handle it
    }
    return (Str){
        .data = buf,
        .length = len
    };
}

Str get_cwd_executable(Arena *a) {
    Str result = get_executable_path(a);
    if (result.length != 0) {
        result = path_dirname(result, S("\\"));
    }
    return result;
}

#endif // ifndef FILESYSTEM_WINDOWS_H
