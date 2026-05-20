/*
 * lsh.c — Little Shell
 * Based on Stephen Brennan's tutorial: https://brennan.io/2015/01/16/write-a-shell-in-c/
 *
 * Builtins implemented:
 *   cd, help, exit   — original
 *   pwd              — Step 3 (mandatory)
 *   echo             — Step 3 (mandatory)
 *   history          — Step 3 (mandatory)
 *   env              — Step 3 (optional)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

/* ─────────────────────────────────────────────
   History
   ───────────────────────────────────────────── */
#define HISTORY_MAX 100
static char *history[HISTORY_MAX];
static int   history_count = 0;

static void history_add(const char *line)
{
    if (history_count < HISTORY_MAX) {
        history[history_count++] = strdup(line);
    } else {
        /* Ring-buffer: drop the oldest entry */
        free(history[0]);
        memmove(history, history + 1, (HISTORY_MAX - 1) * sizeof(char *));
        history[HISTORY_MAX - 1] = strdup(line);
    }
}

static void history_free(void)
{
    for (int i = 0; i < history_count; i++) {
        free(history[i]);
        history[i] = NULL;
    }
    history_count = 0;
}

/* ─────────────────────────────────────────────
   Forward declarations for builtins
   ───────────────────────────────────────────── */
int lsh_cd(char **args);
int lsh_help(char **args);
int lsh_exit(char **args);
int lsh_pwd(char **args);
int lsh_echo(char **args);
int lsh_history(char **args);
int lsh_env(char **args);

/* ─────────────────────────────────────────────
   Builtin tables
   ───────────────────────────────────────────── */
char *builtin_str[] = {
    "cd",
    "help",
    "exit",
    "pwd",
    "echo",
    "history",
    "env"
};

int (*builtin_func[]) (char **) = {
    &lsh_cd,
    &lsh_help,
    &lsh_exit,
    &lsh_pwd,
    &lsh_echo,
    &lsh_history,
    &lsh_env
};

int lsh_num_builtins(void)
{
    return sizeof(builtin_str) / sizeof(char *);
}

/* ─────────────────────────────────────────────
   Builtin: cd
   ───────────────────────────────────────────── */
int lsh_cd(char **args)
{
    if (args[1] == NULL) {
        fprintf(stderr, "lsh: cd: missing argument\n");
    } else if (args[2] != NULL) {
        fprintf(stderr, "lsh: cd: too many arguments\n");
    } else {
        if (chdir(args[1]) != 0) {
            perror("lsh: cd");
        }
    }
    return 1;
}

/* ─────────────────────────────────────────────
   Builtin: help
   ───────────────────────────────────────────── */
int lsh_help(char **args)
{
    (void)args;
    printf("Little Shell (lsh) — based on Stephen Brennan's tutorial\n");
    printf("Built-in commands:\n");
    for (int i = 0; i < lsh_num_builtins(); i++) {
        printf("  %s\n", builtin_str[i]);
    }
    printf("For external programs, just type their name.\n");
    return 1;
}

/* ─────────────────────────────────────────────
   Builtin: exit
   ───────────────────────────────────────────── */
int lsh_exit(char **args)
{
    (void)args;
    history_free();
    return 0;   /* signals the main loop to quit */
}

/* ─────────────────────────────────────────────
   Builtin: pwd
   Uses: getcwd()
   ───────────────────────────────────────────── */
int lsh_pwd(char **args)
{
    (void)args;
    char buf[4096];
    if (getcwd(buf, sizeof(buf)) == NULL) {
        perror("lsh: pwd");
    } else {
        printf("%s\n", buf);
    }
    return 1;
}

/* ─────────────────────────────────────────────
   Builtin: echo
   Uses: printf() / loop over args
   ───────────────────────────────────────────── */
int lsh_echo(char **args)
{
    /* echo with no arguments prints a blank line */
    for (int i = 1; args[i] != NULL; i++) {
        if (i > 1) printf(" ");
        printf("%s", args[i]);
    }
    printf("\n");
    return 1;
}

