#ifndef MIGI_FILESYSTEM_H
#define MIGI_FILESYSTEM_H

// IWYU pragma: begin_exports
#ifdef _WIN32
    #include "filesystem_windows.h"
#else
    #include "filesystem_linux.h"
#endif // ifdef _WIN32
// IWYU pragma: end_exports


#endif // ifndef MIGI_FILESYSTEM_H
