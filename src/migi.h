// Include this header to pull in all the basic dependencies

#ifndef MIGI_H
#define MIGI_H

#ifdef _MSC_VER
// Disabling microsoft's "security" warnings
// https://learn.microsoft.com/en-us/cpp/c-runtime-library/security-features-in-the-crt?view=msvc-170#eliminating-deprecation-warnings
    #define _CRT_SECURE_NO_WARNINGS
// MSVC needs this macro to define math constants (M_PI, etc.)
    #define _USE_MATH_DEFINES
#endif

// IWYU - Include What You Use pragma is needed by clangd to not warn of unused includes
// https://clangd.llvm.org/guides/include-cleaner#scenarios-and-solutions

#include "migi_core.h"      // IWYU pragma: export
#include "arena.h"          // IWYU pragma: export
#include "migi_string.h"    // IWYU pragma: export
#include "migi_list.h"      // IWYU pragma: export


#endif // ifndef MIGI_H
