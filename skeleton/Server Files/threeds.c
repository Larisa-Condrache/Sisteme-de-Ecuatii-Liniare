/*
 * threeds.c — PCD server entry point.
 *
 * Responsibilities (Milestone 1):
 *   1. Parse CLI arguments with getopt()          (--port, --soap-port, …)
 *   2. Read environment variables                  (PCD_CONFIG, PCD_LOG_LEVEL)
 *   3. Load configuration via libconfig           (config_load)
 *   4. Override config with CLI args              (config_apply_args)
 *   5. Run a fork()/wait() demo using the config  (demo_fork)
 *   6. Start the three server threads             (UNIX / INET / SOAP)
 *
 * Build:
 *   gcc -g threeds.c config.c unixds.c inetds2.c soapds.c proto.c \
 *       soapC.c soapServer.c \
 *       -lpthread -lgsoap -lconfig -Wall -Wextra -o serverds
 */

#include <stddef.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>

#include "config.h"

/* ------------------------------------------------------------------ */
/*  Thread entry-points declared in the other translation units       */
/* ------------------------------------------------------------------ */

void *unix_main(void *args);
void *inet_main(void *args);
void *soap_main(void *args);

/* ------------------------------------------------------------------ */
/*  Mutex shared with other modules (e.g. for future ncurses use)     */
/* ------------------------------------------------------------------ */

pthread_mutex_t curmtx = PTHREAD_MUTEX_INITIALIZER;

/* ------------------------------------------------------------------ */
/*  fork()-based demo: child runs simple tasks using libconfig values */
/* ------------------------------------------------------------------ */

/*
 * demo_fork() — Demonstrates mandatory Milestone 1 requirements:
 *   • fork() / wait()
 *   • use of at least one external library (libconfig, already loaded)
 *   • reading values from the parsed configuration in a child process
 *
 * The child performs three tasks from g_cfg.demo_* values and writes
 * results back to the parent via a pipe.  The parent reads and prints
 * them.
 *
 * Pipe layout (child writes, parent reads):
 *   [0..3]  int   : sum  (demo_int1 + demo_int2)
 *   [4..7]  int   : neg  (-demo_int1)
 *   [8..N]  char* : echo of demo_string, null-terminated
 */
static void demo_fork(void)
{
    int    pipefd[2];
    pid_t  pid;
    int    wstatus;

    if (pipe(pipefd) == -1) {
        perror("[demo] pipe");
        return;
    }

    pid = fork();
    if (pid < 0) {
        perror("[demo] fork");
        close(pipefd[0]);
        close(pipefd[1]);
        return;
    }

    /* ---- CHILD --------------------------------------------------- */
    if (pid == 0) {
        close(pipefd[0]); /* child only writes */

        /* Task 1: add the two demo integers */
        int sum = g_cfg.demo_int1 + g_cfg.demo_int2;
        if (write(pipefd[1], &sum, sizeof(sum)) == -1) {
            perror("[demo:child] write sum");
            close(pipefd[1]);
            _exit(EXIT_FAILURE);
        }

        /* Task 2: negate demo_int1 */
        int neg = -g_cfg.demo_int1;
        if (write(pipefd[1], &neg, sizeof(neg)) == -1) {
            perror("[demo:child] write neg");
            close(pipefd[1]);
            _exit(EXIT_FAILURE);
        }

        /* Task 3: echo demo_string (include null terminator) */
        size_t slen = strlen(g_cfg.demo_string) + 1;
        if (write(pipefd[1], g_cfg.demo_string, slen) == -1) {
            perror("[demo:child] write string");
            close(pipefd[1]);
            _exit(EXIT_FAILURE);
        }

        close(pipefd[1]);
        _exit(EXIT_SUCCESS);
    }

    /* ---- PARENT -------------------------------------------------- */
    close(pipefd[1]); /* parent only reads */

    int sum = 0, neg = 0;
    char echo_buf[256];
    memset(echo_buf, 0, sizeof(echo_buf));

    if (read(pipefd[0], &sum, sizeof(sum)) != sizeof(sum))
        perror("[demo:parent] read sum");

    if (read(pipefd[0], &neg, sizeof(neg)) != sizeof(neg))
        perror("[demo:parent] read neg");

    ssize_t n = read(pipefd[0], echo_buf, sizeof(echo_buf) - 1);
    if (n < 0)
        perror("[demo:parent] read string");
    else
        echo_buf[n] = '\0';

    close(pipefd[0]);

    /* Reap child — mandatory to avoid zombies */
    if (waitpid(pid, &wstatus, 0) == -1)
        perror("[demo:parent] waitpid");

    fprintf(stderr,
            "[demo] fork()/wait() results from child (PID %d):\n"
            "       %d + %d = %d\n"
            "       -(%d)   = %d\n"
            "       echo    = \"%s\"\n",
            (int)pid,
            g_cfg.demo_int1, g_cfg.demo_int2, sum,
            g_cfg.demo_int1, neg,
            echo_buf);
}

