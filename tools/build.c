// TODO: automatically rebuild this file if it is newer than build/build
// TODO: add support for choosing the compiler (clang/gcc on linux for the time being)

// TODO: add support for both forward and backslashes on windows somehow
// TODO: this file cannot replace the old exe when compiled on windows, deal with that somehow

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "migi.h"
#include "cli_parse.h"
#include "process.h"
#include "filesystem.h"

#if OS_LINUX
    #define COMPILER S("gcc")
#elif OS_WINDOWS
    #define COMPILER S("cl")
    #define VCVARS_PATH S("C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat")
#else
    #error "Unsupported OS"
#endif

#define BUILD_FOLDER S("./build")


void command_to_args(Arena *arena, char **command_args, StrList *command) {
    StrNode *arg = command->head;
    for (size_t i = 0; i < command->length; i++, arg = arg->next) {
        command_args[i] = str_to_cstr(arena, arg->string);
    }
}

static Cmd prepare_compiler(Str compiler, bool optimize, bool sanitizers, Str filename, Str output_path) {
    unused(sanitizers);
    Cmd command = {0};
    cmd_push(&command, compiler);

#if OS_LINUX
    cmd_push(&command, filename);
    cmd_push(&command, S("-o"));
    cmd_push(&command, output_path);

    cmd_push(&command, S("-I./src"));
    cmd_push(&command, S("-Wall"));
    cmd_push(&command, S("-Wextra"));
    cmd_push(&command, S("-Wno-unused-function"));
    cmd_push(&command, S("-Wno-override-init"));
    cmd_push(&command, S("-Wno-missing-braces")); // dont warn on specific kinds of designated initializers
    cmd_push(&command, S("-lm"));

    if (optimize) {
        cmd_push(&command, S("-O3"));
        cmd_push(&command, S("-DMIGI_DISABLE_ASSERTS"));
    } else {
        cmd_push(&command, S("-ggdb"));
        cmd_push(&command, S("-DMIGI_DEBUG_LOGS"));
        if (sanitizers) cmd_push(&command, S("-fsanitize=undefined,address"));
    }

#elif OS_WINDOWS
    // Resolves the absolute path of filename instead of using a relative path here
    char *input_absolute_path = arena_push_nonzero(command.arena, char, MAX_PATH);
    if (!GetFullPathNameA(str_to_cstr(command.arena, filename),
                          MAX_PATH, input_absolute_path, NULL)) {
        migi_log(Log_Error, "Failed to resolve absolute path of input file: %.*s",
                SArg(str_last_error(command.arena)));
        return (Cmd){0};
    }
    Str input_absolute_path_str = str_from_cstr(input_absolute_path);
    cmd_push(&command, input_absolute_path_str);

    // Getting the basename since it sets the CWD to the build directory on windows
    Str exe_path = strf(command.arena, "/Fe%.*s", SArg(path_basename(output_path, S("\\"))));
    cmd_push(&command, exe_path);

    cmd_push(&command, S("/nologo"));

    cmd_push(&command, S("/I../src"));

    // TODO: explain all the warnings which have been turned off
    cmd_push(&command, S("/W4"));
    cmd_push(&command, S("/wd4200"));
    cmd_push(&command, S("/wd4146"));
    cmd_push(&command, S("/wd4127"));
    cmd_push(&command, S("/wd4034"));
    cmd_push(&command, S("/wd4201"));
    cmd_push(&command, S("/wd4189"));

    cmd_push(&command, S("/std:c11"));


    if (optimize) {
        cmd_push(&command, S("/O2"));
        cmd_push(&command, S("/DMIGI_DISABLE_ASSERTS"));
    } else {
        cmd_push(&command, S("/Zi"));
        cmd_push(&command, S("/DMIGI_DEBUG_LOGS"));
        // if (sanitizers) cmd_push(&command, S("/fsanitize=address")); // TODO: seems to not work?
    }
    cmd_push_many(&command, S("/link"), S("/INCREMENTAL:NO"));

#else
#error "Unsupported OS"
#endif

    return command;
}

Str filename_to_output_path(Arena *arena, Str filename, Str build_folder) {
    // scratch/main.c => build/main
    Str output_name = filename;
    Str extension = S(".c");
    if (!str_ends_with(output_name, extension)) {
        migi_log(Log_Error, "Unknown file type: `%.*s`. Only .c files are supported for compilation", SArg(output_name));
        return S("");
    }
    output_name = str_take(output_name, output_name.length - extension.length);

#if OS_WINDOWS
    Str path_separator = S("\\");
#else
    Str path_separator = S("/");
#endif
    int64_t basename_start = str_find_opt(output_name, path_separator, Find_Reverse);
    output_name = str_skip(output_name, basename_start + 1);

#if OS_WINDOWS
    return strf(arena,"%.*s\\%.*s", SArg(build_folder), SArg(output_name));
#else
    return strf(arena,"%.*s/%.*s", SArg(build_folder), SArg(output_name));
#endif
}

