#ifndef MIGI_RANDOM_H
#define MIGI_RANDOM_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "migi_core.h"
#include "migi_math.h"

// NOTE: MIGI_DONT_AUTO_SEED_RNG can be defined to allow the user to manually seed
// the RNG before calling it for the first time.
// This is helpful when the user wants to control the seed, or if checking
// the seed state on each call of random appears to be slow
//
// When this constant is not defined (default) the RNG will be seeded when 
// any of the `rand_*` functions are called for the first time.
// By default, it is seeded with the UNIX timestamp from time(NULL)
//
// NOTE: All of these functions also take an "optional" `rng` parameter through the
// macro trick. If it is not passed in then the global RNG is used
// For example,
//     `rand_random(.rng = &my_rng)` // uses my_rng
//     `rand_random()`               // uses the global rng


#ifndef MIGI_DONT_AUTO_SEED_RNG
#include <time.h>
#include <stdbool.h>
#endif

#define MIGI_RNG_STATE_LEN 4
typedef struct {
    uint64_t state[MIGI_RNG_STATE_LEN];
    double prev_normal_distr;           // last normal distribution value, cached for efficiency
#ifndef MIGI_DONT_AUTO_SEED_RNG
    bool is_seeded;
#endif
} Rng;

threadvar Rng MIGI_GLOBAL_RNG;

typedef struct {
    Rng *rng;
} RandOpt;

// Reset RNGs
#define rand_rng_reset(...) rand_rng_reset_opt((RandOpt){.rng = &MIGI_GLOBAL_RNG, __VA_ARGS__})
static void rand_rng_reset_opt(RandOpt opt);

// Set the global RNG to `rng`, and return the old one
static Rng rand_rng_set_global(Rng rng);

// Seed RNGs
#define rand_rng_seed(seed, ...) rand_rng_seed_opt(seed, (RandOpt){.rng = &MIGI_GLOBAL_RNG, __VA_ARGS__})
static void rand_rng_seed_opt(uint64_t seed, RandOpt opt);

// Random Generation Functions
static uint64_t xoshiro256_starstar(uint64_t state[MIGI_RNG_STATE_LEN]);
static uint64_t xoshiro256_plus(uint64_t state[MIGI_RNG_STATE_LEN]);
// Used for initialising the seed
static uint64_t splitmix64(uint64_t x);

// Return a random unsigned 64 bit integer
#define rand_random(...) rand_random_opt((RandOpt){.rng = &MIGI_GLOBAL_RNG, __VA_ARGS__})
static uint64_t rand_random_opt(RandOpt opt);

// Return a random float/double in the range [0, 1]
// 0 <= num <= 1
#define rand_float(...) rand_float_opt((RandOpt){.rng = &MIGI_GLOBAL_RNG, __VA_ARGS__})
#define rand_double(...) rand_double_opt((RandOpt){.rng = &MIGI_GLOBAL_RNG, __VA_ARGS__})
static float rand_float_opt(RandOpt opt);
static double rand_double_opt(RandOpt opt);

// Return a random number in the range [min, max]
// min <= num <= max
#define rand_range(min, max, ...) rand_range_opt((min), (max), (RandOpt){.rng = &MIGI_GLOBAL_RNG, __VA_ARGS__})
#define rand_range_float(min, max, ...) rand_range_float_opt((min), (max), (RandOpt){.rng = &MIGI_GLOBAL_RNG, __VA_ARGS__})
#define rand_range_double(min, max, ...) rand_range_double_opt((min), (max), (RandOpt){.rng = &MIGI_GLOBAL_RNG, __VA_ARGS__})
static int64_t rand_range_opt(int64_t min, int64_t max, RandOpt opt);
static float rand_range_float_opt(float min, float max, RandOpt opt);
static double rand_range_double_opt(double min, double max, RandOpt opt);

// Return a random integer in the range [min, max)
// min <= num < max
#define rand_range_exclusive(min, max, ...) rand_range_exclusive_opt((min), (max), (RandOpt){.rng = &MIGI_GLOBAL_RNG, __VA_ARGS__})
static int64_t rand_range_exclusive_opt(int64_t min, int64_t max, RandOpt opt);

// Fill passed in buffer with random bytes
#define rand_fill(array, type, size, ...) \
    rand_fill_bytes_opt((byte *)(array), sizeof(type)*(size), (RandOpt){.rng = &MIGI_GLOBAL_RNG, __VA_ARGS__})