/* ------------------------------------------------------------------ */
/*  CLI usage                                                          */
/* ------------------------------------------------------------------ */

static void print_usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s [OPTIONS]\n"
            "\n"
            "Options:\n"
            "  -c, --config <path>       Path to server.cfg  "
            "[env: PCD_CONFIG, default: server.cfg]\n"
            "  -p, --port <n>            INET TCP port        "
            "[default: 18081]\n"
            "  -s, --soap-port <n>       SOAP port            "
            "[default: 18082]\n"
            "  -u, --unix-socket <path>  UNIX socket path     "
            "[default: /tmp/unixds]\n"
            "  -l, --log-level <0|1|2>   Log verbosity        "
            "[env: PCD_LOG_LEVEL, default: 1]\n"
            "  -h, --help                Show this help\n",
            prog);
}

/* ------------------------------------------------------------------ */
/*  main                                                               */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
    /* ---- 1. Read environment variables before parsing CLI -------- */

    /* PCD_CONFIG: overrides the default config file path */
    const char *env_cfg = getenv("PCD_CONFIG");
    const char *cfg_path = (env_cfg != NULL) ? env_cfg : "server.cfg";

    /* PCD_LOG_LEVEL: pre-seed log level (CLI can override later) */
    int env_log_level = -1;
    const char *env_ll = getenv("PCD_LOG_LEVEL");
    if (env_ll != NULL)
        env_log_level = atoi(env_ll);

    /* ---- 2. Parse CLI arguments with getopt_long() --------------- */

    int opt_inet_port  = -1;
    int opt_soap_port  = -1;
    int opt_log_level  = env_log_level; /* env is the base; CLI wins  */
    const char *opt_unix_socket = NULL;
    const char *opt_config      = NULL;

    static const struct option long_opts[] = {
        { "config",      required_argument, NULL, 'c' },
        { "port",        required_argument, NULL, 'p' },
        { "soap-port",   required_argument, NULL, 's' },
        { "unix-socket", required_argument, NULL, 'u' },
        { "log-level",   required_argument, NULL, 'l' },
        { "help",        no_argument,       NULL, 'h' },
        { NULL, 0, NULL, 0 }
    };

    int c;
    while ((c = getopt_long(argc, argv, "c:p:s:u:l:h",
                            long_opts, NULL)) != -1) {
        switch (c) {
        case 'c': opt_config      = optarg;        break;
        case 'p': opt_inet_port   = atoi(optarg);  break;
        case 's': opt_soap_port   = atoi(optarg);  break;
        case 'u': opt_unix_socket = optarg;        break;
        case 'l': opt_log_level   = atoi(optarg);  break;
        case 'h': print_usage(argv[0]); return EXIT_SUCCESS;
        default:  print_usage(argv[0]); return EXIT_FAILURE;
        }
    }

    /* CLI --config wins over PCD_CONFIG env var */
    if (opt_config != NULL)
        cfg_path = opt_config;

    /* ---- 3. Load config file via libconfig ----------------------- */

    if (config_load(cfg_path) != 0) {
        fprintf(stderr, "[main] Warning: running with compiled-in defaults.\n");
    }

    /* ---- 4. CLI arguments override the file ---------------------- */

    config_apply_args(opt_inet_port, opt_soap_port,
                      opt_unix_socket, opt_log_level);

    /* Print the final merged configuration */
    config_print();

    /* ---- 5. fork()/wait() demo with libconfig values ------------- */

    fprintf(stderr, "[main] Running fork()/wait() demo...\n");
    demo_fork();

    /* ---- 6. Start server threads --------------------------------- */

    pthread_t unixthr, inetthr, soapthr;
    int       iport = g_cfg.inet_port;
    int       sport = g_cfg.soap_port;

    /* Remove stale UNIX socket before binding */
    unlink(g_cfg.unix_socket);

    if (pthread_create(&unixthr, NULL, unix_main,
                       g_cfg.unix_socket) != 0) {
        perror("[main] pthread_create unix_main");
        return EXIT_FAILURE;
    }

    if (pthread_create(&inetthr, NULL, inet_main, &iport) != 0) {
        perror("[main] pthread_create inet_main");
        return EXIT_FAILURE;
    }

    if (pthread_create(&soapthr, NULL, soap_main, &sport) != 0) {
        perror("[main] pthread_create soap_main");
        return EXIT_FAILURE;
    }

    /* Block until all threads finish (they loop forever in practice) */
    pthread_join(unixthr, NULL);
    pthread_join(inetthr, NULL);
    pthread_join(soapthr, NULL);

    unlink(g_cfg.unix_socket);
    return EXIT_SUCCESS;
}
