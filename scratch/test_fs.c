#include "migi.h"

#include "filesystem.h"

void test_dir_delete() {
    bool r = dir_delete(S("junk"), .recursive=true);
    printf("%s\n", bool_to_cstr(r));
}

void test_dir_copy() {
    bool r = dir_copy(S("src"), S("build"));
    printf("%s\n", bool_to_cstr(r));
}

void test_dir_move() {
    bool r = dir_move(S("build"), S("src"));
    printf("%s\n", bool_to_cstr(r));
}

void test_cwd() {
    Temp tmp = arena_temp();
    Str cwd;
    cwd = get_cwd(tmp.arena);
    printf("%.*s\n", SArg(cwd));
    cwd = get_cwd_executable(tmp.arena);
    printf("%.*s\n", SArg(cwd));
    cwd = get_executable_path(tmp.arena);
    printf("%.*s\n", SArg(cwd));

    Str new_cwd = S("C:\\");
    bool r = set_cwd(new_cwd);
    assert(r == true && str_eq(get_cwd(tmp.arena), new_cwd));

    arena_temp_release(tmp);
}

int main() {
    Temp tmp = arena_temp();
    // test_dir_move();
    // test_dir_copy();
    test_dir_delete();
    // test_cwd();
    arena_temp_release(tmp);
    printf("\nExiting Successfully\n");
    return 0;
}
