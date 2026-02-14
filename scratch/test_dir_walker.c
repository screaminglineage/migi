#include "migi.h"
#include "dir_walker.h"

Str file_type_str(Arena *a, DirIter entry) {
    if (!entry.is_dir && !entry.is_symlink && !entry.is_hidden) {
        return S("Regular");
    } else {
        return stringf(a, "[%c%c%c]",
            entry.is_dir? 'D': '_',
            entry.is_symlink? 'S': '_',
            entry.is_hidden? 'H': '_');
    }
}


void test_walk_dir(Str path) {
    DirWalker walker = walker_init(path,
                        .stop_on_error   = false,
                        .follow_symlinks = true);
    size_t max_depth = 2;

    Temp tmp = arena_temp();
#if 0
    DirEntry file = walker_next(tmp.arena, &walker, false);

    while (!file.over) {
        if (file.error) continue;

        bool is_git = file.is_dir && str_eq(file.name, S(".git"));
        if (file.depth >= max_depth || is_git) {
            file = walker_next(tmp.arena, &walker, .dont_recurse = true);
            continue;
        }

        printf("Path: %.*s\n", SArg(file.path));
        printf("Name: %.*s\n", SArg(file.name));
        printf("Size: %zu bytes\n", file.size);
        printf("Time Created: %zu\n", file.time_created);
        printf("Time Modified: %zu\n", file.time_modified);
        printf("Time Accessed: %zu\n", file.time_accessed);
        printf("Type: %.*s\n", SArg(file_type_str(tmp.arena, file)));
        printf("------------------------------------------\n");

        file = walker_next(tmp.arena, &walker);
    }
#else
    WalkerNextOpt opt = {0};
    dir_foreach_opt(tmp.arena, &walker, file, &opt) {
        if (file.error) continue;

        bool is_git = false;//file.is_dir && str_eq(file.name, S(".git"));
        if (file.depth >= max_depth || is_git) {
            opt.dont_recurse = true;
        }

        printf("Path: %.*s\n", SArg(file.path));
        printf("Name: %.*s\n", SArg(file.name));
        printf("Size: %zu bytes\n", file.size);
        printf("Time Created: %zu\n", file.time_created);
        printf("Time Modified: %zu\n", file.time_modified);
        printf("Time Accessed: %zu\n", file.time_accessed);
        printf("Type: %.*s\n", SArg(file_type_str(tmp.arena, file)));
        printf("------------------------------------------\n");
    }
#endif
    walker_free(&walker);
    arena_temp_release(tmp);
}

int main(int argc, char **argv) {
    if (argc <= 1) {
        migi_log(Log_Error, "expected a path as first argument");
        return 1;
    }
    Str path = str_from_cstr(argv[1]);
    test_walk_dir(path);

    Temp tmp = arena_temp();
    DirIterNode *entries = dir_get_all_children(tmp.arena, path);

    list_foreach(entries, DirIterNode, node) {
        printf("%.*s\n", SArg(node->entry.name));
    }
    arena_temp_release(tmp);

    return 0;
}
