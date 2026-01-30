#ifndef MIGI_MATH_H
#define MIGI_MATH_H

// General math functions and constants

#include <stdint.h>
#include <stdbool.h>
#include <math.h>


// These constants are not part of the C standard (even e and pi!)
// and so its best to just define them in one place manually

#define E        2.71828182845904523536   // lim(n -> inf) [ (1 + 1/n)^n ]
#define PI       3.14159265358979323846   // ln(-1)/i
#define TAU      6.28318530717958647692   // 2*pi
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


#endif // ifndef MIGI_MATH_H
