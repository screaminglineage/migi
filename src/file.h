#ifndef MIGI_FILE_H
#define MIGI_FILE_H

#include "migi_core.h"
#include "migi_list.h"
#include "string_builder.h"

#if OS_WINDOWS
#include <windows.h>
#else
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#endif

// TODO: Divide the functions with separate implementations (like filesystem.h)
// rather than using #if's inside the functions
// TODO: complete the parts with todo()'s;

#if OS_WINDOWS
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

static int64_t file_length(File file);
static int64_t file_pos(File file);
static bool file_set_pos(File file, size_t new_pos);

// NOTE: These are lower level functions that do not log the error
// Either use `str_from/to_file` instead or manually call `str_last_error`

// Returns `true` if reading is complete (either due to an error or consuming the entire file)
static bool file_read(File file, char *buffer, size_t length, int64_t *actual_read);

static StrResult file_read_all(Arena *arena, File file);
static bool file_write_all(File file, Str str);


static Str str_from_file(Arena *arena, Str filepath);
static bool str_to_file(Str string, Str filepath);
static bool strlist_to_file(StrList list, Str filepath);

// Only defined if string_builder was also included
#ifdef MIGI_STRING_BUILDER_H

static void sb_push_file(StrBuilder *sb, Str filename);
static bool sb_to_file_opt(StrBuilder *sb, Str filename, StrBuilderOpt opt);
#define sb_to_file(sb, filename, ...) sb_to_file_opt((sb), (filename), (StrBuilderOpt){__VA_ARGS__})

#endif




static int64_t file_length(File file) {
#if OS_WINDOWS
    DWORD size_high = 0;
    DWORD size_low = GetFileSize(file, &size_high);
    // TODO: check if GetFileSize can return an error
    LARGE_INTEGER filesize = {
        .LowPart = size_low,
        .HighPart = size_high
    };
    return filesize.QuadPart;
#else
    off_t length = lseek(file, 0, SEEK_END);
    if (length != -1) {
        lseek(file, 0, SEEK_SET);
    }
    return length;
#endif // #if OS_WINDOWS
}

static int64_t file_pos(File file) {
#if OS_WINDOWS
    unused(file);
    todof("get current file position");
#else
    return lseek(file, 0, SEEK_CUR);
#endif // #if OS_WINDOWS
}

static bool file_set_pos(File file, size_t new_pos) {
#if OS_WINDOWS
    unused(file);
    unused(new_pos);
    todof("set file position");
#else
    return lseek(file, new_pos, SEEK_SET) != -1;
#endif // #if OS_WINDOWS
}



