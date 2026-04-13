#ifndef MIGI_FILE_H
#define MIGI_FILE_H

#include "migi_core.h"
#include "migi_string.h"

// TODO: split into file_win32 and file_posix rather than the #ifdef hell

#ifdef _WIN32
    typedef HANDLE File;
    #define FILE_ERROR INVALID_HANDLE_VALUE
#else
    typedef int File;
    #define FILE_ERROR -1
#endif

typedef struct {
    bool read;
    bool write;
    bool append;

    bool dont_truncate;
} FileOpenOpt;

File file_open_opt(Str filepath, FileOpenOpt opt);
#define file_open(filepath, ...) file_open_opt((filepath), (FileOpenOpt){ __VA_ARGS__ })
static bool file_close(File file);

// Gets the last error from the OS as a string
// NOTE: `arena` is currently unused on linux as it just calls `strerror` which uses its own memory
static Str str_last_error(Arena *arena);

typedef struct {
    Str string;
    bool ok;
} StrResult;

// NOTE: These are lower level functions that do not log the error
// Either use `str_from/to_file` instead or manually call `str_last_error`
static StrResult file_read(Arena *arena, File file);
static bool file_write(File file, Str str);

static Str str_from_file(Arena *arena, Str filepath);
static bool str_to_file(Str string, Str filepath);





// TODO: check if the sharing mode parameter of CreateFileA should be set
// (for example if a file is opened in read mode, it may be SHARED_READ)
File file_open_opt(Str filepath, FileOpenOpt opt) {
    Temp tmp = arena_temp();

    const char *filename_cstr = str_to_cstr(tmp.arena, filepath);
    File file = FILE_ERROR;

    if (opt.append) {
        if (opt.write || opt.read) {
            migi_log(Log_Error, "opening mode cannot simultaneously be read/write and append");
            return file;
        }
        opt.write = true;
    }

    // If read == false, default to read mode
    if (!opt.write) {
#ifdef _WIN32
        file = CreateFileA(filename_cstr, 
            GENERIC_READ, 0, NULL, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, NULL);
#else
        file = open(filename_cstr, O_RDONLY);
#endif
    } else {
#ifdef _WIN32
        DWORD create_mode = 0;
        if (opt.append || opt.dont_truncate) {
            create_mode = OPEN_ALWAYS;
        } else {
            create_mode = CREATE_ALWAYS;
        }

        DWORD open_mode = GENERIC_WRITE;
        if (opt.read) open_mode |= GENERIC_READ;

        file = CreateFileA(filename_cstr,
            open_mode, 0, NULL, create_mode,
            FILE_ATTRIBUTE_NORMAL, NULL);
#else
        todof("implement other features");
        file = open(str_to_cstr(tmp.arena, filepath),
                  O_WRONLY|O_CREAT|O_TRUNC,
                  S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
#endif
    }
    if (file == FILE_ERROR) {
        migi_log(Log_Error, "failed to open file `%.*s`: %.*s",
                SArg(filepath), SArg(str_last_error(tmp.arena)));
        file = FILE_ERROR;
    }

    if (file != FILE_ERROR && opt.append) {
#ifdef _WIN32
        DWORD res = SetFilePointer(file, 0, NULL, FILE_END);
        if (res == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) {
            migi_log(Log_Error, "failed to open file `%.*s` in append mode: %.*s",
                    SArg(filepath), SArg(str_last_error(tmp.arena)));
            CloseHandle(file);
            file = FILE_ERROR;
        }
#else
        todof("implement append mode");
#endif
    }

    arena_temp_release(tmp);
    return file;
}

static StrResult file_read(Arena *arena, File file) {
    StrResult result = {0};
#ifdef _WIN32
    DWORD size_high = 0;
    DWORD size_low = GetFileSize(file, &size_high);
    LARGE_INTEGER filesize = {
        .LowPart = size_low,
        .HighPart = size_high
    };
    // file position cannot be negative at this point
    size_t length = filesize.QuadPart;
    char *buf = arena_push(arena, char, length);

    char *buf_start = buf;
    char *buf_end = buf_start + length;
    while (buf < buf_end) {
        DWORD n = 0;
        if (!ReadFile(file, buf, (DWORD)length, &n, NULL)) {
            arena_pop(arena, char, length);
            return result;
        }
        buf += n;
    }
#else
    off_t length = lseek(fd, 0, SEEK_END);
    if (length == -1) {
        return (Str){0};
    }
    lseek(fd, 0, SEEK_SET);

    // file position cannot be negative at this point
    char *buf = arena_push(arena, char, length);

    ssize_t n = 0;
    char *buf_at = buf;
    while (n < length) {
        ssize_t m = read(fd, buf_at, length);
        if (m == -1) {
            arena_pop(arena, char, length);
            return result;
        }
        n += m;
        buf_at += m;
    }
#endif // #ifndef _WIN32
    result = (StrResult){
        .string.data = buf_start,
        .string.length = length,
        .ok = true
    };
    return result;
}


static bool file_write(File file, Str str) {
#ifdef _WIN32
    while (str.length > 0) {
        DWORD n = 0;
        if (!WriteFile(file, str.data, (DWORD)str.length, &n, NULL)) {
            return false;
        }
        str = str_skip(str, n);
    }
    return true;
#else
    while (str.length > 0) {
        ssize_t n = write(fd, str.data, str.length);
        if (n == -1) {
            return false;
        }
        str = str_skip(str, n);
    }
    return true;
#endif // #ifndef _WIN32
}


static bool file_close(File file) {
#ifdef _WIN32
    return CloseHandle(file);
#else
    // TODO: can close return an error?
    close(fd);
    return true;
#endif // ifdef _WIN32
}

static Str str_last_error(Arena *arena) {
#ifdef _WIN32
    DWORD err = GetLastError();

    DWORD max_length = 64*KB - 1;
    char *buf = arena_push_nonzero(arena, char, max_length);
    DWORD len = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        buf, max_length, NULL);

    arena_pop(arena, char, max_length - len - 2);

    Str err_string = (Str){
        .data = buf,
        .length = len
    };

    // remove `\r\n` from end
    if (str_ends_with(err_string, S("\r\n"))) {
        err_string = str_drop(err_string, 2);
    }
    return err_string;
#else
    unused(arena);
    const char *s = strerror(errno);
    return str_from_cstr(s);
#endif
}


// TODO: passing in a directory as filepath causes ftell to return LONG_MAX which overflows the arena
static Str str_from_file(Arena *arena, Str filepath) {
    Str str = {0};
    Temp tmp = arena_temp_excl(arena);

    File file = file_open(filepath);
    if (file == FILE_ERROR) {
        arena_temp_release(tmp);
        return str;
    }

    StrResult result = file_read(arena, file);
    if (!result.ok) {
        migi_log(Log_Error, "failed to read from file `%.*s`: %.*s",
                SArg(filepath), SArg(str_last_error(tmp.arena)));
    }
    str = result.string;

    file_close(file);
    arena_temp_release(tmp);
    return str;
}


static bool str_to_file(Str string, Str filepath) {
    Temp tmp = arena_temp();
    File file = file_open(filepath, .write=true);
    if (file == FILE_ERROR) {
        arena_temp_release(tmp);
        return false;
    }

    bool ok = file_write(file, string);
    if (!ok) {
        migi_log(Log_Error, "failed to write to file `%.*s`: %.*s", 
                SArg(filepath), SArg(str_last_error(tmp.arena)));
    }

    file_close(file);
    arena_temp_release(tmp);
    return ok;
}

#endif // ifndef MIGI_FILE_H
