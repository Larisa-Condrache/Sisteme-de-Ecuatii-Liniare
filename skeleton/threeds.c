/*
  Verificare build si analiza statica:
    • -Wall si -Wextra semnaleaza probleme generale precum variabile
      nefolosite, formate gresite sau apeluri suspecte.
    • -Wpedantic forteaza respectarea mai stricta a standardului C.
    • -Wshadow avertizeaza daca o variabila locala ascunde alta variabila,
      situatie care poate produce bug-uri greu de observat.
    • -Wconversion avertizeaza la conversii implicite care pot pierde
      informatie sau pot schimba semnul unei valori.
    • -Wformat=2 verifica mai strict folosirea functiilor din familia printf.
    • -Wnull-dereference semnaleaza dereferentieri potential invalide.
    • Fisierul este inclus si in tinta „make tidy”, unde clang-tidy face
      o analiza statica suplimentara pentru stil, siguranta si bune practici.
 */

/*
 * threeds.c — Punct de intrare pentru serverul PCD.
 *
 * Responsabilitati (Milestone 1):
 *   1. Parseaza argumentele din CLI cu getopt()    (--port, --soap-port, …)
 *   2. Citeste variabile de mediu                  (PCD_CONFIG, PCD_LOG_LEVEL)
 *   3. Incarca configuratia via libconfig          (config_load)
 *   4. Suprascrie config-ul cu argumente CLI       (config_apply_args)
 *   5. Porneste cele trei thread-uri de server     (UNIX / INET / SOAP)
 *
 * Compilare:
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
#include <pthread.h>

#include "config.h"

/* ------------------------------------------------------------------ */
/*  Puncte de intrare pentru thread-uri declarate in alte unitati     */
/* ------------------------------------------------------------------ */

void *unix_main(void *args);
void *inet_main(void *args);
void *soap_main(void *args);

/* ------------------------------------------------------------------ */
/*  Mutex partajat cu alte module (ex. pentru utilizare ncurses pe viitor) */
/* ------------------------------------------------------------------ */

pthread_mutex_t curmtx = PTHREAD_MUTEX_INITIALIZER;

/* ------------------------------------------------------------------ */
/*  Utilizare CLI                                                      */
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
    /* ---- 1. Citeste variabilele de mediu inainte de parsarea CLI -- */

    /* PCD_CONFIG: suprascrie calea implicita catre fisierul de configurare */
    const char *env_cfg = getenv("PCD_CONFIG");
    const char *cfg_path = (env_cfg != NULL) ? env_cfg : "server.cfg";

    /* PCD_LOG_LEVEL: seteaza nivelul de log (CLI poate suprascrie ulterior) */
    int env_log_level = -1;
    const char *env_ll = getenv("PCD_LOG_LEVEL");
    if (env_ll != NULL)
        env_log_level = atoi(env_ll);

    /* ---- 2. Parseaza argumentele CLI cu getopt_long() ------------ */

    int opt_inet_port  = -1;
    int opt_soap_port  = -1;
    int opt_log_level  = env_log_level; /* env e baza; CLI are prioritate */
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

    /* CLI --config are prioritate fata de variabila de mediu PCD_CONFIG */
    if (opt_config != NULL)
        cfg_path = opt_config;

    /* ---- 3. Incarca fisierul de config via libconfig ------------- */

    if (config_load(cfg_path) != 0) {
        fprintf(stderr, "[main] Warning: running with compiled-in defaults.\n");
    }

    /* ---- 4. Argumentele CLI suprascriu fisierul ------------------ */

    config_apply_args(opt_inet_port, opt_soap_port,
                      opt_unix_socket, opt_log_level);

    /* Afiseaza configuratia finala (rezultata din merge) */
    config_print();

    /* ---- 5. Porneste thread-urile serverului --------------------- */

    pthread_t unixthr, inetthr, soapthr;
    int       iport = g_cfg.inet_port;
    int       sport = g_cfg.soap_port;

    /* Sterge socket-ul UNIX vechi (daca exista) inainte de bind() */
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

    /* Blocheaza pana termina toate thread-urile (in practica ruleaza la infinit) */
    pthread_join(unixthr, NULL);
    pthread_join(inetthr, NULL);
    pthread_join(soapthr, NULL);

    unlink(g_cfg.unix_socket);
    return EXIT_SUCCESS;
}
