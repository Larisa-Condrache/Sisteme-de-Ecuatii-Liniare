/* Componenta server pentru socket-uri UNIX domain.
 Modificari fata de skeleton: Foloseste calea g_cfg.unix_socket (primita prin args din threeds.c, am adaugat log de eroare cu strerror() in loc de perror() silentios.
   am schimbat socketul la SOCK_STREAM pentru utilizare orientata
     pe conexiune; skeleton-ul original cu SOCK_DGRAM nu avea bucla de accept(),Aceasta versiune adauga o bucla minima de accept()
     ca thread-ul sa deserveasca efectiv clientii.
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

/*   creeaza, face bind si returneaza un socket UNIX domain    */

static int unix_socket(const char *filename)
{
    struct sockaddr_un name;
    int sock;
    size_t size;

    sock = socket(PF_LOCAL, SOCK_STREAM, 0);
    if (sock < 0)
    {
        fprintf(stderr, "[unix] socket(): %s\n", strerror(errno));
        pthread_exit(NULL);
    }

    name.sun_family = AF_LOCAL;
    strncpy(name.sun_path, filename, sizeof(name.sun_path) - 1);
    name.sun_path[sizeof(name.sun_path) - 1] = '\0';

    size = offsetof(struct sockaddr_un, sun_path) +
           strlen(name.sun_path) + 1;

    if (bind(sock, (struct sockaddr *)&name, (socklen_t)size) < 0)
    {
        fprintf(stderr, "[unix] bind() on %s: %s\n",
                filename, strerror(errno));
        close(sock);
        pthread_exit(NULL);
    }

    return sock;
}

/*  Punct de intrare pentru thread                                     */

void *unix_main(void *args)
{
    const char *socket_path = (const char *)args;
    int sock;
    fd_set active_fd_set, read_fd_set;
    char buf[512];

    sock = unix_socket(socket_path);

    if (listen(sock, g_cfg.backlog) < 0)
    {
        fprintf(stderr, "[unix] listen(): %s\n", strerror(errno));
        close(sock);
        pthread_exit(NULL);
    }

    if (g_cfg.log_level >= 1)
        fprintf(stderr, "[unix] Listening on %s\n", socket_path);

    FD_ZERO(&active_fd_set);
    FD_SET(sock, &active_fd_set);

    while (1)
    {
        read_fd_set = active_fd_set;

        if (select(FD_SETSIZE, &read_fd_set, NULL, NULL, NULL) < 0)
        {
            fprintf(stderr, "[unix] select(): %s\n", strerror(errno));
            pthread_exit(NULL);
        }

        for (int i = 0; i < FD_SETSIZE; ++i)
        {
            if (!FD_ISSET(i, &read_fd_set))
                continue;

            if (i == sock)
            {
                /* Conexiune noua */
                int new_fd = accept(sock, NULL, NULL);
                if (new_fd < 0)
                {
                    fprintf(stderr, "[unix] accept(): %s\n",
                            strerror(errno));
                    continue;
                }
                FD_SET(new_fd, &active_fd_set);
                if (g_cfg.log_level >= 1)
                    fprintf(stderr, "[unix] New connection on fd %d\n", new_fd);
            }
            else
            {
                /* Date de la client conectat — le trimitem inapoi  */
                ssize_t nb = recv(i, buf, sizeof(buf) - 1, 0);
                if (nb <= 0)
                {
                    if (g_cfg.log_level >= 1)
                        fprintf(stderr, "[unix] fd %d disconnected\n", i);
                    close(i);
                    FD_CLR(i, &active_fd_set);
                    continue;
                }
                buf[nb] = '\0';
                if (g_cfg.log_level >= 2)
                    fprintf(stderr, "[unix] recv: %s\n", buf);

                if (send(i, buf, (size_t)nb, 0) < 0)
                {
                    fprintf(stderr, "[unix] send(): %s\n", strerror(errno));
                    close(i);
                    FD_CLR(i, &active_fd_set);
                }
            }
        }
    }

    pthread_exit(NULL);
}
