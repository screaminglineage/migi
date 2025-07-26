#ifndef TIMING_H
#define TIMING_H

#include <stdint.h>

typedef uint64_t u64;

#if _WIN32

#include <intrin.h>
#include <windows.h>

static u64 get_os_timer_freq(void) {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return freq.QuadPart;
}

static u64 read_os_timer(void) {
    LARGE_INTEGER value;
    QueryPerformanceCounter(&value);
    return value.QuadPart;
}

#else

#include <x86intrin.h>
#include <sys/time.h>

static u64 get_os_timer_freq(void) {
    return 1000000;
}

static u64 read_os_timer(void) {
    struct timeval value;
    gettimeofday(&value, 0);

    u64 result = get_os_timer_freq()*(u64)value.tv_sec + (u64)value.tv_usec;
    return result;
}

#endif

static u64 read_cpu_timer(void) {
    return __rdtsc();
}

static u64 estimate_cpu_timer_freq(void) {
    u64 milliseconds_to_wait = 100;
    u64 os_freq = get_os_timer_freq();

    u64 cpu_start = read_cpu_timer();
    u64 os_start = read_os_timer();
    u64 os_end = 0;
    u64 os_elapsed = 0;
    u64 os_wait_time = os_freq * milliseconds_to_wait / 1000;
    while(os_elapsed < os_wait_time) {
        os_end = read_os_timer();
        os_elapsed = os_end - os_start;
    }

    u64 cpu_end = read_cpu_timer();
    u64 cpu_elapsed = cpu_end - cpu_start;

    u64 cpu_freq = 0;
    if(os_elapsed) {
        cpu_freq = os_freq * cpu_elapsed / os_elapsed;
    }

    return cpu_freq;
}

#endif // ! TIMING_H

