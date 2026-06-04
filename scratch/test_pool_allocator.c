#include "pool_allocator.h"

int main() {
    Temp tmp = arena_temp();
    Arena *a = tmp.arena;

    {
        PoolAlloc(Str) p = {0};
        *pool_alloc(a, &p) = S("foo");
        *pool_alloc(a, &p) = S("bar");
        *pool_alloc(a, &p) = S("baz");

        pool_foreach(&p, elem) {
            printf("%.*s ", SArg(*elem));
        }
        printf("\n");
        pool_reset(&p);
    }

    {
        PoolAlloc(int) p = {.capacity=1024};

        size_t size = 10;
        int **nums = arena_push(a, int *, size);
        for (size_t i = 0; i < size; i++) {
            int *num = pool_alloc(a, &p);
            *num = i;
            nums[i] = num;
        }

        pool_foreach(&p, item) {
            printf("%d ", *item);
        }
        printf("\n");

        printf("dealloc %d\n", pool_dealloc(&p, nums[5]));
        printf("dealloc %d\n", pool_dealloc(&p, nums[8]));
        printf("dealloc %d\n", pool_dealloc(&p, nums[3]));
        printf("dealloc %d\n", pool_dealloc(&p, nums[9]));

        *pool_alloc(a, &p) = 20;
        *pool_alloc(a, &p) = 30;
        *pool_alloc(a, &p) = 40;
        *pool_alloc(a, &p) = 50;
        *pool_alloc(a, &p) = 60;

        pool_foreach(&p, item) {
            printf("%d ", *item);
        }

        pool_reset(&p);
    }

    {
        typedef struct {
            int foo[512];
            float bar[512];
            char baz[512];
        } LargeStruct;

        PoolAlloc(LargeStruct) p = {0};

        LargeStruct *allocs[10] = {0};
        for (size_t i = 0; i < array_len(allocs); i++) {
            allocs[i] = pool_alloc(a, &p);
            rand_fill_bytes(&allocs[i]->foo, sizeof(allocs[i]->foo));
            rand_fill_bytes(&allocs[i]->bar, sizeof(allocs[i]->bar));
            rand_fill_bytes(&allocs[i]->baz, sizeof(allocs[i]->baz));
        }
        assert(p.length == 10);

        unused(pool_dealloc(&p, allocs[1]));
        unused(pool_dealloc(&p, allocs[9]));
        unused(pool_dealloc(&p, allocs[4]));
        unused(pool_dealloc(&p, allocs[0]));
        assert(p.length == 6);


        LargeStruct *a1 = pool_alloc(a, &p);
        LargeStruct *a2 = pool_alloc(a, &p);
        LargeStruct *a3 = pool_alloc(a, &p);
        LargeStruct *a4 = pool_alloc(a, &p);

        assert(a4 == allocs[1]);
        assert(a3 == allocs[9]);
        assert(a2 == allocs[4]);
        assert(a1 == allocs[0]);
        assert(p.length == 10);

        assert(p.length == 10);
        pool_reset(&p);
        assert(p.length == 0 && p.free_list == NULL);

        LargeStruct *s1 = pool_alloc(a, &p);
        rand_fill_bytes(s1, sizeof(*s1));
        assert(p.length == 1);

        unused(pool_dealloc(&p, s1));
        assert(p.length == 0);

        LargeStruct *s2 = pool_alloc(a, &p);
        rand_fill_bytes(s2, sizeof(*s2));
        assertf(s1 == s2, "s1 reallocated as s2 from free list");

        pool_reset(&p);
    }

    arena_temp_release(tmp);
    return 0;
}
