#ifndef PROCESS_H
#define PROCESS_H

#include <sys/wait.h>

#include "migi_core.h"
#include "arena.h"
#include "migi_list.h"

typedef struct {
    Arena *arena;
    StrList args;
} Cmd;

typedef struct {
    int code;
    StrList stdout;
    StrList stderr;
} CmdResult;

#define cmd_push(cmd, ...) \
    cmd__push((cmd), str_span(__VA_ARGS__ ))

typedef struct {
    // TODO: implement these
    Arena *arena;           // arena to store captured output in (must be set if those options are provided)
    bool capture_stdout;
    bool capture_stderr;

    bool shell;             // run through the shell
    bool background;        // run in background (only possible when running through shell)
    bool no_reset;          // dont reset the cmd after running the command
    bool no_log_cmd;        // dont log the command being executed
} CmdOpt;

static CmdResult cmd_run_opt(Cmd *cmd, CmdOpt opt);

#define cmd_run(cmd, ...) \
    cmd_run_opt((cmd), (CmdOpt){__VA_ARGS__})

static void cmd_reset(Cmd *cmd);



static void cmd__push(Cmd *cmd, StrSpan args) {
    if (!cmd->arena) cmd->arena = arena_init(.type=Arena_Linear);
    array_foreach(&args, arg) {
        strlist_push(cmd->arena, &cmd->args, *arg);
    }
}


// Converts cmd to args to pass to exec functions
char **cmd__to_args(Arena *arena, Cmd *cmd) {
    char **args = arena_push(arena, char *, cmd->args.length + 1);
    args[cmd->args.length] = NULL;

    size_t i = 0;
    strlist_foreach(&cmd->args, arg) {
        args[i++] = str_to_cstr(arena, arg->string);
    }

    return args;
}

static CmdResult cmd_run_opt(Cmd *cmd, CmdOpt opt) {
    CmdResult result = {0};
    if (cmd->args.length == 0) return result;

    LogLevel prev_log_level = MIGI_GLOBAL_LOG_LEVEL;
    if (opt.no_log_cmd) {
        migi_log_set_level(Log_Error);
    }

    Temp tmp = arena_save(cmd->arena);
    migi_log(Log_Info, "Running: %.*s", SArg(strlist_join(tmp.arena, &cmd->args, S(" "))));

    pid_t child_exit_code = -1;
    pid_t ret = fork();
    switch (ret) {
        case -1: {
            migi_log(Log_Error, "Failed to create child process: %s", strerror(errno));
        } break;

        case 0: {
            char **command_args = NULL;
            if (opt.shell) {
                strlist_push(tmp.arena, &cmd->args, S("\0"));  // will convert to a cstring when joined
                command_args = arena_push(tmp.arena, char *, 4 + opt.background);
                int i = 0;
                command_args[i++] = "sh";
                command_args[i++] = "-c";
                command_args[i++] = strlist_join(tmp.arena, &cmd->args, S(" ")).data;
                if (opt.background) {
                    command_args[i++] = "&";
                }
                command_args[i++] = NULL;
            } else {
                command_args = cmd__to_args(tmp.arena, cmd);
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

    arena_rewind(tmp);
    result.code = child_exit_code;

    if (!opt.no_reset) {
        cmd_reset(cmd);
    }
    MIGI_GLOBAL_LOG_LEVEL = prev_log_level;
    return result;
}


static void cmd_reset(Cmd *cmd) {
    arena_reset(cmd->arena);
    strlist_reset(&cmd->args);
}

#endif // ifndef PROCESS_H

