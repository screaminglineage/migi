#ifndef MIGI_FILESYSTEM_H
#define MIGI_FILESYSTEM_H

#include "migi_core.h"
#include "migi_string.h"

typedef enum {
    FileType_Regular,
    FileType_Directory,
    FileType_SymbolicLink,
    FileType_Other
} FileType;

bool file_touch(Str filepath);
bool file_copy(Str from, Str to);
bool file_move(Str from, Str to);
bool file_delete(Str filepath);
FileType file_type(Str filepath);

typedef struct {
    bool recursive;
} DirDeleteOpt;

bool dir_make_if_not_exists(Str dirpath);
bool dir_copy(Str from, Str to);
bool dir_move(Str from, Str to);
bool dir_delete_opt(Str filepath, DirDeleteOpt opt);



#endif // ifndef MIGI_FILESYSTEM_H
