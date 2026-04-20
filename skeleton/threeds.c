/*
Punct de intrare pentru serverul PCD.
 Responsabilitati
   1. Parseaza argumentele din CLI cu getopt()
   2. Citeste variabile de mediu
   3. Incarca configuratia via libconfig
   4. Suprascrie config-ul cu argumente CLI
   5. Ruleaza un demo fork()/wait() folosind cfg
   6. Porneste cele trei thread-uri de server
 Compilare:
   gcc -g threeds.c config.c unixds.c inetds2.c soapds.c proto.c \
       soapC.c soapServer.c \
       -lpthread -lgsoap -lconfig -Wall -Wextra -o serverds
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

void *unix_main(void *args);
void *inet_main(void *args);
void *soap_main(void *args);

pthread_mutex_t curmtx = PTHREAD_MUTEX_INITIALIZER;

static void demo_fork(void)
{
    int pipefd[2];
    pid_t pid;
    int wstatus;

    if (pipe(pipefd) == -1)
    {
        perror("[demo] pipe");
        return;
    }

    pid = fork();
    if (pid < 0)
    {
        perror("[demo] fork");
        close(pipefd[0]);
        close(pipefd[1]);
        return;
    }
    if (pid == 0)
    {
        close(pipefd[0]); /* copilul doar scrie */

        /* Saduna cei doi intregi  */
        int sum = g_cfg.demo_int1 + g_cfg.demo_int2;
        if (write(pipefd[1], &sum, sizeof(sum)) == -1)
        {
            perror("[demo:child] write sum");
            close(pipefd[1]);
            _exit(EXIT_FAILURE);
        }

        int neg = -g_cfg.demo_int1;
        if (write(pipefd[1], &neg, sizeof(neg)) == -1)
        {
            perror("[demo:child] write neg");
            close(pipefd[1]);
            _exit(EXIT_FAILURE);
        }

        size_t slen = strlen(g_cfg.demo_string) + 1;
        if (write(pipefd[1], g_cfg.demo_string, slen) == -1)
        {
            perror("[demo:child] write string");
            close(pipefd[1]);
            _exit(EXIT_FAILURE);
        }

        close(pipefd[1]);
        _exit(EXIT_SUCCESS);
    }

    close(pipefd[1]); /* parintele doar citeste */

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

int main(int argc, char *argv[])

    const char *env_cfg = getenv("PCD_CONFIG");
const char *cfg_path = (env_cfg != NULL) ? env_cfg : "server.cfg";

int env_log_level = -1;
const char *env_ll = getenv("PCD_LOG_LEVEL");
if (env_ll != NULL)
    env_log_level = atoi(env_ll);

/*  Parseaza argumentele  cu getopt_long()  */

int opt_inet_port = -1;
int opt_soap_port = -1;
int opt_log_level = env_log_level; /* env e baza; CLI are prioritate */
const char *opt_unix_socket = NULL;
const char *opt_config = NULL;

static const struct option long_opts[] = {
    {"config", required_argument, NULL, 'c'},
    {"port", required_argument, NULL, 'p'},
    {"soap-port", required_argument, NULL, 's'},
    {"unix-socket", required_argument, NULL, 'u'},
    {"log-level", required_argument, NULL, 'l'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0}};

int c;
while ((c = getopt_long(argc, argv, "c:p:s:u:l:h",
                        long_opts, NULL)) != -1)
{
    switch (c)
    {
    case 'c':
        opt_config = optarg;
        break;
    case 'p':
        opt_inet_port = atoi(optarg);
        break;
    case 's':
        opt_soap_port = atoi(optarg);
        break;
    case 'u':
        opt_unix_socket = optarg;
        break;
    case 'l':
        opt_log_level = atoi(optarg);
        break;
    case 'h':
        print_usage(argv[0]);
        return EXIT_SUCCESS;
    default:
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
}

if (opt_config != NULL)
    cfg_path = opt_config;

/*  Incarca fisierul de config  */

if (config_load(cfg_path) != 0)
{
    fprintf(stderr, "[main] Warning: running with compiled-in defaults.\n");
}

config_apply_args(opt_inet_port, opt_soap_port,
                  opt_unix_socket, opt_log_level);

config_print();

fprintf(stderr, "[main] Running fork()/wait() demo...\n");
demo_fork();

pthread_t unixthr, inetthr, soapthr;
int iport = g_cfg.inet_port;
int sport = g_cfg.soap_port;

unlink(g_cfg.unix_socket);

if (pthread_create(&unixthr, NULL, unix_main,
                   g_cfg.unix_socket) != 0)
{
    perror("[main] pthread_create unix_main");
    return EXIT_FAILURE;
}

if (pthread_create(&inetthr, NULL, inet_main, &iport) != 0)
{
    perror("[main] pthread_create inet_main");
    return EXIT_FAILURE;
}

if (pthread_create(&soapthr, NULL, soap_main, &sport) != 0)
{
    perror("[main] pthread_create soap_main");
    return EXIT_FAILURE;
}

/* Blocheaza pana termina toate thread-urile */
pthread_join(unixthr, NULL);
pthread_join(inetthr, NULL);
pthread_join(soapthr, NULL);

unlink(g_cfg.unix_socket);
return EXIT_SUCCESS;
}