// TODO: instead of assuming read to be the default even if it is unset
// make its default value true in the macro to prevent this confusion entirely
// TODO: check if the sharing mode parameter of CreateFileA should be set
// (for example if a file is opened in read mode, it may be SHARED_READ)
File file_open_opt(Str filepath, FileOpenOpt opt) {
    Temp tmp = arena_temp();

    const char *filename_cstr = str_to_cstr(tmp.arena, filepath);
    File file = FILE_ERROR;

    if (opt.append) {
        if (opt.write || opt.read) {
            migi_log_with_ctx(Log_Error, "opening mode cannot simultaneously be read/write and append");
            return file;
        }
        opt.write = true;
    }

    // If read == false, default to read mode
    if (!opt.write) {
#if OS_WINDOWS
        file = CreateFileA(filename_cstr,
            GENERIC_READ, 0, NULL, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, NULL);
#else
        file = open(filename_cstr, O_RDONLY);
#endif
    } else {
#if OS_WINDOWS
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
        int open_flags = 0;
        if (opt.write  || opt.append)                                    open_flags |= O_CREAT;
        if (opt.write && !opt.read && !opt.append && !opt.dont_truncate) open_flags |= O_TRUNC;

        int access_mode = O_RDONLY;
        if (opt.read && opt.write) {
            access_mode = O_RDWR;
        } else if (!opt.read && opt.write) {
            access_mode = O_WRONLY;
        } else if (!opt.read && !opt.write && opt.append) {
            // for append
            access_mode = O_WRONLY;
        }
        open_flags |= access_mode;

        file = open(str_to_cstr(tmp.arena, filepath),
                  open_flags, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
#endif
    }
    if (file == FILE_ERROR) {
        migi_log(Log_Error, "Failed to open file '%.*s': %.*s",
                SArg(filepath), SArg(str_last_error(tmp.arena)));
        file = FILE_ERROR;
    }

    if (file != FILE_ERROR && opt.append) {
#if OS_WINDOWS
        DWORD res = SetFilePointer(file, 0, NULL, FILE_END);
        if (res == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) {
            migi_log(Log_Error, "Failed to open file '%.*s' in append mode: %.*s",
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

static bool file_read(File file, char *buffer, size_t length, int64_t *actual_read) {
    char *buf_start = buffer;
    char *buf_end   = buf_start + length;

    bool complete = false;
#if OS_WINDOWS
    while (buffer < buf_end) {
        DWORD n = 0;
        // TODO: check if read is complete on windows
        if (!ReadFile(file, buffer, (DWORD)length, &n, NULL)) {
            complete = true;
            goto end;
        }
        buffer += n;
    }
#else
    while (buffer < buf_end) {
        ssize_t n = read(file, buffer, length);
        if (n <= 0) {
            complete = true;
            goto end;
        }
        buffer += n;
    }
#endif

end:
    *actual_read = buffer - buf_start;
    return complete;
}

static StrResult file_read_all(Arena *arena, File file) {
    StrResult result = {0};
    int64_t length = file_length(file);
    if (length < 0) {
        return result;
    }
    // file position cannot be negative at this point
    char *buf = arena_push(arena, char, length);
    int64_t read_length = 0;
    file_read(file, buf, length, &read_length);
    bool ok = read_length == length;

    if (!ok) {
        arena_pop(arena, char, length);
    }

    return (StrResult){
        .ok     = ok,
        .string = str_from(buf, length),
    };
}


static bool file_write_all(File file, Str str) {
#if OS_WINDOWS
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
        ssize_t n = write(file, str.data, str.length);
        if (n == -1) {
            return false;
        }
        str = str_skip(str, n);
    }
    return true;
#endif // #if OS_WINDOWS
}


static bool file_close(File file) {
#if OS_WINDOWS
    return CloseHandle(file);
#else
    // TODO: can close return an error?
    close(file);
    return true;
#endif // #if OS_WINDOWS
}

// TODO: move this into its own file
static Str str_last_error(Arena *arena) {
#if OS_WINDOWS
    DWORD err = GetLastError();

    DWORD max_length = 64*KB - 1;
    char *buf = arena_push(arena, char, max_length, .zeroed=false);
    DWORD len = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        buf, max_length, NULL);

    arena_pop(arena, char, max_length - len - 2);

    Str err_string = str_from(buf, len);

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

    StrResult result = file_read_all(arena, file);
    if (!result.ok) {
        migi_log(Log_Error, "Failed to read from file '%.*s': %.*s",
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

    bool ok = file_write_all(file, string);
    if (!ok) {
        migi_log(Log_Error, "Failed to write to file '%.*s': %.*s", 
                SArg(filepath), SArg(str_last_error(tmp.arena)));
    }

    file_close(file);
    arena_temp_release(tmp);
    return ok;
}

static bool strlist_to_file(StrList list, Str filepath) {
    File file = file_open(filepath, .write=true);
    if (file == FILE_ERROR) {
        return false;
    }

    bool ok = true;
    for (StrNode *node = list.head; ok && node; node = node->next) {
        ok = file_write_all(file, node->string);
    }
    if (!ok) {
        Temp tmp = arena_temp();
        migi_log(Log_Error, "Failed to write to file '%.*s': %.*s",
                SArg(filepath), SArg(str_last_error(tmp.arena)));
        arena_temp_release(tmp);
    }

    file_close(file);
    return ok;
}

#ifdef MIGI_STRING_BUILDER_H

static void sb_push_file(StrBuilder *sb, Str filename) {
    sb__init(sb);

    File file = file_open(filename);
    // TODO: perform `file_length(file)` and if it doesnt fail
    // then allocate that as the amount upfront. Only fall back
    // to the loop below if file_length doesnt work on this file

    // Reading in chunks when the file size is unknown
    int64_t size = 4096;
    bool done = false;
    while (!done) {
        char *buf = arena_push(sb->arena, char, size);
        int64_t read_length = 0;
        done = file_read(file, buf, size, &read_length);
        sb->length += read_length;
        arena_pop(sb->arena, char, size - read_length);
    }
}

static bool sb_to_file_opt(StrBuilder *sb, Str filename, StrBuilderOpt opt) {
    return str_to_file(sb_to_str_opt(sb, opt), filename);
}

#endif

#endif // ifndef MIGI_FILE_H