/* ─────────────────────────────────────────────
   Builtin: history
   Uses: internal array
   ───────────────────────────────────────────── */
int lsh_history(char **args)
{
    (void)args;
    if (history_count == 0) {
        printf("lsh: history: no commands yet\n");
    } else {
        for (int i = 0; i < history_count; i++) {
            printf("%4d  %s\n", i + 1, history[i]);
        }
    }
    return 1;
}

/* ─────────────────────────────────────────────
   Builtin: env  (optional)
   Uses: extern char **environ
   ───────────────────────────────────────────── */
extern char **environ;

int lsh_env(char **args)
{
    (void)args;
    for (char **ep = environ; *ep != NULL; ep++) {
        printf("%s\n", *ep);
    }
    return 1;
}

/* ─────────────────────────────────────────────
   Launch an external program
   ───────────────────────────────────────────── */
int lsh_launch(char **args)
{
    pid_t pid = fork();

    if (pid < 0) {
        /* fork failed */
        perror("lsh: fork");
        return 1;
    }

    if (pid == 0) {
        /* Child process */
        if (execvp(args[0], args) == -1) {
            perror("lsh");
            exit(EXIT_FAILURE);
        }
    }

    /* Parent process: wait for child */
    int status;
    pid_t wpid;
    do {
        wpid = waitpid(pid, &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    (void)wpid;

    return 1;
}

/* ─────────────────────────────────────────────
   Execute a command (builtin or external)
   ───────────────────────────────────────────── */
int lsh_execute(char **args)
{
    if (args[0] == NULL) {
        /* Empty command — do nothing */
        return 1;
    }

    for (int i = 0; i < lsh_num_builtins(); i++) {
        if (strcmp(args[0], builtin_str[i]) == 0) {
            return (*builtin_func[i])(args);
        }
    }

    return lsh_launch(args);
}

/* ─────────────────────────────────────────────
   Read a line of input
   ───────────────────────────────────────────── */
#define LSH_RL_BUFSIZE 1024

char *lsh_read_line(void)
{
    char *line   = NULL;
    size_t bufsize = 0;

    if (getline(&line, &bufsize, stdin) == -1) {
        if (feof(stdin)) {
            /* Ctrl-D */
            printf("\n");
            history_free();
            exit(EXIT_SUCCESS);
        }
        perror("lsh: getline");
        exit(EXIT_FAILURE);
    }

    /* Strip trailing newline */
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n') {
        line[len - 1] = '\0';
    }

    return line;
}

/* ─────────────────────────────────────────────
   Split a line into tokens
   ───────────────────────────────────────────── */
#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM   " \t\r\n\a"

char **lsh_split_line(char *line)
{
    int    bufsize = LSH_TOK_BUFSIZE;
    int    position = 0;
    char **tokens = malloc(bufsize * sizeof(char *));
    char  *token;

    if (!tokens) {
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, LSH_TOK_DELIM);
    while (token != NULL) {
        tokens[position++] = token;

        if (position >= bufsize) {
            bufsize += LSH_TOK_BUFSIZE;
            tokens = realloc(tokens, bufsize * sizeof(char *));
            if (!tokens) {
                fprintf(stderr, "lsh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, LSH_TOK_DELIM);
    }
    tokens[position] = NULL;
    return tokens;
}

/* ─────────────────────────────────────────────
   Main loop
   ───────────────────────────────────────────── */
void lsh_loop(void)
{
    char  *line;
    char **args;
    int    status;

    do {
        printf("lsh> ");
        fflush(stdout);

        line = lsh_read_line();

        /* Skip blank lines — don't add them to history */
        if (line[0] != '\0') {
            history_add(line);
        }

        args   = lsh_split_line(line);
        status = lsh_execute(args);

        free(line);
        free(args);

    } while (status);
}

/* ─────────────────────────────────────────────
   Entry point
   ───────────────────────────────────────────── */
int main(void)
{
    lsh_loop();
    return EXIT_SUCCESS;
}
