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

// MIGI_DONT_AUTO_SEED_RNG can be defined to allow the user manually seed the
// RNG before calling it for the first time.
// This is helpful when the user wants to control the seed, or if checking
// the seed state on each call of random appears to be slow
//
// If when not defined (default) the RNG will be seeded when any of the
// `rand_*` functions are called for the first time.
// By default, it is seeded with the UNIX timestamp from time(NULL)
// The `randr_*` functions take in an rng from the user as the argument instead.

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

// Reset RNGs
static void randr_rng_reset(Rng *rng);
static void rand_rng_reset();

// Set the global RNG to `rng`, and return the old one
static Rng rand_rng_set(Rng rng);

// Seed RNGs
static void randr_rng_seed(Rng *rng, uint64_t seed);
static void rand_rng_seed(uint64_t seed);

// Random Generation Functions
static uint64_t xoshiro256_starstar(uint64_t state[MIGI_RNG_STATE_LEN]);
static uint64_t xoshiro256_plus(uint64_t state[MIGI_RNG_STATE_LEN]);
// Used for initialising the seed
static uint64_t splitmix64(uint64_t x);

// Return a random unsigned 64 bit integer
static uint64_t randr_random(Rng *rng);
static uint64_t rand_random();

// Return a random float/double in the range [0, 1]
// 0 <= num <= 1
static float randr_float(Rng *rng);
static double randr_double(Rng *rng);
static float rand_float();
static double rand_double();

// Return a random number in the range [min, max]
// min <= num <= max
static int64_t randr_range(Rng *rng, int64_t min, int64_t max);
static float randr_range_float(Rng *rng, float min, float max);
static double randr_range_double(Rng *rng, double min, double max);
static int64_t rand_range(int64_t min, int64_t max);
static float rand_range_float(float min, float max);
static double rand_range_double(double min, double max);

// Return a random integer in the range [min, max)
// min <= num < max
static int64_t randr_range_exclusive(Rng *rng, int64_t min, int64_t max);
static int64_t rand_range_exclusive(int64_t min, int64_t max);

// Fill passed in buffer with random bytes
static void randr_fill_bytes(Rng *rng, void *buf, size_t size);
#define randr_fill(rng, array, type, size) randr_fill_bytes((rng), (byte *)(array), sizeof(type)*(size))
static void rand_fill_bytes(void *buf, size_t size);
#define rand_fill(array, type, size) rand_fill_bytes((byte *)(array), sizeof(type)*(size))

// Shuffle elements of buffer
static void randr_shuffle_bytes(Rng *rng, byte *buf, size_t elem_size, size_t size);
#define randr_shuffle(rng, array, type, size) randr_shuffle_bytes((rng), (byte *)(array), sizeof(type), (size))
static void rand_shuffle_bytes(byte *buf, size_t elem_size, size_t size);
#define rand_shuffle(array, type, size) rand_shuffle_bytes((byte *)(array), sizeof(type), (size))


typedef struct {
    double *weights;
} RandChooseOpt;

// Choose a random element from an designated initializer or a static array
// Examples:
//
// Str s = rand_choose((Str []){S("foo"), S("bar"), S("baz"), S("hello"), S("world")});
//
// int foo[] = {10, 20, 30, 40, 50, 60, 70, 80, 90};
// int num = rand_choose(foo);
#define randr_choose(rng, ...) \
    ((__VA_ARGS__)[randr_range_exclusive(rng, 0, sizeof((__VA_ARGS__))/sizeof((__VA_ARGS__)[0]))])
static byte *randr_choose_bytes(Rng *rng, byte *buf, size_t elem_size, size_t len, RandChooseOpt opt);

#define rand_choose(...) \
    ((__VA_ARGS__)[rand_range_exclusive(0, sizeof((__VA_ARGS__))/sizeof((__VA_ARGS__)[0]))])
