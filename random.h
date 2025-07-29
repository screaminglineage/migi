#ifndef MIGI_RANDOM_H
#define MIGI_RANDOM_H

// TODO: add a function that shuffles an existing array
// TODO: maybe add a weighted random chooser which take an array 
// and weights and returns a random value based on them

#include <stddef.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

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

// Return a random float in the range [0, 1)
// 0 <= num < 1
static float random_float() {
#ifndef MIGI_DONT_AUTO_SEED_RNG
    if (!rng.is_seeded) migi_seed(time(NULL));
#endif
    return (float)xoshiro256_plus(rng.state) / (float)UINT64_MAX;
}

// Return a random double in the range [0, 1)
// 0 <= num < 1
static double random_double() {
#ifndef MIGI_DONT_AUTO_SEED_RNG
    if (!rng.is_seeded) migi_seed(time(NULL));
#endif
    return (double)xoshiro256_plus(rng.state) / (double)UINT64_MAX;
}

// Return a random integer in the range [min, max]
// min <= num <= max
static int64_t random_range(int64_t min, int64_t max) {
    return (int64_t)(random_float() * (max - min + 1) + min);
}

// Return a random integer in the range [min, max)
// min <= num < max
static int64_t random_range_exclusive(int64_t min, int64_t max) {
    return (int64_t)(random_float() * (max - min) + min);
}

// Return a random double in the range [min, max]
// min <= num <= max
static double random_range_double(double min, double max) {
    return (random_double() * (max - min + 1) + min);
}

// Return a random float in the range [min, max]
// min <= num <= max
static float random_range_float(float min, float max) {
    return (random_float() * (max - min + 1) + min);
}

// Fill passed in buffer with random bytes
static void random_bytes(byte *buf, size_t size) {
    uint8_t *dest = (uint8_t *)buf;
    for (size_t i = 0; i < size; i+=8) {
        uint64_t rand = migi_random();
        for (size_t j = 0; i+j < size && j < 8; j++) {
            dest[i + j] = (rand >> (56 - 8*j)) & 0xFF;
        }
    }
}

static void array_shuffle_bytes(byte *buf, size_t elem_size, size_t size) {
    for (size_t i = 0; i < size; i++) {
        int64_t index =  random_range_exclusive(0, size);
        byte temp[elem_size];
        memcpy(temp, &buf[elem_size*index], elem_size);
        memcpy(&buf[elem_size*index], buf, elem_size);
        memcpy(buf, temp, elem_size);
    }
}

// Convenience macro to get a random array of any type
#define random_array(array, type, size) \
    (random_bytes((byte *)(array), sizeof(type)*(size)))

// Convenience macro to shuffle an array of any type
#define array_shuffle(array, type, size) \
    (array_shuffle_bytes((byte *)(array), sizeof(type), (size)))

#endif // MIGI_RANDOM_H
