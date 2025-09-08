#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "migi.h"
#include "arena.h"
#include "migi_string.h"

void test_arena() {
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
        *arena_new(a, float) = 324.242;
        assert(a->position == sizeof(Arena) + sizeof(int) + sizeof(float));
        *arena_new(a, Foo) = (Foo){1, 2, 'a', 3.14, 230912830912389};
        assert(a->position == sizeof(Arena) + sizeof(int) + sizeof(float) + sizeof(Foo));
        arena_push(a, long, 10);
        assert(a->position == sizeof(Arena) + sizeof(int) + sizeof(float) + sizeof(Foo) + sizeof(long)*10);
        arena_free(a);
    }

    {
        static byte buffer[1*KB];
        Arena *a = arena_init(.type = Arena_Static, .backing_buffer = buffer, .backing_buffer_size = sizeof(buffer));

        String string = SV("hello world!");
        char *str = arena_copy(a, char, string.data, string.length);
        String uppercased = string_to_upper(a, (String){.data = str, string.length});
        assert(string_eq(uppercased, SV("HELLO WORLD!")));
        arena_free(a);
    }

    {
        Arena *a = arena_init(.type = Arena_Chained, .commit_size = 4*MB, .reserve_size = 64*MB);
        Checkpoint c = arena_save(a);
        for (size_t i = 0; i < 10000; i++) {
            *arena_new(a, Foo) = (Foo){i, i+1, i % 256, sinf(i), i*i};
        }
        arena_rewind(c);
        arena_free(a);
    }

    {
        Arena *a = arena_init(.type = Arena_Chained, .commit_size = 3*KB, .reserve_size = 8*KB);
        Checkpoint c = arena_save(a);
        char *data = arena_push(a, char, 4*KB + 1);
        memset(data, 0xff, 4*KB);
        data = arena_push(a, char, 8*KB);
        memset(data, 0xa, 8*KB);
        data = arena_push(a, char, 8*KB);
        memset(data, 0xb, 8*KB);
        arena_rewind(c);
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
        Checkpoint c = arena_save(a);
        for (size_t i = 0; i < 1022; i++) {
            arena_push(a, char, 1*KB);
        }
        arena_push(a, char, 1*KB);
        arena_rewind(c);
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
}

int main() {
    test_arena();

    Arena *arena = arena_init();
    StringResult res = read_file(arena, SV("./main.c"));
    assert(res.ok);
    assert(write_file(res.string, SV("main-new.c"), arena));

    return 0;
}