static byte *rand_choose_bytes(byte *buf, size_t elem_size, size_t len, RandChooseOpt opt);

// Choose a random element from an array by weight
// If no weight is passed in, then all elements have equal weightage
#define randr_choose_ex(rng, array, type, len, ...) \
    *(type *)(randr_choose_bytes(rng, (byte *)(array), sizeof(type), len, (RandChooseOpt){__VA_ARGS__}))
#define rand_choose_ex(array, type, len, ...) \
    *(type *)(rand_choose_bytes((byte *)(array), sizeof(type), len, (RandChooseOpt){__VA_ARGS__}))


// Mean and Standard Deviation for normal distribution
// The default values are 0.0 and 1.0 respectively which
// gives the standard normal distribution
typedef struct {
    double mean;
    double standard_deviation;
} RandNormalOpt;

// Generate normally distributed doubles
static double randr_normal_opt(Rng *rng, RandNormalOpt opt);
static double rand_normal_opt(RandNormalOpt opt);

#define randr_normal(rng, ...) randr_normal_opt(rng, \
    (RandNormalOpt){ .mean = 0.0, .standard_deviation = 1.0, __VA_ARGS__ })
#define rand_normal(...) rand_normal_opt( \
    (RandNormalOpt){ .mean = 0.0, .standard_deviation = 1.0, __VA_ARGS__ })




static void randr_rng_reset(Rng *rng) {
    mem_clear(rng);
}

static void rand_rng_reset() {
    randr_rng_reset(&MIGI_GLOBAL_RNG);
}

static Rng rand_rng_set(Rng rng) {
    Rng prev_rng = MIGI_GLOBAL_RNG;
    MIGI_GLOBAL_RNG = rng;
    return prev_rng;
}

static void randr_rng_seed(Rng *rng, uint64_t seed) {
    for (size_t i = 0; i < MIGI_RNG_STATE_LEN; i++) {
        rng->state[i] = splitmix64(seed);
    }
    rng->prev_normal_distr = NAN;
}

