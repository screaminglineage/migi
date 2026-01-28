#include "migi.h"

void test_arena_functions() {
    typedef struct {
        int a, b;
        char c;
        float f;
        long l;
    } Foo;

    {
        Arena *a = arena_init();
        *arena_new(a, int) = 12;
        assert(a->position == sizeof(Arena) + sizeof(int));
        *arena_new(a, float) = 324.242f;
        assert(a->position == sizeof(Arena) + sizeof(int) + sizeof(float));
        *arena_new(a, Foo) = (Foo){1, 2, 'a', 3.14f, 23091283};
        assert(a->position == sizeof(Arena) + sizeof(int) + sizeof(float) + sizeof(Foo));
        arena_push(a, long, 10);
        assert(a->position == sizeof(Arena) + sizeof(int) + sizeof(float) + sizeof(Foo) + sizeof(long)*10);
        arena_free(a);
    }

    {
        static byte buffer[1*KB];
        Arena *a = arena_init_static(buffer, sizeof(buffer));

        Str string = S("hello world!");
        char *str = arena_copy(a, char, string.data, string.length);
        Str uppercased = str_to_upper(a, (Str){.data = str, string.length});
        assert(str_eq(uppercased, S("HELLO WORLD!")));
        arena_free(a);
    }

    {
        Arena *a = arena_init(.type = Arena_Chained, .commit_size = 4*MB, .reserve_size = 64*MB);
        Temp tmp = arena_save(a);
        for (int i = 0; i < 10000; i++) {
            *arena_new(a, Foo) = (Foo){i, i+1, i % 256, sinf((float)i), i*i};
        }
        arena_rewind(tmp);
        arena_free(a);
    }

    {
        Arena *a = arena_init(.type = Arena_Chained, .commit_size = 3*KB, .reserve_size = 8*KB);
        Temp tmp = arena_save(a);
        char *data = arena_push(a, char, 4*KB + 1);
        memset(data, 0xff, 4*KB);
        data = arena_push(a, char, 8*KB);
        memset(data, 0xa, 8*KB);
        data = arena_push(a, char, 8*KB);
        memset(data, 0xb, 8*KB);
        arena_rewind(tmp);
    }

    // out of memory test
    {
        Arena *a2 = arena_init(.type = Arena_Linear, .commit_size = 3*KB, .reserve_size = 8*KB);
        arena_push(a2, char, 4*KB);
        // arena_push(a2, char, 4*KB); // should crash here
    }

    // popping to the middle of a previous block
    {
        Arena *a = arena_init(.type = Arena_Chained, .commit_size = 3*KB, .reserve_size = 4*KB);
        arena_push(a, char, 3*KB);
        arena_push(a, char, 5*KB);
        Arena *c = a->current;

        arena_push(a, char, 4*KB);
        arena_pop(a, char, 8*KB);
        assert(a->current == c && a->current->position - sizeof(Arena) == 1*KB);
    }

    // popping to the beginning
    {
        Arena *a = arena_init(.type = Arena_Chained, .commit_size = 3*KB, .reserve_size = 4*KB);
        Arena *c = a->current;
        arena_push(a, char, 3*KB);
        arena_push(a, char, 3*KB);
        arena_pop(a, char, 6*KB);
        assert(a->current == c && a->current->position - sizeof(Arena) == 0);
    }

    {
        Arena *a = arena_init(.type = Arena_Chained, .commit_size = 3*KB, .reserve_size = 1*MB);
        Arena *c = a->current;
        arena_push(a, char, 1*MB);
        arena_push(a, char, 1*MB);
        arena_pop(a, char, 2*MB);
        assert(a->current == c && a->current->position - sizeof(Arena) == 0);
    }

    // popping and decommitting
    {
        Arena *a = arena_init(.type = Arena_Chained, .commit_size = 3*KB, .reserve_size = 1*MB);
        for (size_t i = 0; i < 1023; i++) {
            arena_push(a, char, 1*KB);
        }
        Arena *c = a->current;
        arena_push(a, char, 1*KB);

        size_t prev_committed = a->committed;
        arena_pop(a, char, 10*KB);
        assert(a->current == c && a->current->position - sizeof(Arena) == (1023*KB + 1*KB) - 10*KB);
        assert(a->current->committed < prev_committed);
    }

    // checkpoint save and rewind
    {
        Arena *a = arena_init(.type = Arena_Chained, .commit_size = 3*KB, .reserve_size = 1*MB);
        arena_push(a, char, 1*KB);
        Temp tmp = arena_save(a);
        for (size_t i = 0; i < 1022; i++) {
            arena_push(a, char, 1*KB);
        }
        arena_push(a, char, 1*KB);
        arena_rewind(tmp);
        assert(a->current->position == sizeof(Arena) + 1*KB);
        assert(a->current->committed == 4*KB);

        arena_pop(a, char, 10*KB);
        assert(a->current->position == sizeof(Arena) + 1*KB);
        assert(a->committed == 4*KB);
    }

    // realloc
    {
        Arena *a = arena_init();
        char *chars1 = arena_push(a, char, 1*MB);
        char *chars2 = arena_realloc(a, char, chars1, 1*MB, 2*MB);
        assert(chars1 == chars2);

        *arena_new(a, int) = 50;
        char *chars3 = arena_realloc(a, char, chars1, 2*MB, 4*MB);
        assert(chars2 != chars3);
    }

    // reading/writing files
    {
        Arena *arena = arena_init();
        Str str = str_from_file(arena, S("scratch/test_arena.c"));
        assert(str.length > 0);

        Str filepath = S("build/test_arena-dumped.c");
        assert(str_to_file(str, filepath));
        assert(str_eq(str, str_from_file(arena, filepath)));
    }
}

Str bar(Arena *a) {
    Temp tmp = arena_temp_excl(a);
    Str foo = stringf(a, "hello world %d %f, %.*s\n", 123, 4.51, SArg(S("testing!!!")));

    int *temp = arena_push(tmp.arena, int, 64);
    for (int i = 0; i < 64; i++) {
        temp[i] = i;
    }
    arena_temp_release(tmp);

    return foo;
}

void test_arena_temp() {
    Temp tmp = arena_temp();
    Str foo = bar(tmp.arena);

    int *temp = arena_push(tmp.arena, int, 64);
    for (int i = 0; i < 64; i++) {
        temp[i] = i;
    }

    assertf(str_eq(foo, S("hello world 123 4.510000, testing!!!\n")), "data is not overwritten");
    arena_temp_release(tmp);
}

void test_arena() {
    test_arena_functions();
    test_arena_temp();
}

int main() {
    test_arena();
    printf("\nExiting Successfully\n");
    return 0;
}
