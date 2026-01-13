#ifndef REPETITION_TESTER_H
#define REPETITION_TESTER_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>

typedef int64_t i64;
typedef uint64_t u64;
typedef uint32_t u32;

typedef struct {
    u64 min;
    u64 max;
    u64 total;
} Stats;

typedef enum {
    StatsTime,
    StatsPageFault,

    StatsCount
} StatsKind;

typedef struct {
    Stats stats[StatsCount];
    u32 count;

    u64 last_test_start;
    u64 last_page_faults;
    u64 byte_count;
    u64 try_for_time;
    u64 cpu_freq;
    bool finished;
    char *name;
} Tester;

static Tester tester_init_with_name(char *name, u32 seconds_to_try, u64 cpu_freq, u64 byte_count);
static Tester tester_init(u32 seconds_to_try, u64 cpu_freq, u64 byte_count);
static void tester_begin(Tester *tester);
static void tester_end(Tester *tester);
static void tester_print_stats(Tester *tester);
static double tester_get_min_throughput(Tester *tester, StatsKind stat, int unit);


static i64 get_page_faults();

// define to only include the header
#ifndef REPETITION_TESTER_AS_HEADER

#include "timing.h"

Tester tester_init_with_name(char *name, u32 seconds_to_try, u64 cpu_freq, u64 byte_count) {
#ifdef REPETITION_TESTER_LIVE_VIEW
    printf("\x1b[?25l");
#endif
    return (Tester){
        .name = name,
        .stats = {
            [StatsTime] = {
                .min = (u64)-1
            },
            [StatsPageFault] = {
                .min = (u64)-1
            },
        },
        .cpu_freq = cpu_freq,
        .try_for_time = seconds_to_try*cpu_freq,
        .byte_count = byte_count,
    };
}

Tester tester_init(u32 seconds_to_try, u64 cpu_freq, u64 byte_count) {

#ifdef REPETITION_TESTER_LIVE_VIEW
    printf("\x1b[?25l");
#endif
    return (Tester){
        .name = "test",
        .stats = {
            [StatsTime] = {
                .min = (u64)-1
            },
            [StatsPageFault] = {
                .min = (u64)-1
            },
        },
        .cpu_freq = cpu_freq,
        .try_for_time = seconds_to_try*cpu_freq,
        .byte_count = byte_count,
    };
}

void tester_begin(Tester *tester) {
    tester->last_page_faults = get_page_faults();
    tester->last_test_start = read_cpu_timer();
}

void tester_end(Tester *tester) {
    u64 current_elapsed = read_cpu_timer() - tester->last_test_start;
    u64 current_page_faults = get_page_faults() - tester->last_page_faults;

    if (current_elapsed < tester->stats[StatsTime].min) {
        tester->stats[StatsTime].min = current_elapsed;
    }
    if (current_elapsed > tester->stats[StatsTime].max) {
        tester->stats[StatsTime].max = current_elapsed;
    }
    if (current_page_faults < tester->stats[StatsPageFault].min) {
        tester->stats[StatsPageFault].min = current_page_faults;
    }
    if (current_page_faults > tester->stats[StatsPageFault].max) {
        tester->stats[StatsPageFault].max = current_page_faults;
    }
    tester->stats[StatsTime].total += current_elapsed;
    tester->stats[StatsPageFault].total += current_page_faults;
    tester->count += 1;

    if (tester->stats[StatsTime].total >= tester->try_for_time) tester->finished = true;
#ifdef REPETITION_TESTER_LIVE_VIEW
    double min_time_sec = (double)tester->stats[StatsTime].min/(double)tester->cpu_freq;
    printf("[%s] (best): %.3f ms at %.4f gb/s, (Page Faults: %.4f) \r",
            tester->name,
            min_time_sec*1000.0,
            tester->byte_count/(min_time_sec*GB),
            (double)tester->stats[StatsPageFault].total/(double)tester->count);
#endif
}

// unit must be gb (1024*1024*1024), mb (1024*1024), etc
double tester_get_min_throughput(Tester *tester, StatsKind stat, int unit) {
    double min_time_sec = (double)tester->stats[stat].min/(double)tester->cpu_freq;
    return tester->byte_count/(unit*min_time_sec);
}

void tester_print_stats(Tester *tester) {
#ifdef REPETITION_TESTER_LIVE_VIEW
    printf("\x1b[2K");
#endif
    printf("%s\n----------------------------------------\n", tester->name);
    printf("Ran %u times in %.3f seconds\n", tester->count, (double)tester->stats[StatsTime].total/(double)tester->cpu_freq);
    double min_time_sec = (double)tester->stats[StatsTime].min/(double)tester->cpu_freq;
    double max_time_sec = (double)tester->stats[StatsTime].max/(double)tester->cpu_freq;
    double avg_time_sec = (double)tester->stats[StatsTime].total/((double)tester->count * (double)tester->cpu_freq);
    double data = (double)tester->byte_count;
    double gb = (1024.0*1024.0*1024.0);

    printf("Data: %.2f mb\n", tester->byte_count/(1024.0*1024.0));
    printf("Estimated CPU Frequency: %.3f ghz\n", tester->cpu_freq/gb);
    printf("Min: %.3f ms at %.4f gb/s (Page Faults: %"PRIu64", %.3f k/fault)\n",
            min_time_sec*1000.0,
            data/(gb*min_time_sec),
            tester->stats[StatsPageFault].min,
            (double)(tester->byte_count/1024.0)/(double)tester->stats[StatsPageFault].min);
    printf("Max: %.3f ms at %.4f gb/s (Page Faults: %"PRIu64", %.3f k/fault)\n",
            max_time_sec*1000.0,
            data/(gb*max_time_sec),
            tester->stats[StatsPageFault].max,
            (double)(tester->byte_count/1024.0)/(double)tester->stats[StatsPageFault].max);
    printf("Avg: %.3f ms at %.4f gb/s (Page Faults: %.3f)\n",
            avg_time_sec*1000.0,
            data/(gb*avg_time_sec),
            (double)tester->stats[StatsPageFault].total/(double)tester->count);
    printf("\x1b[?25h");
}


#ifdef _WIN32
#include <windows.h>
#include <psapi.h>

i64 get_page_faults() {
    PROCESS_MEMORY_COUNTERS counters = {0};
    BOOL ret = GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters));
    if (ret == 0) {
        fprintf(stderr, "get_page_faults: error: failed to read usage: %ld\n", GetLastError());
        return -1;
    }
    return counters.PageFaultCount;
}

#else

#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <string.h>
#include <errno.h>

i64 get_page_faults() {
    struct rusage usage = {0};
    if (getrusage(RUSAGE_SELF, &usage) == -1) {
        fprintf(stderr, "get_page_faults: error: failed to read usage: %s\n", strerror(errno));
        return -1;
    }
    return usage.ru_majflt + usage.ru_minflt;
}

#endif // _WIN32
#endif // REPETITION_TESTER_AS_HEADER


#endif // REPETITION_TESTER_H