static void rand_rng_seed(uint64_t seed) {
    randr_rng_seed(&MIGI_GLOBAL_RNG, seed);
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


static uint64_t randr_random(Rng *rng) {
    return xoshiro256_starstar(rng->state);
}

static uint64_t rand_random() {
#ifndef MIGI_DONT_AUTO_SEED_RNG
    if (!MIGI_GLOBAL_RNG.is_seeded) rand_rng_seed(time(NULL));
#endif
    return randr_random(&MIGI_GLOBAL_RNG);
}

static float randr_float(Rng *rng) {
    return (float)xoshiro256_plus(rng->state) / (float)UINT64_MAX;
}

static float rand_float() {
#ifndef MIGI_DONT_AUTO_SEED_RNG
    if (!MIGI_GLOBAL_RNG.is_seeded) rand_rng_seed(time(NULL));
#endif
    return randr_float(&MIGI_GLOBAL_RNG);
}

static double randr_double(Rng *rng) {
    return (double)xoshiro256_plus(rng->state) / (double)UINT64_MAX;
}

static double rand_double() {
#ifndef MIGI_DONT_AUTO_SEED_RNG
    if (!MIGI_GLOBAL_RNG.is_seeded) rand_rng_seed(time(NULL));
#endif
    return randr_double(&MIGI_GLOBAL_RNG);
}

static int64_t randr_range(Rng *rng, int64_t min, int64_t max) {
    return (int64_t)floorf(randr_float(rng) * (max - min + 1) + min);
}

static int64_t rand_range(int64_t min, int64_t max) {
    return randr_range(&MIGI_GLOBAL_RNG, min, max);
}

static int64_t randr_range_exclusive(Rng *rng, int64_t min, int64_t max) {
    return (int64_t)floorf(randr_float(rng) * (max - min) + min);
}

static int64_t rand_range_exclusive(int64_t min, int64_t max) {
    return randr_range_exclusive(&MIGI_GLOBAL_RNG, min, max);
}

static double randr_range_double(Rng *rng, double min, double max) {
    return randr_double(rng) * (max - min) + min;
}

static double rand_range_double(double min, double max) {
    return randr_range_double(&MIGI_GLOBAL_RNG, min, max);
}

static float randr_range_float(Rng *rng, float min, float max) {
    return (randr_float(rng) * (max - min) + min);
}

static float rand_range_float(float min, float max) {
    return randr_range_float(&MIGI_GLOBAL_RNG, min, max);
}

static void randr_fill_bytes(Rng *rng, void *buf, size_t size) {
    byte *dest = (byte *)buf;
    for (size_t i = 0; i < size; i+=8) {
        uint64_t rand = randr_random(rng);
        for (size_t j = 0; i+j < size && j < 8; j++) {
            dest[i + j] = (rand >> (56 - 8*j)) & 0xFF;
        }
    }

}

static void rand_fill_bytes(void *buf, size_t size) {
    randr_fill_bytes(&MIGI_GLOBAL_RNG, buf, size);
}

static void randr_shuffle_bytes(Rng *rng, byte *buf, size_t elem_size, size_t size) {
    Temp tmp = arena_temp();
    byte *temp_buf = arena_push_nonzero(tmp.arena, byte, elem_size);

    for (size_t i = 0; i < size - 1; i++) {
        int64_t rand_index = randr_range_exclusive(rng, 0, size);

        memcpy(temp_buf, &buf[elem_size*i], elem_size);
        memcpy(&buf[elem_size*i], &buf[elem_size*rand_index], elem_size);
        memcpy(&buf[elem_size*rand_index], temp_buf, elem_size);
    }
    arena_temp_release(tmp);
}

static void rand_shuffle_bytes(byte *buf, size_t elem_size, size_t size) {
    randr_shuffle_bytes(&MIGI_GLOBAL_RNG, buf, elem_size, size);
}

static byte *randr_choose_bytes(Rng *rng, byte *buf, size_t elem_size, size_t len, RandChooseOpt opt) {
    if (!opt.weights) {
        size_t i = randr_range_exclusive(rng, 0, len);
        return buf + elem_size*i;
    }

    double total_weight = 0;
    for (size_t i = 0; i < len; i++) {
        total_weight += opt.weights[i];
    }
    double choice = randr_float(rng) * total_weight;
    size_t i = 0;
    for (; i < len; i++) {
        if (choice <= opt.weights[i]) {
            break;
        }
        choice -= opt.weights[i];
    }
    return buf + elem_size*i;
}

static byte *rand_choose_bytes(byte *buf, size_t elem_size, size_t len, RandChooseOpt opt) {
    return randr_choose_bytes(&MIGI_GLOBAL_RNG, buf, elem_size, len, opt);
}


// Box-Muller Transform to convert uniform distribution to normal distribution
// Taken from: https://en.wikipedia.org/wiki/Box%E2%80%93Muller_transform#C++
static double randr_normal_opt(Rng *rng, RandNormalOpt opt) {
    if (!isnan(rng->prev_normal_distr)) {
        double r = rng->prev_normal_distr;
        rng->prev_normal_distr = NAN;
        return r;
    }

    double u1 = 1.0, u2 = 0.0;
    do {
        u1 = randr_double(rng);
    }
    while (u1 == 0);
    u2 = randr_double(rng);

    double mag = opt.standard_deviation * sqrt(-2.0 * log(u1));
    double z0  = mag * cos(TAU * u2) + opt.mean;
    double z1  = mag * sin(TAU * u2) + opt.mean;
    rng->prev_normal_distr = z1;
    return z0;
}


static double rand_normal_opt(RandNormalOpt opt) {
    return randr_normal_opt(&MIGI_GLOBAL_RNG, opt);
}

#endif // MIGI_RANDOM_H
