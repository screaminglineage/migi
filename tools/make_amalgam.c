// A single file with all the main headers concatenated to have a single header library
// Inspired by SQLite Amalgamation

#define DEFAULT_OUTPUT_PATH S("gen/migi_amalgam.h")

#include "migi.h"
#include "cli_parse.h"
#include "file.h"
#include "filepath.h"
#include "filesystem.h"

int main(int argc, char **argv) {
    Arena *a = arena_init();

    Str *output_path = cli_add_str(S("out"), S("amalgam output path"),
                                   .value=DEFAULT_OUTPUT_PATH,
                                   .aliases=str_span(S("o")));
    bool *show_headers = cli_add_bool(S("headers"), S("show all the included headers"),
                                   .aliases=str_span(S("H")));

    if (!cli_parse_args(argc, argv, .help=S("Generate a single amalgamated header "
                                           "containing all the main headers in migi"))) return 1;

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
        S("src/random.h"),
    };

    if (*show_headers) {
        printf("Included Headers:\n");
        for (size_t i = 0; i < array_len(files); i++) {
            Str header = str_skip(files[i], S("src/").length);
            printf("  %.*s\n", SArg(header));
        }
    }

    StrList amalgam = {0};
    strlist_push(a, &amalgam, S("#ifndef MIGI_AMALGAM_H\n"));
    strlist_push(a, &amalgam, S("#define MIGI_AMALGAM_H\n\n"));

    Str no_crt_warnings = S("#if COMPILER_MSVC\n"
        "// Disabling microsoft's \"security\" warnings\n"
        "// https://learn.microsoft.com/en-us/cpp/c-runtime-library/security-features-in-the-crt?view=msvc-170#eliminating-deprecation-warnings\n"
        "    #define _CRT_SECURE_NO_WARNINGS\n"
        "#endif\n\n");
    strlist_push(a, &amalgam, no_crt_warnings);

    for (size_t i = 0; i < array_len(files); i++) {
        Str str = str_from_file(a, files[i]);
        strcut_foreach(str, S("\n"), 0, line) {
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

    Str output_dir = path_dirname(*output_path, S("/"));
    if (!dir_make_if_not_exists(output_dir))                     return 1;
    if (!str_to_file(strlist_to_str(a, &amalgam), *output_path)) return 1;
    migi_log(Log_Info, "Generated '%.*s'", SArg(*output_path));

    return 0;
}
