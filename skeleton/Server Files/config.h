/*
 Populata o singura data la pornire prin config_load() dintr-un fisier
 .cfg libconfig si/sau argumente CLI. Toate celelalte module includ
 acest header si citesc din instanta globala g_cfg de mai jos.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

/*  Structura de configurare la rulare   */

typedef struct server_config {
    uint16_t    inet_port;          /* port de ascultare TCP/INET      */
    uint16_t    soap_port;          /* port de ascultare SOAP    */
    char        unix_socket[256];   /* cale socket UNIX domain         */
    int         backlog;            /* backlog pentru listen()         */
    int         reuse_addr;         /* flag SO_REUSEADDR               */

    int         log_level;          /* 0=error 1=info 2=debug          */
    char        log_dest[256];      /* "stderr" sau cale catre fisier  */

    char        demo_string[256];
    int         demo_int1;
    int         demo_int2;

    int         solver_workers;     /* numar de procese copil (fork)      */
    int         solver_threshold;   /* cale seriala sub acest n           */
} server_config_t;

extern server_config_t g_cfg;

/*
config_load() parseaza 'cfg_path' cu libconfig si populeaza g_cfg si o sa returneze 0 pt succes sau -1 daca fisierul nu se poate deschide. Cheile lipsa sunt inlocuite cu valorile implicite, de la inceputul lui config.c.
 */
int config_load(const char *cfg_path);

/*
 config_apply_args() suprascrie valori din g_cfg cu optiunile din CLI si returneaza -1 / NULL pentru argumentele care nu au fost date de user.
 */
void config_apply_args(int inet_port, int soap_port,
                       const char *unix_socket, int log_level);

/*
config_print() afiseaza configuratia activa la stderr.
 */
void config_print(void);

#endif /* CONFIG_H */
