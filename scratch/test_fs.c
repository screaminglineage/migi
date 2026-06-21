#include "migi.h"

#include "filesystem/filesystem.h"


void test_cwd() {
    Temp tmp = arena_temp();
    Str cwd = get_cwd(tmp.arena);
    printf("%.*s\n", SArg(cwd));
    cwd = get_cwd_executable(tmp.arena);
    printf("%.*s\n", SArg(cwd));
    cwd = get_executable_path(tmp.arena);
    printf("%.*s\n", SArg(cwd));

    Str new_cwd = S("/mnt");
    bool r = set_cwd(new_cwd);
    assert(r == true && str_eq(get_cwd(tmp.arena), new_cwd));

    arena_temp_release(tmp);
}

int main() {
    Temp tmp = arena_temp();
    test_cwd();
    arena_temp_release(tmp);
    return 0;
}
