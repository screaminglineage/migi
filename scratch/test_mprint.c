#include "migi_print.h"

int main(int argc, char **argv) {
    const char *a = "abcd";
    mprint("Example:", argc, argv, 'a', 3.14f, S("foo, bar, baz"), a);

    mprint_ex("\t", "\n\n", "Example:", argc, argv, 'a', 3.14f, S("foo, bar, baz"), a);

    const char *baz = "baz";
    mprintf("%: (%, %) %-%-% and more!\n", "Example", 1, -2.1, S("foo"), S("bar"), baz);

    mfprintf(stderr, "%: %-%-%: %\n", S("Example"), 2026, 5LL, 23U, -1.323);

    return 0;
}