#define rand_fill_bytes(buf, size, ...) rand_fill_bytes_opt((buf), (size), (RandOpt){.rng = &MIGI_GLOBAL_RNG, __VA_ARGS__})
static void rand_fill_bytes_opt(void *buf, size_t size, RandOpt opt);

// Shuffle elements of buffer
#define rand_shuffle(array, type, size, ...) \
    rand_shuffle_bytes((byte *)(array), sizeof(type), (size), (RandOpt){.rng = &MIGI_GLOBAL_RNG, __VA_ARGS__})
static void rand_shuffle_bytes(byte *buf, size_t elem_size, size_t size, RandOpt opt);


typedef struct {
    double *weights;
    Rng *rng;
} RandChooseOpt;

// Choose a random element from an designated initializer or a static array
// Examples:
//
// Str s = rand_choose((Str []){S("foo"), S("bar"), S("baz"), S("hello"), S("world")});
//
// int foo[] = {10, 20, 30, 40, 50, 60, 70, 80, 90};
// int num = rand_choose(foo);
#define randr_choose(rng, ...) \
    ((__VA_ARGS__)[rand_range_exclusive_opt(0, sizeof((__VA_ARGS__))/sizeof((__VA_ARGS__)[0]), (RandOpt){.rng = rng})])

#define rand_choose(...) \
    ((__VA_ARGS__)[rand_range_exclusive_opt(0, sizeof((__VA_ARGS__))/sizeof((__VA_ARGS__)[0]), (RandOpt){.rng = &MIGI_GLOBAL_RNG})])


// Choose a random element from an array by weight
// If no weights are passed in, then all elements have equal weightage
static byte *rand_choose_bytes(byte *buf, size_t elem_size, size_t len, RandChooseOpt opt);
#define rand_choose_ex(array, type, len, ...) \
    *(type *)(rand_choose_bytes((byte *)(array), sizeof(type), len, (RandChooseOpt){.rng = &MIGI_GLOBAL_RNG, __VA_ARGS__}))


// Mean and Standard Deviation for normal distribution
// The default values are 0.0 and 1.0 respectively which
// gives the standard normal distribution
typedef struct {
    double mean;
    double standard_deviation;
    Rng *rng;
} RandNormalOpt;

// Generate normally distributed doubles
static double rand_normal_opt(RandNormalOpt opt);
#define rand_normal(...) rand_normal_opt( \
    (RandNormalOpt){ .mean = 0.0, .standard_deviation = 1.0, .rng = &MIGI_GLOBAL_RNG, __VA_ARGS__ })




static void rand_rng_reset_opt(RandOpt opt) {
    mem_clear(opt.rng);
}

static Rng rand_rng_set_global(Rng rng) {
    Rng prev_rng = MIGI_GLOBAL_RNG;
    MIGI_GLOBAL_RNG = rng;
    return prev_rng;
}

static void rand_rng_seed_opt(uint64_t seed, RandOpt opt) {
    for (size_t i = 0; i < MIGI_RNG_STATE_LEN; i++) {
        opt.rng->state[i] = splitmix64(seed);
    }
    opt.rng->prev_normal_distr = NAN;
#ifndef MIGI_DONT_AUTO_SEED_RNG
    MIGI_GLOBAL_RNG.is_seeded = true;
#endif
}


// xoshiro256** algorithm
static uint64_t xoshiro256_starstar(uint64_t state[MIGI_RNG_STATE_LEN]) {
    const uint64_t result = rotate_left(state[1] * 5, 7) * 9;

    const uint64_t t = state[1] << 17;

    state[2] ^= state[0];
    state[3] ^= state[1];
    state[1] ^= state[2];
    state[0] ^= state[3];

    state[2] ^= t;
    state[3] = rotate_left(state[3], 45);
    return result;
}

// xoshiro256+ algorithm
// used for floating point generation
static uint64_t xoshiro256_plus(uint64_t state[MIGI_RNG_STATE_LEN]) {
    const uint64_t result = state[0] + state[3];

    const uint64_t t = state[1] << 17;

    state[2] ^= state[0];
    state[3] ^= state[1];
    state[1] ^= state[2];
    state[0] ^= state[3];

    state[2] ^= t;
    state[3] = rotate_left(state[3], 45);
    return result;
}


static uint64_t splitmix64(uint64_t x) {
    uint64_t z = (x += 0x9e3779b97f4a7c15);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
    z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
    return z ^ (z >> 31);
}


