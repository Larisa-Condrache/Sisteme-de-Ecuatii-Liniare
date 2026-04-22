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

    g_cfg.log_level = DEFAULT_LOG_LEVEL;
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

/*
 * config_load() – parsează fișierul .cfg cu libconfig și populează g_cfg.
 * Returnează 0 la succes, -1 dacă fișierul nu poate fi deschis/parsat.
 * Cheile lipsă sunt lăsate cu valorile implicite setate anterior.
 */
int config_load(const char *cfg_path)
{
    config_t cfg;
    int ival;
    long long llval;
    const char *sval;

    config_set_defaults();

    config_init(&cfg);

    if (config_read_file(&cfg, cfg_path) == CONFIG_FALSE) {
        fprintf(stderr, "[config] Cannot read '%s': %s (line %d)\n",
                cfg_path,
                config_error_text(&cfg),
                config_error_line(&cfg));
        config_destroy(&cfg);
        return -1;
    }

    /* server.* */
    if (config_lookup_int(&cfg, "server.inet_port", &ival) == CONFIG_TRUE)
        g_cfg.inet_port = (uint16_t)ival;
    if (config_lookup_int(&cfg, "server.soap_port", &ival) == CONFIG_TRUE)
        g_cfg.soap_port = (uint16_t)ival;
    if (config_lookup_string(&cfg, "server.unix_socket", &sval) == CONFIG_TRUE) {
        strncpy(g_cfg.unix_socket, sval, sizeof(g_cfg.unix_socket) - 1);
        g_cfg.unix_socket[sizeof(g_cfg.unix_socket) - 1] = '\0';
    }
    if (config_lookup_int(&cfg, "server.backlog", &ival) == CONFIG_TRUE)
        g_cfg.backlog = ival;
    if (config_lookup_int(&cfg, "server.reuse_addr", &ival) == CONFIG_TRUE)
        g_cfg.reuse_addr = ival;

    /* logging.* */
    if (config_lookup_int(&cfg, "logging.level", &ival) == CONFIG_TRUE)
        g_cfg.log_level = ival;
    if (config_lookup_string(&cfg, "logging.destination", &sval) == CONFIG_TRUE) {
        strncpy(g_cfg.log_dest, sval, sizeof(g_cfg.log_dest) - 1);
        g_cfg.log_dest[sizeof(g_cfg.log_dest) - 1] = '\0';
    }

    /* demo.* */
    if (config_lookup_string(&cfg, "demo.input_string", &sval) == CONFIG_TRUE) {
        strncpy(g_cfg.demo_string, sval, sizeof(g_cfg.demo_string) - 1);
        g_cfg.demo_string[sizeof(g_cfg.demo_string) - 1] = '\0';
    }
    if (config_lookup_int(&cfg, "demo.input_int1", &ival) == CONFIG_TRUE)
        g_cfg.demo_int1 = ival;
    if (config_lookup_int(&cfg, "demo.input_int2", &ival) == CONFIG_TRUE)
        g_cfg.demo_int2 = ival;

    /* solver.* */
    if (config_lookup_int(&cfg, "solver.workers", &ival) == CONFIG_TRUE)
        g_cfg.solver_workers = ival;
    if (config_lookup_int(&cfg, "solver.serial_threshold", &ival) == CONFIG_TRUE)
        g_cfg.solver_threshold = ival;

    /* Suprima warning unused pentru llval daca libconfig nu-l folosim altundeva */
    (void)llval;

    config_destroy(&cfg);
    return 0;
}

/*
 * config_apply_args() – suprascrie câmpurile din g_cfg cu valorile
 * primite din CLI. Valorile -1 / NULL înseamnă "nespecificat de user".
 */
void config_apply_args(int inet_port, int soap_port,
                       const char *unix_socket, int log_level)
{
    if (inet_port != -1)
        g_cfg.inet_port = (uint16_t)inet_port;
    if (soap_port != -1)
        g_cfg.soap_port = (uint16_t)soap_port;
    if (unix_socket != NULL) {
        strncpy(g_cfg.unix_socket, unix_socket,
                sizeof(g_cfg.unix_socket) - 1);
        g_cfg.unix_socket[sizeof(g_cfg.unix_socket) - 1] = '\0';
    }
    if (log_level != -1)
        g_cfg.log_level = log_level;
}

/*
 * config_print() – afișează la stderr configurația activă.
 */
void config_print(void)
{
    fprintf(stderr, "[config] inet_port      = %u\n",  (unsigned)g_cfg.inet_port);
    fprintf(stderr, "[config] soap_port      = %u\n",  (unsigned)g_cfg.soap_port);
    fprintf(stderr, "[config] unix_socket    = %s\n",  g_cfg.unix_socket);
    fprintf(stderr, "[config] backlog        = %d\n",  g_cfg.backlog);
    fprintf(stderr, "[config] reuse_addr     = %d\n",  g_cfg.reuse_addr);
    fprintf(stderr, "[config] log_level      = %d\n",  g_cfg.log_level);
    fprintf(stderr, "[config] log_dest       = %s\n",  g_cfg.log_dest);
    fprintf(stderr, "[config] demo_string    = %s\n",  g_cfg.demo_string);
    fprintf(stderr, "[config] demo_int1      = %d\n",  g_cfg.demo_int1);
    fprintf(stderr, "[config] demo_int2      = %d\n",  g_cfg.demo_int2);
    fprintf(stderr, "[config] solver_workers = %d\n",  g_cfg.solver_workers);
    fprintf(stderr, "[config] solver_thresh  = %d\n",  g_cfg.solver_threshold);
}