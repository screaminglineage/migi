#ifndef MIGI_MATH_H
#define MIGI_MATH_H

// General math functions and constants

#include <stdint.h>
#include <stdbool.h>
#include <math.h>


// These constants are not part of the C standard (not even e and pi!)
// so its best to just define them in one place manually

#define E        2.71828182845904523536   // lim(n -> inf) [ (1 + 1/n)^n ]
#define PI       3.14159265358979323846   // ln(-1)/i
#define TAU      6.28318530717958647693   // 2*pi
#define PHI      1.61803398874989484820   // the spin itself
#define SQRT2    1.41421356237309504880   // sqrt(2)
#define LOG2E    1.44269504088896340736   // log2(e)
#define LOG10E   0.434294481903251827651  // log10(e)
#define LN2      0.693147180559945309417  // ln(2)
#define LN10     2.30258509299404568402   // ln(10)


#define min_of(a, b) ((a) < (b)? (a): (b))
#define max_of(a, b) ((a) > (b)? (a): (b))
#define between(value, start, end) ((start) <= (value) && (value) <= (end))

// Ensure value is clamped at the top or bottom
// Simply min/max by another name but useful in context
#define clamp_top(a, b) min_of((a), (b))
#define clamp_bottom(a, b) max_of((a), (b))

// Return value if it lies in [low, high], otherwise return the respective end (low or high)
#define clamp(value, low, high) clamp_bottom(clamp_top((value), (high)), (low))

// like modulo but wraps-around to `b - 1` if result is negative
#define remainder(a, b) ((a) - (b) * ((a) / (b)))

#define abs_difference(a, b) ((a) > (b)? (a) - (b): (b) - (a))


// Returns `n` if its already a power of two
static uint64_t next_power_of_two(uint64_t n);

// Return the number of bytes needed to align `value` to the next multiple of `align_to`
// align_up_pow2_amt(9, 8) = 7 [9 + 7 = 16 (next multiple of 8)]
// NOTE: `align_to` should be a power of 2
static uint64_t align_up_pow2_amt(uint64_t value, uint64_t align_to);

// Return the number of bytes needed to align `value` to the previous multiple of `align_to`
// align_down_pow2_amt(21, 8) = 5 [21 - 5 = 16 (prev multiple of 8)]
// NOTE: `align_to` should be a power of 2
static uint64_t align_down_pow2_amt(uint64_t value, uint64_t align_to);

// Align up `value` to the next multiple of `align_to`
// Returns `value` if it is already aligned
// align_up_pow2(9, 8) = 16 [next multiple of 8]
// NOTE: `align_to` should be a power of 2
static uint64_t align_up_pow2(uint64_t value, uint64_t align_to);

// Align down `value` to the previous multiple of `align_to`
// Returns `value` if it is already aligned
// align_down_pow2(21, 8) = 16 [prev multiple of 8]
// NOTE: `align_to` should be a power of 2
static uint64_t align_down_pow2(uint64_t value, uint64_t align_to);

// Rotate `x` by `k` (x <<< k)
static uint64_t rotate_left(const uint64_t x, int k);

// Calculates log2 in constant time
// NOTE: `value` must be greater than 0
static int log2_64(uint64_t value);
static int log2_32(uint32_t value);

// Relative and absolute Tolerances for isclose
typedef struct {
    double rel_tol; // defaults to 1e-9
    double abs_tol; // defaults to 0.0
} IsCloseOpt;

// Checks whether two doubles are almost equal
static bool isclose_opt(double a, double b, IsCloseOpt opt);

// Default values
#define isclose(a, b, ...) isclose_opt((a), (b), \
    (IsCloseOpt){ .rel_tol = 1e-9, .abs_tol = 0.0, __VA_ARGS__ })



static uint64_t next_power_of_two(uint64_t n) {
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    n++;
    return n;
}


static uint64_t align_up_pow2_amt(uint64_t value, uint64_t align_to) {
    return -value & (align_to - 1);
}

static uint64_t align_down_pow2_amt(uint64_t value, uint64_t align_to) {
    return value & (align_to - 1);
}

static uint64_t align_up_pow2(uint64_t value, uint64_t align_to) {
    return value + align_up_pow2_amt(value, align_to);
}

static uint64_t align_down_pow2(uint64_t value, uint64_t align_to) {
    return value - align_down_pow2_amt(value, align_to);
}

// From: https://docs.python.org/3/whatsnew/3.5.html#pep-485-a-function-for-testing-approximate-equality
static bool isclose_opt(double a, double b, IsCloseOpt opt) {
    return fabs(a - b) <= max_of(opt.rel_tol * max_of(fabs(a), fabs(b)), opt.abs_tol);
}

static uint64_t rotate_left(const uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
}


// Both log2_64 and log2_32 taken from: https://stackoverflow.com/a/11398748
const int tab64[64] = {
    63,  0, 58,  1, 59, 47, 53,  2,
    60, 39, 48, 27, 54, 33, 42,  3,
    61, 51, 37, 40, 49, 18, 28, 20,
    55, 30, 34, 11, 43, 14, 22,  4,
    62, 57, 46, 52, 38, 26, 32, 41,
    50, 36, 17, 19, 29, 10, 13, 21,
    56, 45, 25, 31, 35, 16,  9, 12,
    44, 24, 15,  8, 23,  7,  6,  5
};

static int log2_64(uint64_t value) {
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value |= value >> 32;
    return tab64[((uint64_t)((value - (value >> 1))*0x07EDD5E59A4E28C2)) >> 58];
}

const int tab32[32] = {
     0,  9,  1, 10, 13, 21,  2, 29,
    11, 14, 16, 18, 22, 25,  3, 30,
     8, 12, 20, 28, 15, 17, 24,  7,
    19, 27, 23,  6, 26,  5,  4, 31
};

int log2_32 (uint32_t value)
{
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    return tab32[(uint32_t)(value*0x07C4ACDD) >> 27];
}

#endif // ifndef MIGI_MATH_H
