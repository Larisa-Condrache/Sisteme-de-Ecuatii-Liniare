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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libconfig.h>

#include "config.h"

/* Valori implicite utilizate când lipsește o cheie din fișierul de configurare */
#define DEFAULT_INET_PORT 18081
#define DEFAULT_SOAP_PORT 18082
#define DEFAULT_UNIX_SOCKET "/tmp/unixds"
#define DEFAULT_BACKLOG 10
#define DEFAULT_REUSE_ADDR 1
#define DEFAULT_LOG_LEVEL 1
#define DEFAULT_LOG_DEST "stderr"
#define DEFAULT_DEMO_STRING "Hello from libconfig!"
#define DEFAULT_DEMO_INT1 42
#define DEFAULT_DEMO_INT2 58
#define DEFAULT_SOLVER_WORKERS 4
#define DEFAULT_SOLVER_THRESHOLD 32

server_config_t g_cfg;

/* Inițializează g_cfg cu valori implicite înainte de a aplica configurările din fișier sau CLI */
static void config_set_defaults(void)
{
    g_cfg.inet_port = DEFAULT_INET_PORT;
    g_cfg.soap_port = DEFAULT_SOAP_PORT;
    strncpy(g_cfg.unix_socket, DEFAULT_UNIX_SOCKET,
            sizeof(g_cfg.unix_socket) - 1);
    g_cfg.unix_socket[sizeof(g_cfg.unix_socket) - 1] = '\0';

    g_cfg.backlog      = DEFAULT_BACKLOG;
    g_cfg.reuse_addr   = DEFAULT_REUSE_ADDR;
    g_cfg.log_level    = DEFAULT_LOG_LEVEL;
    strncpy(g_cfg.log_dest, DEFAULT_LOG_DEST, sizeof(g_cfg.log_dest) - 1);
    g_cfg.log_dest[sizeof(g_cfg.log_dest) - 1] = '\0';

    strncpy(g_cfg.demo_string, DEFAULT_DEMO_STRING,
            sizeof(g_cfg.demo_string) - 1);
    g_cfg.demo_string[sizeof(g_cfg.demo_string) - 1] = '\0';
    g_cfg.demo_int1 = DEFAULT_DEMO_INT1;
    g_cfg.demo_int2 = DEFAULT_DEMO_INT2;

    g_cfg.solver_workers   = DEFAULT_SOLVER_WORKERS;
    g_cfg.solver_threshold = DEFAULT_SOLVER_THRESHOLD;
}

/* ------------------------------------------------------------------ */
/*  config_load — parseaza fisierul .cfg cu libconfig                 */
/* ------------------------------------------------------------------ */
int config_load(const char *cfg_path)
{
    config_set_defaults();

    config_t cfg;
    config_init(&cfg);

    if (config_read_file(&cfg, cfg_path) == CONFIG_FALSE) {
        fprintf(stderr, "[config] Cannot read '%s': %s (line %d)\n",
                cfg_path,
                config_error_text(&cfg),
                config_error_line(&cfg));
        config_destroy(&cfg);
        return -1;
    }

    int ival;
    long long llval;
    const char *sval;

    if (config_lookup_int(&cfg, "inet_port", &ival) == CONFIG_TRUE)
        g_cfg.inet_port = (uint16_t)ival;

    if (config_lookup_int(&cfg, "soap_port", &ival) == CONFIG_TRUE)
        g_cfg.soap_port = (uint16_t)ival;

    if (config_lookup_string(&cfg, "unix_socket", &sval) == CONFIG_TRUE) {
        strncpy(g_cfg.unix_socket, sval, sizeof(g_cfg.unix_socket) - 1);
        g_cfg.unix_socket[sizeof(g_cfg.unix_socket) - 1] = '\0';
    }

    if (config_lookup_int(&cfg, "backlog", &ival) == CONFIG_TRUE)
        g_cfg.backlog = ival;

    if (config_lookup_int(&cfg, "reuse_addr", &ival) == CONFIG_TRUE)
        g_cfg.reuse_addr = ival;

    if (config_lookup_int(&cfg, "log_level", &ival) == CONFIG_TRUE)
        g_cfg.log_level = ival;

    if (config_lookup_string(&cfg, "log_dest", &sval) == CONFIG_TRUE) {
        strncpy(g_cfg.log_dest, sval, sizeof(g_cfg.log_dest) - 1);
        g_cfg.log_dest[sizeof(g_cfg.log_dest) - 1] = '\0';
    }

    if (config_lookup_string(&cfg, "demo_string", &sval) == CONFIG_TRUE) {
        strncpy(g_cfg.demo_string, sval, sizeof(g_cfg.demo_string) - 1);
        g_cfg.demo_string[sizeof(g_cfg.demo_string) - 1] = '\0';
    }

    if (config_lookup_int64(&cfg, "demo_int1", &llval) == CONFIG_TRUE)
        g_cfg.demo_int1 = (int)llval;
    else if (config_lookup_int(&cfg, "demo_int1", &ival) == CONFIG_TRUE)
        g_cfg.demo_int1 = ival;

    if (config_lookup_int64(&cfg, "demo_int2", &llval) == CONFIG_TRUE)
        g_cfg.demo_int2 = (int)llval;
    else if (config_lookup_int(&cfg, "demo_int2", &ival) == CONFIG_TRUE)
        g_cfg.demo_int2 = ival;

    if (config_lookup_int(&cfg, "solver_workers", &ival) == CONFIG_TRUE)
        g_cfg.solver_workers = ival;

    if (config_lookup_int(&cfg, "solver_threshold", &ival) == CONFIG_TRUE)
        g_cfg.solver_threshold = ival;

    config_destroy(&cfg);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  config_apply_args — suprascrie g_cfg cu optiunile CLI             */
/*  Valoarea -1 / NULL inseamna "nespecificat de user", se ignora.    */
/* ------------------------------------------------------------------ */
void config_apply_args(int inet_port, int soap_port,
                       const char *unix_socket, int log_level)
{
    if (inet_port > 0)
        g_cfg.inet_port = (uint16_t)inet_port;

    if (soap_port > 0)
        g_cfg.soap_port = (uint16_t)soap_port;

    if (unix_socket != NULL) {
        strncpy(g_cfg.unix_socket, unix_socket,
                sizeof(g_cfg.unix_socket) - 1);
        g_cfg.unix_socket[sizeof(g_cfg.unix_socket) - 1] = '\0';
    }

    if (log_level >= 0)
        g_cfg.log_level = log_level;
}

/* ------------------------------------------------------------------ */
/*  config_print — afiseaza configuratia activa la stderr             */
/* ------------------------------------------------------------------ */
void config_print(void)
{
    fprintf(stderr,
            "[config] Active configuration:\n"
            "         inet_port        = %u\n"
            "         soap_port        = %u\n"
            "         unix_socket      = %s\n"
            "         backlog          = %d\n"
            "         reuse_addr       = %d\n"
            "         log_level        = %d\n"
            "         log_dest         = %s\n"
            "         demo_string      = %s\n"
            "         demo_int1        = %d\n"
            "         demo_int2        = %d\n"
            "         solver_workers   = %d\n"
            "         solver_threshold = %d\n",
            (unsigned)g_cfg.inet_port,
            (unsigned)g_cfg.soap_port,
            g_cfg.unix_socket,
            g_cfg.backlog,
            g_cfg.reuse_addr,
            g_cfg.log_level,
            g_cfg.log_dest,
            g_cfg.demo_string,
            g_cfg.demo_int1,
            g_cfg.demo_int2,
            g_cfg.solver_workers,
            g_cfg.solver_threshold);
}