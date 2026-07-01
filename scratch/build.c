#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "migi.h"
#include "cli_parse_new.h"
#include "process.h"

#define COMPILER S("gcc")
#define BUILD_FOLDER S("./build")

void command_to_args(Arena *arena, char **command_args, StrList *command) {
    StrNode *arg = command->head;
    for (size_t i = 0; i < command->length; i++, arg = arg->next) {
        command_args[i] = str_to_cstr(arena, arg->string);
    }
}

static int run_command(StrList *command, bool run_in_shell, bool run_in_bg) {
    if (command->length == 0) return 0;
    Temp tmp = arena_temp();

    Temp mark = arena_save(tmp.arena);
    migi_log(Log_Info, "Running: %.*s", SArg(strlist_join(tmp.arena, command, S(" "))));

    arena_rewind(mark);

    pid_t child_exit_code = -1;
    pid_t ret = fork();
    switch (ret) {
        case -1: {
            migi_log(Log_Error, "Failed to create child process: %s", strerror(errno));
        } break;

        case 0: {
            char **command_args = NULL;
            if (run_in_shell) {
                strlist_push(tmp.arena, command, S("\0"));  // will convert to a cstring when joined
                command_args = arena_push(tmp.arena, char *, 4 + run_in_bg);
                int i = 0;
                command_args[i++] = "sh";
                command_args[i++] = "-c";
                command_args[i++] = strlist_join(tmp.arena, command, S(" ")).data;
                if (run_in_bg) {
                    command_args[i++] = "&";
                }
                command_args[i++] = NULL;
            } else {
                command_args = arena_push(tmp.arena, char *, command->length + 1);
                command_to_args(tmp.arena, command_args, command);
                command_args[command->length] = NULL;
            }

            int ret = execvp(command_args[0], command_args);
            if (ret == -1) {
                migi_log(Log_Error, "Failed to run `%s`: %s", command_args[0], strerror(errno));
                exit(1);
            }
            migi_unreachable();
        } break;

        default: {
            int wstatus;
            pid_t child_pid = waitpid(ret, &wstatus, 0);
            if (child_pid == -1) {
                migi_log(Log_Error, "Failed to wait on child process: %s", strerror(errno));
            } else {
                if (WIFEXITED(wstatus)) {
                    child_exit_code = WEXITSTATUS(wstatus);
                } else if (WIFSIGNALED(wstatus)) {
                    migi_log(Log_Error, "Child process killed by: %s", strsignal(WTERMSIG(wstatus)));
                }
            }
        } break;
    }

    arena_temp_release(tmp);
    return child_exit_code;
}

static bool run_compiler(Str compiler, bool optimize, Str filename, Str output_path) {
    Temp tmp = arena_temp();
    StrList command = {0};
    strlist_push(tmp.arena, &command, compiler);

    strlist_push(tmp.arena, &command, filename);
    strlist_push(tmp.arena, &command, S("-o"));
    strlist_push(tmp.arena, &command, output_path);

    strlist_push(tmp.arena, &command, S("-I./src"));
    strlist_push(tmp.arena, &command, S("-Wall"));
    strlist_push(tmp.arena, &command, S("-Wextra"));
    strlist_push(tmp.arena, &command, S("-Wno-unused-function"));
    strlist_push(tmp.arena, &command, S("-Wno-override-init"));
    strlist_push(tmp.arena, &command, S("-Wno-missing-braces")); // dont warn on specific kinds of designated initializers
    strlist_push(tmp.arena, &command, S("-lm"));

    if (optimize) {
        strlist_push(tmp.arena, &command, S("-O3"));
        strlist_push(tmp.arena, &command, S("-DMIGI_DISABLE_ASSERTS"));
    } else {
        strlist_push(tmp.arena, &command, S("-ggdb"));
        strlist_push(tmp.arena, &command, S("-DMIGI_DEBUG_LOGS"));
        strlist_push(tmp.arena, &command, S("-fsanitize=undefined,address"));
    }

    bool ok = run_command(&command, false, false) == 0;
    arena_temp_release(tmp);
    return ok;
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

    bool *run      = cli_add_bool(S("run"),      S("run the program after compiling"), .aliases=str_span(S("r")));
    bool *optimize = cli_add_bool(S("optimize"), S("enable optimizations"),            .aliases=str_span(S("O")));
    bool *debug    = cli_add_bool(S("debug"),    S("debug program in gf2"),            .aliases=str_span(S("d")));
    bool *help     = cli_add_bool(S("help"),     S("show this help message"),          .aliases=str_span(S("h")));

    if (!cli_parse_args(argc, argv, .arena = arena)) return 1;

    if (*run && *debug) {
        migi_log(Log_Error, "options '-%.*s' and '-%.*s' are mutually exclusive", 
                SArg(cli_var_to_arg(run)->name), SArg(cli_var_to_arg(optimize)->name));
        return 1;
    }

    if (global_cli.pos_args.length == 0) {
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

    Str executable_path = filename_to_output_path(arena, filename, BUILD_FOLDER);
    if (!run_compiler(COMPILER, *optimize, filename, executable_path)) {
        return 1;
    }

    if (*debug || *run) {
        Cmd command = {.arena=arena};
        if (*debug) cmd_push(&command, S("gf2"));

        cmd_push(&command, executable_path);
        strlist_extend(&command.args, &cli_meta_args());
        CmdResult res = cmd_run(&command, .shell=*debug, .background=*debug);

        if (res.code != 0) {
            migi_log(Log_Error, "Program: `%.*s` exited with code: %d", SArg(executable_path), res.code);
            return 1;
        }
    }

    return 0;
}
