#include "string_style.h"


#define check_str_eq(left, right)                                                                                            \
    ((!(str_eq((left), (right))))                                                                                            \
        ? migi_log(Log_Error, "assertion failed at line: %d left: '%.*s', right: '%.*s'", __LINE__, SArg(left), SArg(right)) \
        : (void)0)

void test_string_style() {
    Temp tmp = arena_temp();

    // snake case
    {
        check_str_eq(str_style_snake(tmp.arena, S("Foo_Bar_Baz")), S("foo_bar_baz"));
        check_str_eq(str_style_snake(tmp.arena, S("fooBarBaz")), S("foo_bar_baz"));
        check_str_eq(str_style_snake(tmp.arena, S("FooBarBaz")), S("foo_bar_baz"));
        check_str_eq(str_style_snake(tmp.arena, S("foo_bar_baz")), S("foo_bar_baz"));
        // check_str_eq(str_style_snake(tmp.arena, S("FOO_BAR_BAZ")), S("foo_bar_baz"));        // doesnt work
        check_str_eq(str_style_snake(tmp.arena, S("_____foo__bar__baz_")), S("_____foo__bar__baz_"));
    }

    // capitalized snake case
    {
        check_str_eq(str_style_snake_caps(tmp.arena, S("foo_bar_baz")), S("Foo_Bar_Baz"));
        check_str_eq(str_style_snake_caps(tmp.arena, S("fooBarBaz")), S("Foo_Bar_Baz"));
        check_str_eq(str_style_snake_caps(tmp.arena, S("FooBarBaz")), S("Foo_Bar_Baz"));
        check_str_eq(str_style_snake_caps(tmp.arena, S("Foo_Bar_Baz")), S("Foo_Bar_Baz"));
        // check_str_eq(str_style_snake_caps(tmp.arena, S("FOO_BAR_BAZ")), S("FOO_BAR_BAZ"));   // doesnt work
        check_str_eq(str_style_snake_caps(tmp.arena, S("_____foo__bar__baz_")), S("_____Foo__Bar__Baz_"));
    }

    // upper snake case
    {
        check_str_eq(str_style_snake_upper(tmp.arena, S("Foo_Bar_Baz")), S("FOO_BAR_BAZ"));
        check_str_eq(str_style_snake_upper(tmp.arena, S("fooBarBaz")), S("FOO_BAR_BAZ"));
        check_str_eq(str_style_snake_upper(tmp.arena, S("FooBarBaz")), S("FOO_BAR_BAZ"));
        check_str_eq(str_style_snake_upper(tmp.arena, S("foo_bar_baz")), S("FOO_BAR_BAZ"));
        // check_str_eq(str_style_snake_upper(tmp.arena, S("FOO_BAR_BAZ")), S("FOO_BAR_BAZ"));  // doesnt work
        check_str_eq(str_style_snake_upper(tmp.arena, S("_____foo__bar__baz_")), S("_____FOO__BAR__BAZ_"));
    }

    // camel case
    {
        check_str_eq(str_style_camel(tmp.arena, S("foo_bar_baz")), S("fooBarBaz"));
        check_str_eq(str_style_camel(tmp.arena, S("fooBarBaz")), S("fooBarBaz"));
        check_str_eq(str_style_camel(tmp.arena, S("FooBarBaz")), S("fooBarBaz"));
        check_str_eq(str_style_camel(tmp.arena, S("Foo_Bar_Baz")), S("fooBarBaz"));
        check_str_eq(str_style_camel(tmp.arena, S("foo__bar__baz")), S("fooBarBaz"));
        check_str_eq(str_style_camel(tmp.arena, S("")), S(""));
        // check_str_eq(str_style_camel(tmp.arena, S("_f_")), S("_F_")); // weird, doesnt work, needs custom parsing rather than strcut_foreach
        check_str_eq(str_style_camel(tmp.arena, S("FOO_BAR_BAZ")), S("fooBarBaz"));
        check_str_eq(str_style_camel(tmp.arena, S("a_a")), S("aA"));
        check_str_eq(str_style_camel(tmp.arena, S("foo_BAr_bAz")), S("fooBarBaz"));
        check_str_eq(str_style_camel(tmp.arena, S("A")), S("a"));
    }

    // pascal case
    {
        check_str_eq(str_style_pascal(tmp.arena, S("foo_bar_baz")), S("FooBarBaz"));
        check_str_eq(str_style_pascal(tmp.arena, S("fooBarBaz")), S("FooBarBaz"));
        check_str_eq(str_style_pascal(tmp.arena, S("FooBarBaz")), S("FooBarBaz"));
        check_str_eq(str_style_pascal(tmp.arena, S("Foo_Bar_Baz")), S("FooBarBaz"));
        check_str_eq(str_style_pascal(tmp.arena, S("foo__bar__baz")), S("FooBarBaz"));
        check_str_eq(str_style_pascal(tmp.arena, S("")), S(""));
        // check_str_eq(str_style_pascal(tmp.arena, S("_f_")), S("_F_")); // weird, doesnt work, needs custom parsing rather than strcut_foreach
        check_str_eq(str_style_pascal(tmp.arena, S("FOO_BAR_BAZ")), S("FooBarBaz"));
        check_str_eq(str_style_pascal(tmp.arena, S("a_a")), S("AA"));
        check_str_eq(str_style_pascal(tmp.arena, S("foo_BAr_bAz")), S("FooBarBaz"));
        check_str_eq(str_style_pascal(tmp.arena, S("A")), S("A"));
        check_str_eq(str_style_pascal(tmp.arena, S("a")), S("A"));
    }
    arena_temp_release(tmp);
}

int main() {
    test_string_style();
    printf("Exiting Successfully\n");
    return 0;
}
