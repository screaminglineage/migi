#include <stdio.h>
#include "src/migi_string.h"
#include "gen/String_printer.gen.c"

typedef struct {
    int a;
    int b;
    char ch;
    float f;
    const char *str;
} Foo;
#include "gen/Foo_printer.gen.c"

typedef struct {
    String abcd;
    const char *efgh;
    double d;
    Foo f;
} Bar;
#include "gen/Bar_printer.gen.c"


int main() {
    Foo f = {
        .a = 25,
        .b = 40,
        .ch = 'A',
        .f = 3.14,
        .str = "hello world"
    };

    Bar b = {
        .abcd = SV("abcd"),
        .efgh = "efgh",
        .d = -1.243423,
        .f = f
    };

    print_Foo(f);
    print_Bar(b);
}
