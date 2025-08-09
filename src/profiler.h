#ifndef PROFILER_H
#define PROFILER_H

static void begin_profiling();
static void end_profiling_and_print_stats();

#ifdef PROFILER_H_IMPLEMENTATION

#include <math.h>
#include <stdio.h>
#include <string.h>

#define TIMING_H_IMPLEMENTATION
#include "timing.h"

#include "migi.h"

typedef uint64_t u64;

#define MAX_TIMESTAMPS 4096

typedef struct {
    u64 bytes_count;
    u64 start_time;
    u64 elapsed_inclusive;
    u64 elapsed_exclusive;
    u64 hits;
    const char *name;
} ProfilerTimestamp;

typedef struct {
    ProfilerTimestamp timestamps[MAX_TIMESTAMPS];
    u64 start_time;
    u64 end_time;
} Profiler;

static Profiler global_profiler = {0};
static u64 global_parent_index = 0;

#ifdef ENABLE_PROFILING

// TODO: come up with a better name for this
typedef struct {
    u64 start_time;
    u64 elapsed_inclusive;
    u64 timestamp_index;
    u64 timestamp_parent_index;
    const char *name;
} ProfilerTimestampTemp;


static ProfilerTimestampTemp start_block(u64 index, const char *name, u64 bytes_count) {
    ProfilerTimestampTemp tp = {
        .elapsed_inclusive = global_profiler.timestamps[index].elapsed_inclusive,
        .timestamp_index = index,
        .timestamp_parent_index = global_parent_index,
        .name = name
    };
    global_profiler.timestamps[index].bytes_count = bytes_count;
    global_parent_index = tp.timestamp_index;
    tp.start_time = read_cpu_timer();
    return tp;
}

static void end_block(ProfilerTimestampTemp *tp) {
    u64 elapsed = read_cpu_timer() - tp->start_time;
    ProfilerTimestamp *saved_ts = &global_profiler.timestamps[tp->timestamp_index];

    saved_ts->elapsed_inclusive = tp->elapsed_inclusive + elapsed;
    saved_ts->elapsed_exclusive += elapsed;
    global_profiler.timestamps[tp->timestamp_parent_index].elapsed_exclusive -= elapsed;

    saved_ts->hits += 1;
    saved_ts->name = tp->name;
    global_parent_index = tp->timestamp_parent_index;
}

#define CONCAT_INNER(a, b) a##b
#define CONCAT(a, b) CONCAT_INNER(a, b)

#define TIME_BANDWIDTH(name, bytes) \
    __attribute__((cleanup(end_block))) ProfilerTimestampTemp CONCAT(PROFILER_TIMESTAMP_, __LINE__) = start_block(__COUNTER__ + 1, (name), (bytes))

#define TIME_BLOCK(name) TIME_BANDWIDTH((name), 0)
#define TIME_FUNCTION TIME_BLOCK(__func__)
#define TIME_FUNCTION_BANDWIDTH(size) TIME_BANDWIDTH(__func__, (size))

#define EMPTY()
#define DEFER(...) __VA_ARGS__ EMPTY()
#define EVAL(...) __VA_ARGS__

#define PROFILER_END static_assert(EVAL(DEFER(__COUNTER__)) < MAX_TIMESTAMPS, "More timestamps were added than allowed. Either increase MAX_TIMESTAMPS or reduce profile points")


static void print_timestamps(u64 total, u64 cpu_freq) {
    for (size_t i = 1; i < MAX_TIMESTAMPS; i++) {
        ProfilerTimestamp ts = global_profiler.timestamps[i];
        if (ts.elapsed_inclusive) {
            printf("%s [%lu]: %lu (%.2f%%",
                   ts.name, ts.hits,
                   ts.elapsed_exclusive,
                   (double)ts.elapsed_exclusive/(double)total * 100);
            if (ts.elapsed_inclusive != ts.elapsed_exclusive) {
                printf(", %.2f%% w/children)", (double)ts.elapsed_inclusive/(double)total * 100);
            } else {
                putchar(')');
            }
            if (ts.bytes_count) {
                double bytes_count_mb = (double)ts.bytes_count/(1024.0*1024.0);
                double elapsed_seconds = (double)ts.elapsed_inclusive / (double)cpu_freq;
                double throughput_gbps = (double)ts.bytes_count / (1024.0*1024.0*1024.0*elapsed_seconds);
                printf(" (%.3fMB at %.2f GB/s)", bytes_count_mb, throughput_gbps);
            }
            putchar('\n');
        }
    }
}

#else // ENABLE_PROFILING
#define TIME_FUNCTION
#define TIME_FUNCTION_BANDWIDTH(size)
#define TIME_BLOCK(...)
#define print_timestamps(...)
#define PROFILER_END
#endif  // ENABLE_PROFILING


static void begin_profiling() {
    global_profiler.start_time = read_cpu_timer();
    global_parent_index = 0;
}

static void end_profiling_and_print_stats() {
    global_profiler.end_time = read_cpu_timer();
    u64 total = global_profiler.end_time - global_profiler.start_time;

    u64 cpu_freq = estimate_cpu_timer_freq();
    if (cpu_freq) {
        double total_time = (double)total/(double)cpu_freq;
        if (total_time < 1) {
            printf("Total Time: %.4fms\n", 1000.0 * total_time);
        } else {
            printf("Total Time: %.4fs\n", total_time);
        }
    }
    print_timestamps(total, cpu_freq);
    memset(&global_profiler, 0, sizeof(global_profiler));
    memset(&global_profiler.timestamps, 0, sizeof(*global_profiler.timestamps)*MAX_TIMESTAMPS);
    global_parent_index = 0;
}


#endif // PROFILER_H_IMPLEMENTATION
#endif // PROFILER_H

