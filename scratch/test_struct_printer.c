#include "migi.h"
#include "dynamic_array.h"
#include "../gen/String_printer.gen.c"

typedef struct {
    int *data;
    size_t length;
    size_t capacity;
    Str *extra;
} Ints;
#include "../gen/Ints_printer.gen.c"

typedef struct {
    Str *data;
    size_t length;
    void *more;
} Strings;
#include "../gen/Strings_printer.gen.c"

typedef struct {
    int integer;
    const long *pointer;
    char ch;
    const float f32;
    const char *str;
    Ints ints;
} Foo;
#include "../gen/Foo_printer.gen.c"

typedef struct {
    Str string;
    const char *c_string;
    char *char_ptr;
    Foo foo;
    Strings strings;
    // int array[10];               // unsupported for now
    // int flexible_array_member[]; // unsupported for now
} Bar;
#include "../gen/Bar_printer.gen.c"

int main() {
    long l = 10*TB;

    Ints arr = {0};
    for (size_t i = 0; i < 10; i++) {
        array_push(&arr, i);
    }

    Strings strings = slice_from(Str, Strings, S("hello"), S("world"), S("Generated!"));
    strings.more = (void *)0x8000f;

    Foo f = {
        .integer = 25,
        .pointer = &l,
        .ch = 'A',
        .f32 = 3.14,
        .str = "hello world",
        .ints = arr,
    };

    Bar b = {
        .string = S("abcd"),
        .c_string = "efgh",
        .char_ptr = (char *)0xad171a,
        .foo = f,
        .strings = strings,
    };


    print_Bar(b);
}
