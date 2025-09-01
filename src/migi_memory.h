#ifndef MIGI_MEMORY_H
#define MIGI_MEMORY_H

#include <stddef.h>
#define PROFILER_H_IMPLEMENTATION
#include "profiler.h"

#ifdef _WIN32
#error "windows not yet supported!"
#else

#include <sys/mman.h>
#include <errno.h>

#define OS_PAGE_SIZE 4*KB

// TODO(2025-08-30): Linux allows overcommitting upto a certain level, so its possible to make 
// the commit and decommit functions no-ops, and reserve can add the read/write protections itself
// This will still fail for allocations that are much higher than the system RAM
// (seems to be just equal or lesser than 1.5x the available RAM)

static void *memory_reserve(size_t size) {
    TIME_FUNCTION;
    void *mem = mmap(0, size, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    avow(mem != MAP_FAILED, "%s: failed to map memory: %s", __func__, strerror(errno));
    return mem;
}

static void memory_release(void *mem, size_t size) {
    TIME_FUNCTION;
    int ret = munmap(mem, size);
    avow(ret != -1, "%s: failed to unmap memory: %s", __func__, strerror(errno));
}

// NOTE: memory_commit and memory_decommit must be passed in addresses aligned to the OS page boundary
static void *memory_commit(void *mem, size_t size) {
    TIME_FUNCTION;
    void *allocation = mmap(mem, size, PROT_READ|PROT_WRITE, MAP_FIXED|MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    avow(allocation != MAP_FAILED, "%s: failed to commit memory: %s", __func__, strerror(errno));
    return allocation;
}

// NOTE: memory_commit and memory_decommit must be passed in addresses aligned to the OS page boundary
static void memory_decommit(void *mem, size_t size) {
    TIME_FUNCTION;
    void *new_mem = mmap(mem, size, PROT_NONE, MAP_FIXED|MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    avow(new_mem != MAP_FAILED, "%s: failed to decommit memory: %s", __func__, strerror(errno));
}

static void *memory_alloc(size_t size) {
    TIME_FUNCTION;
    void *mem = mmap(0, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    avow(mem != MAP_FAILED, "%s: failed to decommit memory: %s", __func__, strerror(errno));
    return mem;
}


#define memory_free(mem, size) (memory_release((mem), (size)))

#endif
#endif // MIGI_MEMORY_H
