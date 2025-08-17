#ifndef MIGI_RANDOM_H
#define MIGI_RANDOM_H

// TODO: forward declare all functions

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "migi.h"

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
    return (float)xoshiro256_plus(rng.state) / UINT64_MAX;
}

// Return a random double in the range [0, 1]
// 0 <= num <= 1
static double random_double() {
#ifndef MIGI_DONT_AUTO_SEED_RNG
    if (!rng.is_seeded) migi_seed(time(NULL));
#endif
    return (double)xoshiro256_plus(rng.state) / UINT64_MAX;
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
static void random_bytes(byte *buf, size_t size) {
    byte *dest = (uint8_t *)buf;
    for (size_t i = 0; i < size; i+=8) {
        uint64_t rand = migi_random();
        for (size_t j = 0; i+j < size && j < 8; j++) {
            dest[i + j] = (rand >> (56 - 8*j)) & 0xFF;
        }
    }
}

// TODO: look into improving this function
static void random_shuffle_bytes(byte *buf, size_t elem_size, size_t size) {
    for (size_t i = 0; i < size; i++) {
        int64_t index_a = random_range_exclusive(0, size);
        int64_t index_b = random_range_exclusive(0, size);

        // TODO: maybe use a temporary arena instead of a VLA
        byte temp[elem_size];
        memcpy(temp, &buf[elem_size*index_a], elem_size);
        memcpy(&buf[elem_size*index_a], &buf[elem_size*index_b], elem_size);
        memcpy(&buf[elem_size*index_b], temp, elem_size);
    }
}

typedef struct {
    float weight;
    size_t index;
} WeightIndex;

static int compare_weights(const void *_a, const void *_b) {
    const WeightIndex *a = _a;
    const WeightIndex *b = _b;
    if (a->weight > b->weight) {
        return 1;
    } else if (a->weight < b->weight) {
        return -1;
    } else {
        return 0;
    }
}

static byte *random_choose_bytes_weighted(byte *buf, size_t elem_size, float *weights, size_t length) {
    // TODO: use an arena rather than a VLA
    WeightIndex weight_indices[length];
    for (size_t i = 0; i < length; i++) {
        weight_indices[i] = (WeightIndex){.weight = weights[i], .index = i};
    }
    qsort(weight_indices, length, sizeof(*weight_indices), compare_weights);

    float choice = random_float();
    float accumulator = 0;
    for (size_t i = 0; i < length; i++) {
        accumulator += weight_indices[i].weight;
        if (choice <= accumulator) {
            return buf + elem_size*weight_indices[i].index;
        }
    }
    migi_unreachablef("since choice is in [0, 1], it must already be selected");
}

static byte *random_choose_bytes_fuzzy(byte *buf, size_t elem_size, int64_t *weights, size_t length) {
    int sum = 0;
    for (size_t i = 0; i < length; i++) {
        sum += weights[i];
    }
    avow(sum != 0, "sum of weights must not be 0");

    // TODO: use an arena rather than a VLA
    float normalised_weights[length];
    for (size_t i = 0; i < length; i++) {
        normalised_weights[i] = (float)weights[i] / (float)sum;
    }
    return random_choose_bytes_weighted(buf, elem_size, normalised_weights, length);
}


// Convenience macro to get a random array of any type
#define random_array(array, type, size) \
    (random_bytes((byte *)(array), sizeof(type)*(size)))

// Convenience macro to shuffle an array of any type
#define array_shuffle(array, type, size) \
    (random_shuffle_bytes((byte *)(array), sizeof(type), (size)))

// Convenience macro to choose a random element from an designated initializer
#define random_choose(...) \
    ((__VA_ARGS__)[random_range_exclusive(0, sizeof(__VA_ARGS__)/sizeof(*(__VA_ARGS__)))])

// Convenience macros to choose a random element from an array by weight
#define random_choose_weighted(array, type, ...)                           \
    (*(type *)(random_choose_bytes_weighted((byte *)(array), sizeof(type), \
            (__VA_ARGS__), sizeof(__VA_ARGS__)/sizeof(*(__VA_ARGS__)))))

#define random_choose_fuzzy(array, type, ...)                           \
    (*(type *)(random_choose_bytes_fuzzy((byte *)(array), sizeof(type), \
            (__VA_ARGS__), sizeof(__VA_ARGS__)/sizeof(*(__VA_ARGS__)))))

#endif // MIGI_RANDOM_H
