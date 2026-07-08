#include "../build/migi_amalgam.h"

int main() {
    Temp tmp = arena_temp();
    Str s = strf(tmp.arena, "foo %.*s baz", SArg(S("bar")));
    printf("s = %.*s\n", SArg(s));

    StrList a = str_split(tmp.arena, s, S(" "));
    strlist_foreach(&a, node) {
        printf("%.*s, ", SArg(node->string));
    }
    printf("\n");

    arena_temp_release(tmp);

    return 0;
}
