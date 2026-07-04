// A single file with all the basics
// Inspired by SQLite Amalgamation


#include "migi.h"
#include "file.h"

int main() {
    Str files[] = {
        S("src/migi_core.h"),
        S("src/migi_math.h"),
        S("src/timing.h"),
        S("src/profiler.h"),
        S("src/migi_memory.h"),
        S("src/arena.h"),
        S("src/migi_string.h"),
        S("src/migi_list.h"),
        S("src/hashmap.h"),
        S("src/file.h"),
        S("src/cli_parse.h"),
        S("src/migi_random.h"),
    };

    Arena *a = arena_init();

    StrList amalgam = {0};
    strlist_push(a, &amalgam, S("#ifndef MIGI_AMALGAM_H\n"));
    strlist_push(a, &amalgam, S("#define MIGI_AMALGAM_H\n\n"));

    Str no_crt_warnings = S("#ifdef OS_WINDOWS\n"
        "// Disabling microsoft's \"security\" warnings\n"
        "// https://learn.microsoft.com/en-us/cpp/c-runtime-library/security-features-in-the-crt?view=msvc-170#eliminating-deprecation-warnings\n"
        "    #define _CRT_SECURE_NO_WARNINGS\n"
        "#endif\n\n");
    strlist_push(a, &amalgam, no_crt_warnings);

    for (size_t i = 0; i < array_len(files); i++) {
        Str str = str_from_file(a, files[i]);
        strcut_foreach(str, S("\n"), 0, line) {
            if (str_eq(files[i], S("src/profiler.h"))) {
                strlist_push(a, &amalgam, S("#define PROFILER_H_IMPLEMENTATION\n"));
            }

            // Skip local includes as all the needed files are simply included
            // TODO: improve the parsing to take multi-line comments into account
            if (str_starts_with(str_trim_left(line.split), S("#include \""))) {
                continue;
            }
            strlist_push(a, &amalgam, line.split);
            strlist_push(a, &amalgam, S("\n"));
        }
    }
    strlist_push(a, &amalgam, S("#endif // #ifndef MIGI_AMALGAM_H\n"));
    if (!str_to_file(strlist_to_str(a, &amalgam), S("src/migi_amalgam.h"))) {
        return 1;
    }

    return 0;
}
