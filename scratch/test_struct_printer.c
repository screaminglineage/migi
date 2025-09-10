#include <stddef.h>
#include <stdio.h>
#include "migi.h"
#include "dynamic_array.h"
#include "migi_string.h"
#include "../gen/String_printer.gen.c"

typedef struct {
    int *data;
    size_t length;
    size_t capacity;
    String *extra;
} Ints;
#include "../gen/Ints_printer.gen.c"

typedef struct {
    String *data;
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
    String string;
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
        array_add(&arr, i);
    }

    Strings strings = migi_slice(Strings, (String[]){SV("hello"), SV("world"), SV("Generated!")});
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
        .string = SV("abcd"),
        .c_string = "efgh",
        .char_ptr = (char *)0xad171a,
        .foo = f,
        .strings = strings,
    };


    print_Bar(b);
}
