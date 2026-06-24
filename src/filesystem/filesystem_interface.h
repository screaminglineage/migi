#ifndef MIGI_FILESYSTEM_INTERFACE_H
#define MIGI_FILESYSTEM_INTERFACE_H

// Defines the common interface for the filesystem.h for both windows and linux

#include "migi_string.h"

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

#endif // ifndef MIGI_FILESYSTEM_INTERFACE_H