int main(int argc, char **argv) {
    Arena *arena = arena_init();

    bool *run           = cli_add_bool(S("run"),        S("run the program after compiling"),               .aliases=str_span(S("r")));
    bool *run_old       = cli_add_bool(S("run-old"),    S("run the old executable without compiling"),      .aliases=str_span(S("ro")));
    bool *dry_run       = cli_add_bool(S("dry-run"),    S("only print the compiler invocation"),            .aliases=str_span(S("dr")));
    Str  *output        = cli_add_str (S("output"),     S("path to output executable"),                     .aliases=str_span(S("o")));
    bool *optimize      = cli_add_bool(S("optimize"),   S("enable optimizations"),                          .aliases=str_span(S("O")));
    bool *debug         = cli_add_bool(S("debug"),      S("debug program in gf2"),                          .aliases=str_span(S("d")));
    bool *sanitizers    = cli_add_bool(S("sanitizers"), S("add sanitizers"),  .value=true, .takes_arg=true, .aliases=str_span(S("s")));
    bool *help          = cli_add_bool(S("help"),       S("show this help message"),                        .aliases=str_span(S("h")));

    if (!cli_parse_args(argc, argv, .help=S("Build and Run C Files"))) return 1;

    if (*run && *debug) {
        migi_log(Log_Error, "options '-%.*s' and '-%.*s' are mutually exclusive", 
                SArg(cli_var_name(run)), SArg(cli_var_name(debug)));
        return 1;
    }

    // Sanitizers cause certain issues when debugging which are annoying
    if (*debug) {
        migi_log(Log_Info, "Disabling sanitizers since the program is running in a debugger");
        *sanitizers = false;
    }
    if (!*sanitizers) {
        migi_log(Log_Info, "Compiling without sanitizers");
    }

    if (cli_pos_args().length == 0) {
        migi_log(Log_Error, "no file to compile");
        fprintf(stderr, "%.*s", SArg(cli_help_text(arena)));
        return 1;
    }

    if (*help) {
        fprintf(stderr, "%.*s", SArg(cli_help_text(arena)));
        return 1;
    }

    Str filename = cli_pos_args().head->string;
    // TODO: if `run_old` is set then print "Running previous executable"
    if (!*run_old) {
        migi_log(Log_Info, "Compiling%s: '%.*s'", run? " and Running": "", SArg(filename));
    } else if (*run || *run_old) {
        migi_log(Log_Info, "Running: '%.*s'", SArg(filename));
    }

    Str executable_path = output->length != 0
        ? *output
        : filename_to_output_path(arena, filename, BUILD_FOLDER);

    if (!*run_old) {
        Cmd command = prepare_compiler(COMPILER, *optimize, *sanitizers, filename, executable_path);
        if (*dry_run) {
            migi_log(Log_Info, "Compiling (Dry Run): %.*s", SArg(strlist_join(arena, &command.args, S(" "))));
            cmd_reset(&command);
        } else {

#if OS_WINDOWS
            bool cl_found = true;
            char *cl_path = arena_push(command.arena, char, MAX_PATH);
            if (SearchPathA(NULL, "cl", ".exe", MAX_PATH, cl_path, NULL) == 0) {
                if (GetLastError() == ERROR_FILE_NOT_FOUND) {
                    cl_found = false;
                }
            }
            StrList args = {0};
            if (!cl_found) {
                migi_log(Log_Info, "Could not find 'cl.exe' in PATH, running vcvars.bat");
                strlist_push(command.arena, &args, VCVARS_PATH);
                strlist_push(command.arena, &args, S(">nul"));
                strlist_push(command.arena, &args, S("2>nul"));
                strlist_push(command.arena, &args, S("&&"));
            }

            // migi_log(Log_Info, "Running: %.*s", command);
            command.args = strlist_extend(&args, &command.args);
            // cmd_push(&command, S("\""));

            Str prev_cwd = get_cwd(arena);
            if (!set_cwd(S("./build"))) return 1;
            CmdResult result = cmd_run(&command, .shell=true, /*.no_log_cmd=true*/);
            if (!set_cwd(prev_cwd)) return 1;

#else
            CmdResult result = cmd_run(&command);
#endif

            if (result.error || result.code != 0) {
                return 1;
            }
        }
    }

    if (*debug || *run || *run_old) {
        Cmd command = {0};
#if OS_LINUX
        if (*debug) cmd_push(&command, S("gf2"));
        cmd_push(&command, executable_path);

        // Arguments passed to GDB
        if (*debug) {
            // Start running the program immediately
            cmd_push_many(&command, S("-ex"), S("start"));
            if (cli_meta_args().length > 0) {
                // This ensures that the meta args are handled correctly
                cmd_push(&command, S("--args"));
                cmd_push(&command, executable_path);
            }
        }
#elif OS_WINDOWS
        if (*debug) cmd_push(&command, S("raddbg"));
        cmd_push(&command, executable_path);
#else
#error "Unsupported OS"
#endif
        // Pass meta args to the program being ran
        strlist_extend(&command.args, &cli_meta_args());

        if (*dry_run) {
            migi_log(Log_Info, "Running (Dry Run): %.*s", SArg(strlist_join(arena, &command.args, S(" "))));
            cmd_reset(&command);
        } else {
            CmdResult res = cmd_run(&command, .shell=*debug, .background=*debug);

            if (res.code != 0) {
                migi_log(Log_Error, "Program: `%.*s` exited with code: %d", SArg(executable_path), res.code);
                return 1;
            }
        }
    }

    return 0;
}
