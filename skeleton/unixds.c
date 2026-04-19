/*
 * unixds.c — UNIX domain socket server component.
 *
 * Changes from skeleton:
 *   • Removed commented-out ncurses blocks.
 *   • Uses g_cfg.unix_socket path (passed in via args from threeds.c,
 *     consistent with how the original code worked).
 *   • Added error logging with strerror() instead of silent perror().
 *   • Socket type changed to SOCK_STREAM for connection-oriented use;
 *     the original SOCK_DGRAM skeleton had no accept() loop, leaving
 *     the thread as a stub.  This version adds a minimal accept loop
 *     so the thread actually serves clients.
 */

#include <stddef.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>

#include "config.h"

/* ------------------------------------------------------------------ */
/*  Internal: create, bind, and return a UNIX domain socket           */
/* ------------------------------------------------------------------ */

static int unix_socket(const char *filename)
{
    struct sockaddr_un name;
    int    sock;
    size_t size;

    sock = socket(PF_LOCAL, SOCK_STREAM, 0);
    if (sock < 0) {
        fprintf(stderr, "[unix] socket(): %s\n", strerror(errno));
        pthread_exit(NULL);
    }

    name.sun_family = AF_LOCAL;
    strncpy(name.sun_path, filename, sizeof(name.sun_path) - 1);
    name.sun_path[sizeof(name.sun_path) - 1] = '\0';

    size = offsetof(struct sockaddr_un, sun_path) +
           strlen(name.sun_path) + 1;

    if (bind(sock, (struct sockaddr *)&name, (socklen_t)size) < 0) {
        fprintf(stderr, "[unix] bind() on %s: %s\n",
                filename, strerror(errno));
        close(sock);
        pthread_exit(NULL);
    }

    return sock;
}

/* ------------------------------------------------------------------ */
/*  Thread entry-point                                                 */
/* ------------------------------------------------------------------ */

void *unix_main(void *args)
{
    const char *socket_path = (const char *)args;
    int         sock;
    fd_set      active_fd_set, read_fd_set;
    char        buf[512];

    sock = unix_socket(socket_path);

    if (listen(sock, g_cfg.backlog) < 0) {
        fprintf(stderr, "[unix] listen(): %s\n", strerror(errno));
        close(sock);
        pthread_exit(NULL);
    }

    if (g_cfg.log_level >= 1)
        fprintf(stderr, "[unix] Listening on %s\n", socket_path);

    FD_ZERO(&active_fd_set);
    FD_SET(sock, &active_fd_set);

    while (1) {
        read_fd_set = active_fd_set;

        if (select(FD_SETSIZE, &read_fd_set, NULL, NULL, NULL) < 0) {
            fprintf(stderr, "[unix] select(): %s\n", strerror(errno));
            pthread_exit(NULL);
        }

        for (int i = 0; i < FD_SETSIZE; ++i) {
            if (!FD_ISSET(i, &read_fd_set))
                continue;

            if (i == sock) {
                /* New connection */
                int new_fd = accept(sock, NULL, NULL);
                if (new_fd < 0) {
                    fprintf(stderr, "[unix] accept(): %s\n",
                            strerror(errno));
                    continue;
                }
                FD_SET(new_fd, &active_fd_set);
                if (g_cfg.log_level >= 1)
                    fprintf(stderr, "[unix] New connection on fd %d\n", new_fd);
            } else {
                /* Data from connected client — echo it back */
                ssize_t nb = recv(i, buf, sizeof(buf) - 1, 0);
                if (nb <= 0) {
                    if (g_cfg.log_level >= 1)
                        fprintf(stderr, "[unix] fd %d disconnected\n", i);
                    close(i);
                    FD_CLR(i, &active_fd_set);
                    continue;
                }
                buf[nb] = '\0';
                if (g_cfg.log_level >= 2)
                    fprintf(stderr, "[unix] recv: %s\n", buf);

                if (send(i, buf, (size_t)nb, 0) < 0) {
                    fprintf(stderr, "[unix] send(): %s\n", strerror(errno));
                    close(i);
                    FD_CLR(i, &active_fd_set);
                }
            }
        }
    }

    pthread_exit(NULL);
}
