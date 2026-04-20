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

    g_cfg.backlog = DEFAULT_BACKLOG;
    g_cfg.reuse_addr = DEFAULT_REUSE_ADDR;
}