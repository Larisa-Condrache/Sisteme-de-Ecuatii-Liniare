/* === COMENTARII EXPLICITE PENTRU PREZENTARE ===
 * Comentariile adaugate mai jos marcheaza explicit UNDE se intampla fiecare pas important:
 * initializare, conectare, handshake, citire cerere, procesare, rezolvare, raspuns si inchidere.
 * Logica programului NU este modificata; au fost adaugate doar explicatii in cod.
 */

/*
 * Verificare build si analiza statica:
 *   • Acest fisier se compileaza din Makefile cu warning-uri stricte,
 *     pentru a detecta din timp erori frecvente de programare si probleme
 *     de portabilitate.
 *   • -Wall si -Wextra semnaleaza probleme generale precum variabile
 *     nefolosite, formate gresite sau apeluri suspecte.
 *   • -Wpedantic forteaza respectarea mai stricta a standardului C.
 *   • -Wshadow avertizeaza daca o variabila locala ascunde alta variabila,
 *     situatie care poate produce bug-uri greu de observat.
 *   • -Wconversion avertizeaza la conversii implicite care pot pierde
 *     informatie sau pot schimba semnul unei valori.
 *   • -Wformat=2 verifica mai strict folosirea functiilor din familia printf.
 *   • -Wnull-dereference semnaleaza dereferentieri potential invalide.
 *   • Fisierul este inclus si in tinta „make tidy”, unde clang-tidy face
 *     o analiza statica suplimentara pentru stil, siguranta si bune practici.
 */

/*
 * unixds.c — Componenta server pentru socket-uri UNIX domain.
 *
 * Modificari fata de skeleton:
 *   • Am eliminat blocurile ncurses comentate.
 *   • Foloseste calea g_cfg.unix_socket (primita prin args din threeds.c,
 *     in acelasi mod in care functiona codul original).
 *   • Am adaugat log de eroare cu strerror() in loc de perror() silentios.
 *   • Tipul socket-ului a fost schimbat la SOCK_STREAM pentru utilizare orientata
 *     pe conexiune; skeleton-ul original cu SOCK_DGRAM nu avea bucla de accept(),
 *     lasand thread-ul ca stub. Aceasta versiune adauga o bucla minima de accept()
 *     ca thread-ul sa deserveasca efectiv clientii.
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
/*  Intern: creeaza, face bind si returneaza un socket UNIX domain    */
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
/*  Punct de intrare pentru thread                                     */
/* ------------------------------------------------------------------ */

void *unix_main(void *args)
{
    const char *socket_path = (const char *)args;
    int         sock;
    fd_set      active_fd_set, read_fd_set;
    char        buf[512];

    sock = unix_socket(socket_path);

    /* AICI serverul UNIX incepe sa asculte conexiuni locale. */
    if (listen(sock, g_cfg.backlog) < 0) {
        fprintf(stderr, "[unix] listen(): %s\n", strerror(errno));
        close(sock);
        pthread_exit(NULL);
    }

    if (g_cfg.log_level >= 1)
        fprintf(stderr, "[unix] Listening on %s\n", socket_path);

    FD_ZERO(&active_fd_set);
    FD_SET(sock, &active_fd_set);

    /* AICI ruleaza bucla principala a serverului UNIX. */
    while (1) {
        read_fd_set = active_fd_set;

        if (select(FD_SETSIZE, &read_fd_set, NULL, NULL, NULL) < 0) {
            fprintf(stderr, "[unix] select(): %s\n", strerror(errno));
            pthread_exit(NULL);
        }

        for (int i = 0; i < FD_SETSIZE; ++i) {
            if (!FD_ISSET(i, &read_fd_set))
                continue;

            /* AICI este acceptata o conexiune noua pe socketul UNIX. */
            if (i == sock) {
                /* Conexiune noua */
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
                /* Date de la client conectat — le trimitem inapoi (echo) */
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
