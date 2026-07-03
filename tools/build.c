// TODO: automatically rebuild this file if it is newer than build/build

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "migi.h"
#include "cli_parse.h"
#include "process.h"

#define COMPILER S("gcc")
#define BUILD_FOLDER S("./build")

void command_to_args(Arena *arena, char **command_args, StrList *command) {
    StrNode *arg = command->head;
    for (size_t i = 0; i < command->length; i++, arg = arg->next) {
        command_args[i] = str_to_cstr(arena, arg->string);
    }
}

static Cmd prepare_compiler(Str compiler, bool optimize, bool sanitizers, Str filename, Str output_path) {
    Cmd command = {0};
    cmd_push(&command, compiler);

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

    int64_t basename_start = str_find_ex(output_name, S("/"), Find_Reverse);
    output_name = str_skip(output_name, basename_start + 1);

    return strf(arena,"%.*s/%.*s", SArg(build_folder), SArg(output_name));
}

int main(int argc, char **argv) {
    Arena *arena = arena_init();

    bool *run           = cli_add_bool(S("run"),        S("run the program after compiling"),               .aliases=str_span(S("r")));
    bool *dry_run       = cli_add_bool(S("dry-run"),    S("only print the compiler flags"),                 .aliases=str_span(S("dr")));
    Str  *output        = cli_add_str (S("output"),     S("path to output executable"),                     .aliases=str_span(S("o")));
    bool *optimize      = cli_add_bool(S("optimize"),   S("enable optimizations"),                          .aliases=str_span(S("O")));
    bool *debug         = cli_add_bool(S("debug"),      S("debug program in gf2"),                          .aliases=str_span(S("d")));
    bool *sanitizers    = cli_add_bool(S("sanitizers"), S("add sanitizers"),  .value=true, .takes_arg=true, .aliases=str_span(S("s")));
    bool *help          = cli_add_bool(S("help"),       S("show this help message"),                        .aliases=str_span(S("h")));

    if (!cli_parse_args(argc, argv)) return 1;

    if (*run && *debug) {
        migi_log(Log_Error, "options '-%.*s' and '-%.*s' are mutually exclusive", 
                SArg(cli_arg_from_var(run)->name), SArg(cli_arg_from_var(debug)->name));
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
    migi_log(Log_Info, "Compiling%s: %.*s", run? " and Running": "", SArg(filename));

    Str executable_path = output->length != 0
        ? *output
        : filename_to_output_path(arena, filename, BUILD_FOLDER);

    Cmd command = prepare_compiler(COMPILER, *optimize, *sanitizers, filename, executable_path);
    if (*dry_run) {
        migi_log(Log_Info, "Compiling (Dry Run): %.*s", SArg(strlist_join(arena, &command.args, S(" "))));
        cmd_reset(&command);
    } else {
        if (cmd_run(&command).code != 0) {
            return 1;
        }
    }

    if (*debug || *run) {
        Cmd command = {0};
        if (*debug) cmd_push(&command, S("gf2"));

        cmd_push(&command, executable_path);

        if (*debug && cli_meta_args().length > 0) {
            // Extra arguments to gf2 are passed to gdb
            // This ensures that the meta args are handled correctly
            cmd_push(&command, S("--args"));
            cmd_push(&command, executable_path);
        }

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
