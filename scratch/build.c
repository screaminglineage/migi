#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "migi.h"
#include "../scratch/cli_parse.h"
#include "migi_core.h"
#include "migi_string.h"


#define COMPILER SV("gcc")
#define BUILD_FOLDER SV("./build")

void command_to_args(Arena *arena, char **command_args, StringList *command) {
    StringNode *arg = command->head;
    for (size_t i = 0; i < command->length; i++, arg = arg->next) {
        command_args[i] = string_to_cstr(arena, arg->string);
    }
}

static int run_command(StringList *command) {
    if (command->length == 0) return 0;
    Temp tmp = arena_temp();

    Temp mark = arena_save(tmp.arena);
    migi_log(Log_Info, "Running: %.*s", SV_FMT(strlist_join(tmp.arena, command, SV(" "))));

    arena_rewind(mark);

    pid_t child_exit_code = -1;
    pid_t ret = fork();
    switch (ret) {
        case -1: {
            migi_log(Log_Error, "Failed to create child process: %s\n", strerror(errno));
        } break;

        case 0: {
            char **command_args = arena_push(tmp.arena, char *, command->length + 1);
            command_to_args(tmp.arena, command_args, command);
            command_args[command->length] = NULL;

            int ret = execvp(command_args[0], command_args);
            if (ret == -1) {
                migi_log(Log_Error, "Failed to run `%s`: %s\n", command_args[0], strerror(errno));
                exit(1);
            }
            migi_unreachable();
        } break;

        default: {
            int wstatus;
            pid_t child_pid = waitpid(ret, &wstatus, 0);
            if (child_pid == -1) {
                migi_log(Log_Error, "Failed to wait on child process: %s\n", strerror(errno));
            } else {
                if (WIFEXITED(wstatus)) {
                    child_exit_code = WEXITSTATUS(wstatus);
                } else if (WIFSIGNALED(wstatus)) {
                    migi_log(Log_Error, "Child process killed by signal: %d\n", WTERMSIG(wstatus));
                }
            }
        } break;
    }

    arena_temp_release(tmp);
    return child_exit_code;
}

static bool run_compiler(String compiler, bool debug, String filename, String output_path) {
    Temp tmp = arena_temp();
    StringList command = {0};
    strlist_push(tmp.arena, &command, compiler);

    strlist_push(tmp.arena, &command, filename);
    strlist_push(tmp.arena, &command, SV("-o"));
    strlist_push(tmp.arena, &command, output_path);

    strlist_push(tmp.arena, &command, SV("-I./src"));
    strlist_push(tmp.arena, &command, SV("-Wall"));
    strlist_push(tmp.arena, &command, SV("-Wextra"));
    strlist_push(tmp.arena, &command, SV("-Wno-unused-function"));
    strlist_push(tmp.arena, &command, SV("-Wno-override-init"));
    strlist_push(tmp.arena, &command, SV("-lm"));

    if (debug) {
        strlist_push(tmp.arena, &command, SV("-ggdb"));
        strlist_push(tmp.arena, &command, SV("-DMIGI_DEBUG_LOGS"));
        strlist_push(tmp.arena, &command, SV("-fsanitize=undefined,address"));
    } else {
        strlist_push(tmp.arena, &command, SV("-O3"));
        strlist_push(tmp.arena, &command, SV("-DMIGI_DISABLE_ASSERTS"));
    }

    bool ok = run_command(&command) == 0;
    arena_temp_release(tmp);
    return ok;
}

String filename_to_output_path(Arena *arena, String filename, String build_folder) {
    // scratch/main.c => build/main
    String output_name = filename;
    String extension = SV(".c");
    if (!string_ends_with(output_name, extension)) {
        migi_log(Log_Error, "Unknown file type: `%.*s`. Only .c files are supported for compilation", SV_FMT(output_name));
        return SV("");
    }
    output_name = string_take(output_name, output_name.length - extension.length);

    int64_t basename_start = string_find_char_rev(output_name, '/');
    output_name = string_skip(output_name, basename_start + 1);

    return stringf(arena,"%.*s/%.*s", SV_FMT(build_folder), SV_FMT(output_name));
}

int main(int argc, char **argv) {
    // options which are freestanding
    StringSlice free_keys = slice_from(String, StringSlice, 
        SV("run"), SV("r"),
        SV("optimize"), SV("O"),
        SV("help"), SV("h"));

    Arena *arena = arena_init();
    FlagTable cli = cli_parse_args_ex(arena, argc, argv, free_keys);

    if (flag_exists(&cli, SV("h")) || flag_exists(&cli, SV("help"))) {
        todof("print help");
        return 0;
    }


    bool run = flag_exists(&cli, SV("r")) || flag_exists(&cli, SV("run"));
    bool debug = !(flag_exists(&cli, SV("O")) || flag_exists(&cli, SV("optimize")));

    if (cli.args.length == 0) {
        migi_log(Log_Error, "no file to compile");
        return 1;
    }
    String filename = strlist_pop(&cli.args);
    migi_log(Log_Info, "Compiling%s: %.*s", run? " and Running": "", SV_FMT(filename));

    String output_path = filename_to_output_path(arena, filename, BUILD_FOLDER);
    if (!run_compiler(COMPILER, debug, filename, output_path)) {
        return 1;
    }

    StringList command = {0};
    strlist_push(arena, &command, output_path);
    strlist_extend(&command, &cli.meta_args);

    if (run) {
        int ret = run_command(&command);
        if (ret != 0) {
            migi_log(Log_Error, "Program: `%.*s` exited with code: %d", SV_FMT(output_path), ret);
        }
    }

    return 0;
}
