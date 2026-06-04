#include "migi.h"

#include "filesystem/filesystem.h"

int main() {
    Temp tmp = arena_temp();
    bool r = file_move(S("build/test_fs"), S("foo"), .replace_exisiting=true);
    printf("%.*s\n", SArg(bool_to_str(r)));

    arena_temp_release(tmp);
    return 0;
}
