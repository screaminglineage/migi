#include "timing.h"
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TIMING_H_IMPLEMENTATION
#include "timing.h"

#define REPETITION_TESTER_IMPLEMENTATION
#define REPETITION_TESTER_LIVE_VIEW
#include "repetition_tester.h"

#include "random.h"
#include "string.h"
#include "migi.h"

typedef struct {
    int *data;
    size_t length;
    size_t capacity;
} Ints;

int main() {
    StringBuilder sb = {0};
    defer_block(sb.length = 0) {
        sb_push_str(&sb, SV("hello"));
        sb_push_str(&sb, SV("foo"));
        sb_push_str(&sb, SV("bar"));
        sb_push_str(&sb, SV("baz"));

        array_foreach(&sb, char, elem) {
            printf("%c ", *elem);
        }
        printf("len: %zu\n", sb.length);
    }
    printf("len: %zu\n", sb.length);
    array_foreach(&sb, char, elem) {
        printf("%c ", *elem);
    }

#if 0
    size_t size = 1*MB;
    time_t seed = time(NULL);
    char *buf1 = malloc(size);
    char *buf2 = malloc(size);

    migi_seed(seed);
    random_bytes(buf1, size);

    migi_seed(seed);
    random_bytes(buf2, size);

    assertf(memcmp(buf1, buf2, size) == 0, "random with same seed must have same data");

    size_t size = 1*MB;
    int time = 60;

    char *buf = malloc(1*MB);
    Tester tester = tester_init_with_name("random_bytes_regular", time, EstimateCPUTimerFreq(), size);
    while (!tester.finished) {
        tester_begin(&tester);
        random_bytes_regular(buf, size);
        tester_end(&tester);
    }
    tester_print_stats(&tester);
    free(buf);

    buf = malloc(1*MB);
    tester = tester_init_with_name("random_bytes_unrolled", time, EstimateCPUTimerFreq(), size);
    while (!tester.finished) {
        tester_begin(&tester);
        random_bytes_unrolled(buf, size);
        tester_end(&tester);
    }
    tester_print_stats(&tester);
    free(buf);

    StringBuilder sb = {0};
    sb_printf(&sb, "Hello world, %d, %.10f - %s\n\n", -3723473, sin(25.6212e99), "what is this even doing????");
    sb_printf(&sb, "Hello world, %d, %.10f - %s\n\n", -3723473, sin(25.6212e99), "what is this even doing????");

    StringBuilder new_sb = {0};
    // read_to_string(&new_sb, SV("migi_string.c"));
    const char *str = sb_to_cstr(&new_sb);

    sb_printf(&sb, "%s\n", str);
    printf("%s", sb_to_cstr(&sb));

    Ints ints = {0};
    for (size_t i = 0; i < 100; i++) {
        array_add(&ints, i);
    }

    Ints ints_new = {0};
    array_reserve(&ints_new, 100);
    for (size_t i = 0; i < 100; i++) {
        array_add(&ints, 2*i);
    }

    array_extend(&ints_new, &ints);
    array_swap_remove(&ints_new, 50);
    printf("ints = %zu, new_ints = %zu\n", ints.length, ints_new.length);

    for (size_t i = 0; i < ints_new.length; i++) {
        printf("%d ", ints_new.data[i]);
    }
#endif
    printf("\n");


    return 0;
}
