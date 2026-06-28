#include <sys/wait.h>

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
    cmd__push((cmd), span_from(Str, StrSpan, __VA_ARGS__ ))

typedef struct {
    // TODO: implement these
    bool capture_stdout;
    bool capture_stderr;
    bool shell;             // run through the shell

    bool no_reset;          // dont reset the cmd after running the command
} CmdOpt;

static CmdResult cmd_run_opt(Arena *a, Cmd *cmd, CmdOpt opt);

#define cmd_run(arena, cmd, ...) \
    cmd_run_opt((arena), (cmd), (CmdOpt){__VA_ARGS__})


static void cmd__push(Cmd *cmd, StrSpan args) {
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

static CmdResult cmd_run_opt(Arena *a, Cmd *cmd, CmdOpt opt) {
    // TODO: If stdout or stderr was captured, they are stored in the arena
    unused(a);
    unused(opt);

    CmdResult result = {0};

    if (cmd->args.length == 0) return result;

    Temp tmp = arena_temp();

    Temp mark = arena_save(tmp.arena);
    migi_log(Log_Info, "Running: %.*s", SArg(strlist_join(tmp.arena, &cmd->args, S(" "))));
    arena_rewind(mark);

    pid_t child_exit_code = -1;
    pid_t ret = fork();
    switch (ret) {
        case -1: {
            migi_log(Log_Error, "Failed to create child process: %s", strerror(errno));
        } break;

        case 0: {
            char **command_args = cmd__to_args(tmp.arena, cmd);
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
    result.code = child_exit_code;

    if (!opt.no_reset) {
        arena_reset(cmd->arena);
        strlist_reset(&cmd->args);
    }

    return result;
}

int main() {
    Temp tmp = arena_temp();
    Cmd cmd = {.arena=tmp.arena};
    cmd_push(&cmd, S("ls"), S("-l"));
    cmd_push(&cmd, S("-a"));
    CmdResult res = cmd_run(tmp.arena, &cmd, .capture_stdout=true, .capture_stderr=true);
    printf("res = %d\n", res.code);

    printf("%p %p %zu %zu\n", cmd.args.head, cmd.args.tail, cmd.args.length, cmd.args.total_size);

    arena_temp_release(tmp);

    return 0;
}