static uint64_t rand_random_opt(RandOpt opt) {
#ifndef MIGI_DONT_AUTO_SEED_RNG
    if (!MIGI_GLOBAL_RNG.is_seeded) rand_rng_seed_opt(time(NULL), opt);
#endif
    return xoshiro256_starstar(opt.rng->state);
}

static float rand_float_opt(RandOpt opt) {
#ifndef MIGI_DONT_AUTO_SEED_RNG
    if (!MIGI_GLOBAL_RNG.is_seeded) rand_rng_seed_opt(time(NULL), opt);
#endif
    return (float)xoshiro256_plus(opt.rng->state) / (float)UINT64_MAX;
}

static double rand_double_opt(RandOpt opt) {
#ifndef MIGI_DONT_AUTO_SEED_RNG
    if (!MIGI_GLOBAL_RNG.is_seeded) rand_rng_seed_opt(time(NULL), opt);
#endif
    return (double)xoshiro256_plus(opt.rng->state) / (double)UINT64_MAX;
}

static int64_t rand_range_opt(int64_t min, int64_t max, RandOpt opt) {
    return (int64_t)floorf(rand_float_opt(opt) * (max - min + 1) + min);
}

static int64_t rand_range_exclusive_opt(int64_t min, int64_t max, RandOpt opt) {
    return (int64_t)floorf(rand_float_opt(opt) * (max - min) + min);
}

static double rand_range_double_opt(double min, double max, RandOpt opt) {
    return rand_double_opt(opt) * (max - min) + min;
}

static float rand_range_float_opt(float min, float max, RandOpt opt) {
    return (rand_float_opt(opt) * (max - min) + min);
}

static void rand_fill_bytes_opt(void *buf, size_t size, RandOpt opt) {
    byte *dest = (byte *)buf;
    for (size_t i = 0; i < size; i+=8) {
        uint64_t rand = rand_random_opt(opt);
        for (size_t j = 0; i+j < size && j < 8; j++) {
            dest[i + j] = (rand >> (56 - 8*j)) & 0xFF;
        }
    }

}

static void rand_shuffle_bytes(byte *buf, size_t elem_size, size_t size, RandOpt opt) {
    Temp tmp = arena_temp();
    byte *temp_buf = arena_push(tmp.arena, byte, elem_size, .zeroed=false);

    // Fisher-Yates Shuffle
    for (size_t i = 0; i < size - 2; i++) {
        int64_t j = rand_range_exclusive_opt(i, size, opt);

        memcpy(temp_buf, &buf[elem_size*i], elem_size);
        memcpy(&buf[elem_size*i], &buf[elem_size*j], elem_size);
        memcpy(&buf[elem_size*j], temp_buf, elem_size);
    }
    arena_temp_release(tmp);
}

static byte *rand_choose_bytes(byte *buf, size_t elem_size, size_t len, RandChooseOpt opt) {
    RandOpt rand_opt = { .rng = opt.rng };

    if (!opt.weights) {
        size_t i = rand_range_exclusive_opt(0, len, rand_opt);
        return buf + elem_size*i;
    }

    double total_weight = 0;
    for (size_t i = 0; i < len; i++) {
        total_weight += opt.weights[i];
    }
    double choice = rand_float_opt(rand_opt) * total_weight;
    size_t i = 0;
    for (; i < len; i++) {
        if (choice <= opt.weights[i]) {
            break;
        }
        choice -= opt.weights[i];
    }
    return buf + elem_size*i;
}


// Box-Muller Transform to convert uniform distribution to normal distribution
// Taken from: https://en.wikipedia.org/wiki/Box%E2%80%93Muller_transform#C++
static double rand_normal_opt(RandNormalOpt opt) {
    if (!isnan(opt.rng->prev_normal_distr)) {
        double r = opt.rng->prev_normal_distr;
        opt.rng->prev_normal_distr = NAN;
        return r;
    }

    RandOpt rand_opt = { .rng = opt.rng };

    double u1 = 1.0, u2 = 0.0;
    do {
        u1 = rand_double_opt(rand_opt);
    }
    while (u1 == 0);
    u2 = rand_double_opt(rand_opt);

    double mag = opt.standard_deviation * sqrt(-2.0 * log(u1));
    double z0  = mag * cos(TAU * u2) + opt.mean;
    double z1  = mag * sin(TAU * u2) + opt.mean;
    opt.rng->prev_normal_distr = z1;
    return z0;
}


#endif // MIGI_RANDOM_H
