/*
 * config.c — libconfig integration for the PCD server.
 *
 * Reads server.cfg (or any path given by the caller), populates g_cfg,
 * and lets CLI arguments override individual fields afterwards.
 *
 * Compile:  gcc -c config.c -o config.o -lconfig -Wall -Wextra
 * Link:     add -lconfig to the final binary link step.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libconfig.h>

#include "config.h"

/* ------------------------------------------------------------------ */
/*  Compiled-in defaults (used when a key is missing from the file)   */
/* ------------------------------------------------------------------ */

#define DEFAULT_INET_PORT    18081
#define DEFAULT_SOAP_PORT    18082
#define DEFAULT_UNIX_SOCKET  "/tmp/unixds"
#define DEFAULT_BACKLOG      10
#define DEFAULT_REUSE_ADDR   1
#define DEFAULT_LOG_LEVEL    1
#define DEFAULT_LOG_DEST     "stderr"
#define DEFAULT_DEMO_STRING  "Hello from libconfig!"
#define DEFAULT_DEMO_INT1    42
#define DEFAULT_DEMO_INT2    58
#define DEFAULT_SOLVER_WORKERS    4
#define DEFAULT_SOLVER_THRESHOLD  32

/* ------------------------------------------------------------------ */
/*  Global instance                                                    */
/* ------------------------------------------------------------------ */

server_config_t g_cfg;

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                   */
/* ------------------------------------------------------------------ */

/* Fill g_cfg with safe defaults before applying the file/CLI. */
static void config_set_defaults(void)
{
    g_cfg.inet_port  = DEFAULT_INET_PORT;
    g_cfg.soap_port  = DEFAULT_SOAP_PORT;
    strncpy(g_cfg.unix_socket, DEFAULT_UNIX_SOCKET,
            sizeof(g_cfg.unix_socket) - 1);
    g_cfg.unix_socket[sizeof(g_cfg.unix_socket) - 1] = '\0';

    g_cfg.backlog    = DEFAULT_BACKLOG;
    g_cfg.reuse_addr = DEFAULT_REUSE_ADDR;

    g_cfg.log_level  = DEFAULT_LOG_LEVEL;
    strncpy(g_cfg.log_dest, DEFAULT_LOG_DEST,
            sizeof(g_cfg.log_dest) - 1);
    g_cfg.log_dest[sizeof(g_cfg.log_dest) - 1] = '\0';

    strncpy(g_cfg.demo_string, DEFAULT_DEMO_STRING,
            sizeof(g_cfg.demo_string) - 1);
    g_cfg.demo_string[sizeof(g_cfg.demo_string) - 1] = '\0';
    g_cfg.demo_int1  = DEFAULT_DEMO_INT1;
    g_cfg.demo_int2  = DEFAULT_DEMO_INT2;

    g_cfg.solver_workers   = DEFAULT_SOLVER_WORKERS;
    g_cfg.solver_threshold = DEFAULT_SOLVER_THRESHOLD;
}

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

int config_load(const char *cfg_path)
{
    config_t     cfg;       /* libconfig context — stack-allocated     */
    int          itmp;
    const char  *stmp;

    config_set_defaults();  /* start from known-good values            */

    config_init(&cfg);

    if (config_read_file(&cfg, cfg_path) == CONFIG_FALSE) {
        fprintf(stderr,
                "[config] Cannot read '%s': %s (line %d)\n"
                "[config] Continuing with compiled-in defaults.\n",
                cfg_path,
                config_error_text(&cfg),
                config_error_line(&cfg));
        config_destroy(&cfg);
        return -1;
    }

    /* ---- server group -------------------------------------------- */

    if (config_lookup_int(&cfg, "server.inet_port", &itmp) == CONFIG_TRUE)
        g_cfg.inet_port = (uint16_t)itmp;

    if (config_lookup_int(&cfg, "server.soap_port", &itmp) == CONFIG_TRUE)
        g_cfg.soap_port = (uint16_t)itmp;

    if (config_lookup_string(&cfg, "server.unix_socket", &stmp) == CONFIG_TRUE) {
        strncpy(g_cfg.unix_socket, stmp, sizeof(g_cfg.unix_socket) - 1);
        g_cfg.unix_socket[sizeof(g_cfg.unix_socket) - 1] = '\0';
    }

    if (config_lookup_int(&cfg, "server.backlog", &itmp) == CONFIG_TRUE)
        g_cfg.backlog = itmp;

    if (config_lookup_int(&cfg, "server.reuse_addr", &itmp) == CONFIG_TRUE)
        g_cfg.reuse_addr = itmp;

    /* ---- logging group ------------------------------------------- */

    if (config_lookup_int(&cfg, "logging.level", &itmp) == CONFIG_TRUE)
        g_cfg.log_level = itmp;

    if (config_lookup_string(&cfg, "logging.destination", &stmp) == CONFIG_TRUE) {
        strncpy(g_cfg.log_dest, stmp, sizeof(g_cfg.log_dest) - 1);
        g_cfg.log_dest[sizeof(g_cfg.log_dest) - 1] = '\0';
    }

    /* ---- demo group ---------------------------------------------- */

    if (config_lookup_string(&cfg, "demo.input_string", &stmp) == CONFIG_TRUE) {
        strncpy(g_cfg.demo_string, stmp, sizeof(g_cfg.demo_string) - 1);
        g_cfg.demo_string[sizeof(g_cfg.demo_string) - 1] = '\0';
    }

    if (config_lookup_int(&cfg, "demo.input_int1", &itmp) == CONFIG_TRUE)
        g_cfg.demo_int1 = itmp;

    if (config_lookup_int(&cfg, "demo.input_int2", &itmp) == CONFIG_TRUE)
        g_cfg.demo_int2 = itmp;

    /* ---- solver group -------------------------------------------- */

    if (config_lookup_int(&cfg, "solver.workers", &itmp) == CONFIG_TRUE)
        g_cfg.solver_workers = itmp;

    if (config_lookup_int(&cfg, "solver.serial_threshold", &itmp) == CONFIG_TRUE)
        g_cfg.solver_threshold = itmp;

    config_destroy(&cfg);   /* always destroy to free libconfig memory */
    return 0;
}

void config_apply_args(int inet_port, int soap_port,
                       const char *unix_socket, int log_level)
{
    if (inet_port   > 0)   g_cfg.inet_port  = (uint16_t)inet_port;
    if (soap_port   > 0)   g_cfg.soap_port  = (uint16_t)soap_port;
    if (unix_socket != NULL) {
        strncpy(g_cfg.unix_socket, unix_socket,
                sizeof(g_cfg.unix_socket) - 1);
        g_cfg.unix_socket[sizeof(g_cfg.unix_socket) - 1] = '\0';
    }
    if (log_level   >= 0)  g_cfg.log_level  = log_level;
}

void config_print(void)
{
    if (g_cfg.log_level < 1)
        return;

    fprintf(stderr,
            "[config] Active configuration:\n"
            "         inet_port   = %d\n"
            "         soap_port   = %d\n"
            "         unix_socket = %s\n"
            "         backlog     = %d\n"
            "         reuse_addr  = %d\n"
            "         log_level   = %d\n"
            "         log_dest    = %s\n"
            "         demo_string = %s\n"
            "         demo_int1   = %d\n"
            "         demo_int2   = %d\n"
            "         solver_workers   = %d\n"
            "         solver_threshold = %d\n",
            g_cfg.inet_port,
            g_cfg.soap_port,
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
