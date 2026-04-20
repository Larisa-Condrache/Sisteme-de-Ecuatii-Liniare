/*
 * config.h — Centralized runtime configuration for the PCD server.
 *
 * Populated once at startup by config_load() from a libconfig .cfg file
 * and/or CLI arguments.  All other modules include this header and read
 * from the global g_cfg instance declared below.
 *
 * Build dependency: libconfig  (sudo apt install libconfig-dev)
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  Runtime configuration structure                                    */
/* ------------------------------------------------------------------ */

typedef struct server_config {
    /* Network */
    uint16_t    inet_port;          /* TCP/INET listener port          */
    uint16_t    soap_port;          /* SOAP/gsoap listener port        */
    char        unix_socket[256];   /* UNIX domain socket path         */
    int         backlog;            /* listen() backlog                */
    int         reuse_addr;         /* SO_REUSEADDR flag               */

    /* Logging */
    int         log_level;          /* 0=error 1=info 2=debug          */
    char        log_dest[256];      /* "stderr" or file path           */

    /* Demo (fork + libconfig showcase) */
    char        demo_string[256];
    int         demo_int1;
    int         demo_int2;

    /* T26 — Linear system solver */
    int         solver_workers;     /* number of child processes to fork  */
    int         solver_threshold;   /* serial path below this n           */
} server_config_t;

/* ------------------------------------------------------------------ */
/*  Global instance — defined once in config.c, extern everywhere     */
/* ------------------------------------------------------------------ */

extern server_config_t g_cfg;

/* ------------------------------------------------------------------ */
/*  API                                                                */
/* ------------------------------------------------------------------ */

/*
 * config_load() — Parse 'cfg_path' with libconfig, populate g_cfg.
 *
 * Returns  0 on success,
 *         -1 if the file cannot be opened or parsed.
 *
 * Missing keys are silently replaced by the compiled-in defaults
 * defined at the top of config.c.
 */
int config_load(const char *cfg_path);

/*
 * config_apply_args() — Override g_cfg values from parsed CLI options.
 *
 * Call this AFTER config_load() so CLI flags win over the file.
 *
 * Pass -1 / NULL for arguments that were not supplied by the user.
 */
void config_apply_args(int inet_port, int soap_port,
                       const char *unix_socket, int log_level);

/*
 * config_print() — Dump the active configuration to stderr.
 * Controlled by g_cfg.log_level (only prints when level >= 1).
 */
void config_print(void);

#endif /* CONFIG_H */
