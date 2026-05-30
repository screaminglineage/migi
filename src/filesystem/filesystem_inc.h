#ifndef MIGI_FILESYSTEM_INC_H
#define MIGI_FILESYSTEM_INC_H

// Defines the common interface for the filesystem.h for both windows and linux

#include "migi_string.h"

typedef struct {
    bool error;
    bool is_directory;
    bool is_symbolic_link;
} FileType;

typedef struct {
    bool replace_exisiting;
} FileOpt;

bool file_touch(Str filepath);
bool file_copy_opt(Str from, Str to, FileOpt opt);
#define file_copy(from, to, ...) file_copy_opt((from), (to), (FileOpt){__VA_ARGS__})
bool file_move_opt(Str from, Str to, FileOpt opt);
#define file_move(from, to, ...) file_move_opt((from), (to), (FileOpt){__VA_ARGS__})
bool file_delete(Str filepath);
bool file_exists(Str filepath);
FileType file_type(Str filepath);

typedef struct {
    bool recursive;
} DirDeleteOpt;

bool dir_make_if_not_exists(Str dirpath);
bool dir_copy(Str from, Str to);
bool dir_move(Str from, Str to);
bool dir_delete_opt(Str filepath, DirDeleteOpt opt);
#define dir_delete(filepath, ...) dir_delete_opt((filepath), (DirDeleteOpt){__VA_ARGS__})

#endif // ifndef MIGI_FILESYSTEM_H
