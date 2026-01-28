#ifndef MIGI_RANDOM_H
#define MIGI_RANDOM_H

// TODO: forward declare all functions

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "arena.h"
#include "migi_core.h"

// MIGI_DONT_AUTO_SEED_RNG can be defined to allow the user manually seed the
// RNG before calling it for the first time.
// This is helpful when the user wants to control the seed, or if checking
// the seed state on each call of random appears to be slow
//
// If when not defined (default) the RNG will be seeded when any of the
// `random_()` functions are called for the first time.
// By default, it is seeded with the UNIX timestamp from time(NULL)

#ifndef MIGI_DONT_AUTO_SEED_RNG
#include <time.h>
#include <stdbool.h>
#endif

#define MIGI_RNG_STATE_LEN 4
typedef struct {
    uint64_t state[MIGI_RNG_STATE_LEN];
#ifndef MIGI_DONT_AUTO_SEED_RNG
    bool is_seeded;
#endif
} Rng;

static Rng rng;

static void rng_reset() {
    memset(&rng, 0, sizeof(rng));
}

static inline uint64_t rotl(const uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
}

// xoshiro256** algorithm
static uint64_t xoshiro256_starstar(uint64_t state[MIGI_RNG_STATE_LEN]) {
    const uint64_t result = rotl(state[1] * 5, 7) * 9;

    const uint64_t t = state[1] << 17;

    state[2] ^= state[0];
    state[3] ^= state[1];
    state[1] ^= state[2];
    state[0] ^= state[3];

    state[2] ^= t;
    state[3] = rotl(state[3], 45);
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
    state[3] = rotl(state[3], 45);
    return result;
}


// Used for initialising the seed
static uint64_t splitmix64(uint64_t x) {
    uint64_t z = (x += 0x9e3779b97f4a7c15);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
    z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
    return z ^ (z >> 31);
}

// Seed the random number generator
static void migi_seed(uint64_t seed) {
    for (size_t i = 0; i < MIGI_RNG_STATE_LEN; i++) {
        rng.state[i] = splitmix64(seed);
    }
#ifndef MIGI_DONT_AUTO_SEED_RNG
    rng.is_seeded = true;
#endif
}


// Return a random unsigned 64 bit integer
static uint64_t migi_random() {
#ifndef MIGI_DONT_AUTO_SEED_RNG
    if (!rng.is_seeded) migi_seed(time(NULL));
#endif
    return xoshiro256_starstar(rng.state);
}

// Return a random float in the range [0, 1]
// 0 <= num <= 1
static float random_float() {
#ifndef MIGI_DONT_AUTO_SEED_RNG
    if (!rng.is_seeded) migi_seed(time(NULL));
#endif
    return (float)xoshiro256_plus(rng.state) / (float)UINT64_MAX;
}

// Return a random double in the range [0, 1]
// 0 <= num <= 1
static double random_double() {
#ifndef MIGI_DONT_AUTO_SEED_RNG
    if (!rng.is_seeded) migi_seed(time(NULL));
#endif
    return (double)xoshiro256_plus(rng.state) / (double)UINT64_MAX;
}

// Return a random integer in the range [min, max]
// min <= num <= max
static int64_t random_range(int64_t min, int64_t max) {
    return (int64_t)floorf(random_float() * (max - min + 1) + min);
}

// Return a random integer in the range [min, max)
// min <= num < max
static int64_t random_range_exclusive(int64_t min, int64_t max) {
    float r = random_float();
    return (int64_t)floorf(r * (max - min) + min);
}

// Return a random double in the range [min, max]
// min <= num <= max
static double random_range_double(double min, double max) {
    return (random_double() * (max - min) + min);
}

// Return a random float in the range [min, max]
// min <= num <= max
static float random_range_float(float min, float max) {
    return (random_float() * (max - min) + min);
}

// Fill passed in buffer with random bytes
static void random_bytes(void *buf, size_t size) {
    byte *dest = (byte *)buf;
    for (size_t i = 0; i < size; i+=8) {
        uint64_t rand = migi_random();
        for (size_t j = 0; i+j < size && j < 8; j++) {
            dest[i + j] = (rand >> (56 - 8*j)) & 0xFF;
        }
    }
}

static void random_shuffle_bytes(byte *buf, size_t elem_size, size_t size) {
    Temp tmp = arena_temp();
    byte *temp_buf = arena_push_nonzero(tmp.arena, byte, elem_size);

    for (size_t i = 0; i < size - 1; i++) {
        int64_t rand_index = random_range_exclusive(0, size);

        memcpy(temp_buf, &buf[elem_size*i], elem_size);
        memcpy(&buf[elem_size*i], &buf[elem_size*rand_index], elem_size);
        memcpy(&buf[elem_size*rand_index], temp_buf, elem_size);
    }
    arena_temp_release(tmp);
}

typedef struct {
    float weight;
    size_t index;
} WeightIndex;

static int compare_weights(const void *a_, const void *b_) {
    const WeightIndex *a = a_;
    const WeightIndex *b = b_;
    if (a->weight > b->weight) {
        return 1;
    } else if (a->weight < b->weight) {
        return -1;
    } else {
        return 0;
    }
}

typedef struct {
    double *weights;
} RandomChooseOpt;

static byte *random_choose_bytes(byte *buf, size_t elem_size, size_t len, RandomChooseOpt opt) {
    if (!opt.weights) {
        size_t i = random_range_exclusive(0, len);
        return buf + elem_size*i;
    }

    double total_weight = 0;
    for (size_t i = 0; i < len; i++) {
        total_weight += opt.weights[i];
    }
    double choice = random_float() * total_weight;
    size_t i = 0;
    for (; i < len; i++) {
        if (choice <= opt.weights[i]) {
            break;
        }
        choice -= opt.weights[i];
    }
    return buf + elem_size*i;
}

// Convenience macro to get a random array of any type
#define random_array(array, type, size) \
    random_bytes((byte *)(array), sizeof(type)*(size))

// Convenience macro to shuffle an array of any type
#define array_shuffle(array, type, size) \
    random_shuffle_bytes((byte *)(array), sizeof(type), (size))

// Convenience macro to choose a random element from an designated initializer or a static array
//
// Str s = random_choose((Str []){S("foo"), S("bar"), S("baz"), S("hello"), S("world")});
// int foo[] = {10, 20, 30, 40, 50, 60, 70, 80, 90};
// int num = random_choose(foo);
#define random_choose(...) \
    ((__VA_ARGS__)[random_range_exclusive(0, sizeof((__VA_ARGS__))/sizeof((__VA_ARGS__)[0]))])

// Convenience macros to choose a random element from an array by weight
#define random_choose_ex(array, type, len, ...) \
    *(type *)(random_choose_bytes((byte *)(array), sizeof(type), len, (RandomChooseOpt){__VA_ARGS__}))

#endif // MIGI_RANDOM_H
