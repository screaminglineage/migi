#ifndef CONTEXT_INFO_H
#define CONTEXT_INFO_H

// Get info about OS, compiler, architecture as a string

#include "migi_core.h"
#include "migi_string.h"

static Str os_name() {
#if OS_WINDOWS
    return S("windows");
#elif OS_LINUX
    return S("linux");
#elif OS_MAC
    return S("mac");
#else
    return S("unknown");
#endif
}

static Str compiler_name() {
#if COMPILER_MINGW
    return S("gcc (mingw)");
#elif COMPILER_GCC
    return S("gcc");
#elif COMPILER_CLANG
    return S("clang");
#elif COMPILER_MSVC
    return S("msvc");
#else
    return S("unknown");
#endif
}

static Str architecture_name() {
#if ARCH_X64
    return S("x86_64");
#elif ARCH_X86
    return S("x86");
#elif ARCH_ARM64
    return S("arm64");
#elif ARCH_ARM32
    return S("arm32");
#else
    return S("unknown");
#endif
}

#endif // ifndef CONTEXT_INFO_H
