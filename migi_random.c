#include <stddef.h>
#include <stdint.h>

// AUTO_SEED_RNG can be set to 0 to make the user manually seed the
// RNG before calling it for the first time.
// This is helpful when the user wants to control the seed, or if checking
// the seed state on each call of random appears to be slow
//
// If set to 1, the RNG will be seeded when any of the random functions are
// called for the first time.
// By default, it is seeded with the UNIX timestamp from time(NULL)

#ifndef AUTO_SEED_RNG
#define AUTO_SEED_RNG 1
#endif

#if AUTO_SEED_RNG
#include <time.h>
#endif

#define RNG_STATE_LEN 4
typedef struct {
#if AUTO_SEED_RNG
    _Bool is_seeded;
#endif
    uint64_t state[RNG_STATE_LEN];
} Rng;

static Rng rng;

static inline uint64_t rotl(const uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
}

// xoshiro256** algorithm
static uint64_t xoshiro256_starstar(uint64_t state[RNG_STATE_LEN]) {
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
static uint64_t xoshiro256_plus(uint64_t state[RNG_STATE_LEN]) {
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
static void seed(uint64_t seed) {
    for (size_t i = 0; i < RNG_STATE_LEN; i++) {
        rng.state[i] = splitmix64(seed);
    }
#if AUTO_SEED_RNG
    rng.is_seeded = 1;
#endif
}

// Return a random unsigned 64 bit integer
static uint64_t random() {
#if AUTO_SEED_RNG
    if (!rng.is_seeded) seed(time(NULL));
#endif
    return xoshiro256_starstar(rng.state);
}

// Return a random float in the range [0, 1)
// 0 <= num < 1
static float random_float() {
#if AUTO_SEED_RNG
    if (!rng.is_seeded) seed(time(NULL));
#endif
    return (float)xoshiro256_plus(rng.state) / (float)UINT64_MAX;
}

// Return a random double in the range [0, 1)
// 0 <= num < 1
static double random_double() {
#if AUTO_SEED_RNG
    if (!rng.is_seeded) seed(time(NULL));
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
// TODO: try unrolling the inner loop and benchmark the result
static void random_bytes(void *buf, size_t size) {
    uint8_t *dest = (uint8_t *)buf;
    for (size_t i = 0; i < size; i+=8) {
        uint64_t rand = random();
        for (size_t j = 0; i+j < size && j < 8; j++) {
            dest[i + j] = (rand >> (56 - 8*j)) & 0xFF;
        }
        /*
         * dest[i + 0] = rand >> 56;
         * dest[i + 1] = rand >> 48;
         * dest[i + 2] = rand >> 40;
         * dest[i + 3] = rand >> 32;
         * dest[i + 4] = rand >> 24;
         * dest[i + 5] = rand >> 16;
         * dest[i + 6] = rand >> 8;
         * dest[i + 7] = rand >> 0;
         */
    }
}

