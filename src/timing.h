#ifndef TIMING_H
#define TIMING_H

#include <stdint.h>
#include "migi_core.h"

// Returns current timestamp in nanoseconds
// The beginning of the epoch is system-dependant
static uint64_t timer_now();

// Read the CPU timer (rdtsc instruction)
// The resolution of the timer is unspecified, use estimate_cpu_timer_freq for that
static uint64_t timer_now_cpu();

// Sleep until target (in nanoseconds)
// Target must be an absolute time, Eg. sleep_until(timer_nanos() + 1*NS) to sleep 1 sec
// This is done to make the timer much more accurate than regular sleeping
// Returns the current time after waking up
static uint64_t sleep_until(uint64_t target_nanos);

#if _WIN32

#include <intrin.h>
#include <windows.h>

static uint64_t get_os_timer_freq(void) {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return freq.QuadPart;
}

static uint64_t read_os_timer(void) {
    LARGE_INTEGER value;
    QueryPerformanceCounter(&value);
    return value.QuadPart;
}

static uint64_t timer_now() {
    LARGE_INTEGER time;
    QueryPerformanceCounter(&time);

    static LARGE_INTEGER freq = {0};
    if (freq.QuadPart == 0) {
        QueryPerformancefreq(&freq);
    }

    uint64_t secs  = time.QuadPart / freq.QuadPart;
    uint64_t nanos = (time.QuadPart % freq.QuadPart) * (NS / freq.QuadPart);
    return (secs * NS) + nanos;
}

static uint64_t sleep_until(uint64_t target_nanos) {
    todof("implement for windows");
}

#else

#include <x86intrin.h>
#include <sys/time.h>
#include <time.h>

static uint64_t get_os_timer_freq(void) {
    return 1000000;
}

static uint64_t read_os_timer(void) {
    struct timeval value;
    gettimeofday(&value, 0);

    uint64_t result = get_os_timer_freq()*(uint64_t)value.tv_sec + (uint64_t)value.tv_usec;
    return result;
}

static uint64_t timer_now() {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec * NS) + now.tv_nsec;
}

// Taken from: https://www.youtube.com/watch?v=rMBtprggzuQ
static uint64_t sleep_until(uint64_t target_nanos) {
    // sleep for target_nanos - 2us
    struct timespec deadline = {
        .tv_sec  = (target_nanos - 2*US) / NS,
        .tv_nsec = (target_nanos - 2*US) % NS,
    };
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline, NULL);

    // busy wait the final 2us
    uint64_t last;
    do {
        last = timer_now();
    } while (last < target_nanos);
    return last;
}

#endif

static uint64_t read_cpu_timer(void) {
    return __rdtsc();
}

static uint64_t timer_now_cpu(void) {
    return __rdtsc();
}

static uint64_t estimate_cpu_timer_freq(void) {
    uint64_t milliseconds_to_wait = 100;
    uint64_t os_freq = get_os_timer_freq();

    uint64_t cpu_start = read_cpu_timer();
    uint64_t os_start = read_os_timer();
    uint64_t os_end = 0;
    uint64_t os_elapsed = 0;
    uint64_t os_wait_time = os_freq * milliseconds_to_wait / 1000;
    while(os_elapsed < os_wait_time) {
        os_end = read_os_timer();
        os_elapsed = os_end - os_start;
    }

    uint64_t cpu_end = read_cpu_timer();
    uint64_t cpu_elapsed = cpu_end - cpu_start;

    uint64_t cpu_freq = 0;
    if(os_elapsed) {
        cpu_freq = os_freq * cpu_elapsed / os_elapsed;
    }

    return cpu_freq;
}


#endif // ! TIMING_H

