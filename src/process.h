#ifndef PROCESS_H
#define PROCESS_H

#include "migi_core.h"
#include "arena.h"
#include "migi_list.h"
#include "file.h"

typedef struct {
    Arena *arena;
    StrList args;
    bool owns_arena;    // false if the arena was passed from the outside
} Cmd;

typedef struct {
    int code;
    bool error;
    StrList cmd_stdout;
    StrList cmd_stderr;
} CmdResult;

// NOTE: These two are separate since `str_span("foo")` is suprisingly valid.
// This is because `(Str[]){"foo"}` gets parsed into `(Str){.data="foo", .length=0}`,
// resulting in no errors in case `cmd_push_many(&cmd, "foo")` is used
//
// On the other hand, `cmd_push_many(&cmd, "foo", "bar")`, is not valid since
// it implies `(Str[]){"foo", "bar"}` which is parsed as `(Str){.data="foo", .length="bar"}`
// so everything will be fine as long as multiple elements are passed to cmd_push_many
#define cmd_push(cmd, arg) \
    cmd__push((cmd), (arg))

#define cmd_push_many(cmd, ...) \
    cmd__push_many((cmd), str_span( __VA_ARGS__ ))


typedef struct {
    // TODO: implement these
    Arena *arena;           // arena to store captured output in (must be set if those options are provided)
    bool capture_stdout;
    bool capture_stderr;

    bool shell;             // run through the shell (TODO: seems to be not needed on windows)
    bool background;        // run in background (only possible when running through shell)
    bool no_reset;          // dont reset after running the command, the internal arena is reset only if it wasn't passed from the outside
    bool no_log_cmd;        // dont log the command being executed
} CmdOpt;

static CmdResult cmd_run_opt(Cmd *cmd, CmdOpt opt);

#define cmd_run(cmd, ...) \
    cmd_run_opt((cmd), (CmdOpt){__VA_ARGS__})

static void cmd_reset(Cmd *cmd);
static void cmd_free(Cmd *cmd);

static void cmd__push(Cmd *cmd, Str arg) {
    if (!cmd->arena) {
        cmd->owns_arena = true;
        cmd->arena = arena_init();
    }
    strlist_push(cmd->arena, &cmd->args, arg);
}

static void cmd__push_many(Cmd *cmd, StrSpan args) {
    if (!cmd->arena) {
        cmd->owns_arena = true;
        cmd->arena = arena_init();
    }
    array_foreach(&args, arg) {
        strlist_push(cmd->arena, &cmd->args, *arg);
    }
}

#if OS_WINDOWS

// Taken and adapted from: https://github.com/tsoding/nob.h/
Str win32_quote_command_line(Arena *arena, StrList *cmd) {
    Str quoted = {0};
    size_t i = 0;
    for (StrNode *arg = cmd->head; arg; i++, arg = arg->next) {
        if (i > 0) quoted = str_cat(arena, quoted, S(" "));

        // TODO: does the following need to be ASCII_WHITESPACES instead?
        // Check for more info: https://learn.microsoft.com/en-gb/archive/blogs/twistylittlepassagesallalike/everyone-quotes-command-line-arguments-the-wrong-way
        if (str_find_ex(arg->string, S(" \t\n\v\""), Find_Any) == (int64_t)arg->string.length) {
            // no need to quote
            quoted = str_cat(arena, quoted, arg->string);
        } else {
            // we need to escape:
            // 1. double quotes in the original arg
            // 2. consequent backslashes before a double quote
            size_t backslashes = 0;
            quoted = str_cat(arena, quoted, S("\""));
            for (size_t j = 0; j < arg->string.length; ++j) {
                char x = arg->string.data[j];

                if (x == '\\') {
                    backslashes += 1;
                } else {
                    if (x == '\"') {
                        // escape backslashes (if any) and the double quote
                        for (size_t k = 0; k < 1+backslashes; ++k) {
                            quoted = str_cat(arena, quoted, S("\\"));
                        }
                    }
                    backslashes = 0;
                }
                Str ch = str_from(&x, 1);
                quoted = str_cat(arena, quoted, ch);
            }
            // escape backslashes (if any)
            for (size_t k = 0; k < backslashes; ++k) {
                quoted = str_cat(arena, quoted, S("\\"));
            }
            quoted = str_cat(arena, quoted, S("\""));
        }
    }
    return quoted;
}


static CmdResult cmd_run_opt(Cmd *cmd, CmdOpt opt) {
    CmdResult result = {0};
    if (cmd->args.length == 0) return result;

    LogLevel prev_log_level = MIGI_GLOBAL_LOG_LEVEL;
    if (opt.no_log_cmd) {
        migi_log_set_level(Log_Error);
    }

    Temp tmp = arena_save(cmd->arena);


    Str command_line = win32_quote_command_line(cmd->arena, &cmd->args);
    command_line     = str_cat(cmd->arena, command_line, S("\0"));

    // if (opt.shell) {
    //     command_line = str_cat(cmd->arena, command_line, S("cmd "));
    //     command_line = str_cat(cmd->arena, command_line, S("/s /c \""));
    //     if (opt.background) command_line = str_cat(cmd->arena, command_line, S("start "));
    //     command_line.length += strlist_join(cmd->arena, &cmd->args, S(" ")).length;
    //     command_line = str_cat(cmd->arena, command_line, S("\""));
    // } else {
    //     command_line = strlist_join(cmd->arena, &cmd->args, S(" "));
    // }
    // command_line = str_cat(cmd->arena, command_line, S("\0"));

    migi_log(Log_Info, "Running: '%s'", command_line.data);

    STARTUPINFO info = { .cb = sizeof(info) };
    PROCESS_INFORMATION process_info;
    if (!CreateProcessA(NULL, command_line.data, NULL, NULL, true, 0, NULL, NULL, &info, &process_info)) {
        migi_log(Log_Error, "Failed to run `%.*s`: %.*s",
                SArg(cmd->args.head->string), SArg(str_last_error(tmp.arena)));
        result.error = true;
        goto end;
    }

    if (WaitForSingleObject(process_info.hProcess, INFINITE) == WAIT_FAILED) {
        migi_log(Log_Error, "Failed to wait for child process: %.*s", SArg(str_last_error(tmp.arena)));
        result.error = true;
        goto end;
    }

    DWORD exit_code;
    if (!GetExitCodeProcess(process_info.hProcess, &exit_code)) {
        migi_log(Log_Error, "Failed to get process exit code: %.*s", SArg(str_last_error(tmp.arena)));
        result.error = true;
        goto end;
    }
    result.code = exit_code;

    CloseHandle(process_info.hProcess);
    CloseHandle(process_info.hThread);

end:
    arena_rewind(tmp);

    if (!opt.no_reset) {
        cmd_reset(cmd);
    }

    MIGI_GLOBAL_LOG_LEVEL = prev_log_level;
    return result;
}

#else

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


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


#endif


static void cmd_reset(Cmd *cmd) {
    if (cmd->owns_arena) arena_reset(cmd->arena);
    strlist_reset(&cmd->args);
}

static void cmd_free(Cmd *cmd) {
    if (cmd->owns_arena) arena_free(cmd->arena);
    mem_clear(cmd);
}

#endif // ifndef PROCESS_H

