#ifndef MIGI_MEMORY_H
#define MIGI_MEMORY_H

#include "migi.h"
#include <stddef.h>
#include <stdio.h>
#define PROFILER_H_IMPLEMENTATION
#include "profiler.h"

#define align_up_page_size(n) (align_up((n), memory_page_size()))
#define align_down_page_size(n) (align_down((n), memory_page_size()))

static size_t memory_page_size();
static void *memory_reserve(size_t size);
static void memory_release(void *mem, size_t size);

// NOTE: memory_commit and memory_decommit must be passed in addresses aligned to the OS page boundary
static void *memory_commit(void *mem, size_t size);
static void memory_decommit(void *mem, size_t size);
static void *memory_alloc(size_t size);

#ifdef _WIN32

static size_t memory_page_size() {
    SYSTEM_INFO info = {0};
    GetSystemInfo(&info);
    return info.dwPageSize;
}

static void *memory_reserve(size_t size) {
    TIME_FUNCTION;
    void *mem = VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_NOACCESS);
    assertf(mem != NULL, "%s: failed to map memory: %ld", __func__, GetLastError());
    return mem;
}

static void memory_release(void *mem, size_t size) {
    TIME_FUNCTION;
    // not needed by VirtualFree
    (void)size;
    BOOL ret = VirtualFree(mem, 0, MEM_RELEASE);
    assertf(ret != 0, "%s: failed to unmap memory: %ld", __func__, GetLastError());
}

static void *memory_commit(void *mem, size_t size) {
    void *committed_mem = VirtualAlloc(mem, size, MEM_COMMIT, PAGE_READWRITE);
    assertf(committed_mem != NULL, "%s: failed to commit memory: %ld", __func__, GetLastError());
    return committed_mem;
}

static void memory_decommit(void *mem, size_t size) {
    TIME_FUNCTION;
    // VirtualFree has special case behaviour for `size` = 0 and fails
    // if that case is not met with `487: Attempt to access invalid address.`
    if (size == 0) return;
    BOOL ret = VirtualFree(mem, size, MEM_DECOMMIT);
    assertf(ret != 0, "%s: failed to decommit memory: %ld", __func__, GetLastError());
}

static void *memory_alloc(size_t size) {
    TIME_FUNCTION;
    void *mem = VirtualAlloc(NULL, size, MEM_RESERVE|MEM_COMMIT, 0);
    assertf(mem != NULL, "%s: failed to allocate memory: %ld", __func__, GetLastError());
    return mem;
}
#else

#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>

static size_t memory_page_size() {
    return getpagesize();
}

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

static void *memory_commit(void *mem, size_t size) {
    TIME_FUNCTION;
    int ret = mprotect(mem, size, PROT_READ|PROT_WRITE);
    avow(ret != -1, "%s: failed to commit memory: %s", __func__, strerror(errno));
    return mem;
}

static void memory_decommit(void *mem, size_t size) {
    TIME_FUNCTION;
    madvise(mem, size, MADV_DONTNEED);
    int ret = mprotect(mem, size, PROT_NONE);
    avow(ret != -1, "%s: failed to decommit memory: %s", __func__, strerror(errno));
}

static void *memory_alloc(size_t size) {
    TIME_FUNCTION;
    void *mem = mmap(0, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    avow(mem != MAP_FAILED, "%s: failed to decommit memory: %s", __func__, strerror(errno));
    return mem;
}


#endif
#endif // MIGI_MEMORY_H
