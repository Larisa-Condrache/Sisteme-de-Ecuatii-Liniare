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
Modificari fata de skeleton: am eliminat #include <ncurses.h>, portul si backlog-ul sunt acum citite din g_cfg in loc de constante hardcodate, am inlocuit apelurile de debug writecu fprint si am inlocuit toate perror() cu fprintf

 */

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "proto.h"
#include "config.h"
#include "solver.h"

/*  creeam si facem bind la un socket TCP de ascultare        */

static int inet_socket(uint16_t port, int reuse)
{
    int sock;
    struct sockaddr_in name;

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        fprintf(stderr, "[inet] socket(): %s\n", strerror(errno));
        pthread_exit(NULL);
    }

    if (reuse) {
        int on = 1;
        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
                       &on, sizeof(on)) < 0) {
            fprintf(stderr, "[inet] setsockopt(SO_REUSEADDR): %s\n",
                    strerror(errno));
            close(sock);
            pthread_exit(NULL);
        }
    }

    name.sin_family      = AF_INET;
    name.sin_port        = htons(port);
    name.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr *)&name, sizeof(name)) < 0) {
        fprintf(stderr, "[inet] bind() on port %d: %s\n",
                port, strerror(errno));
        close(sock);
        pthread_exit(NULL);
    }

    return sock;
}


/*
create_client_id() genereaza un id si foloseste timestamp-ul Unix
Punct de intrare pentru thread-ul INET*/


void *inet_main(void *args)
{
    int              port = *((int *)args);
    int              sock;
    fd_set           active_fd_set, read_fd_set;
    struct sockaddr_in clientname;
    socklen_t        clen;

    /* AICI se creeaza si se face bind pe socketul TCP de ascultare. */
    sock = inet_socket((uint16_t)port, g_cfg.reuse_addr);

    /* AICI socketul devine socket de ascultare pentru clienti noi. */
    if (listen(sock, g_cfg.backlog) < 0) {
        fprintf(stderr, "[inet] listen(): %s\n", strerror(errno));
        close(sock);
        pthread_exit(NULL);
    }

    if (g_cfg.log_level >= 1)
        fprintf(stderr, "[inet] Listening on port %d\n", port);

    FD_ZERO(&active_fd_set);
    FD_SET(sock, &active_fd_set);

    /* AICI incepe bucla principala a serverului TCP; ruleaza permanent. */
    while (1) {
        read_fd_set = active_fd_set;

        /* AICI serverul asteapta activitate pe orice descriptor monitorizat. */
        if (select(FD_SETSIZE, &read_fd_set, NULL, NULL, NULL) < 0) {
            fprintf(stderr, "[inet] select(): %s\n", strerror(errno));
            pthread_exit(NULL);
        }

        for (int i = 0; i < FD_SETSIZE; ++i) {
            if (!FD_ISSET(i, &read_fd_set))
                continue;

            /* Daca descriptorul activ este chiar socketul de ascultare, AICI vine un client nou. */
            if (i == sock) {
                /* Conexiune noua  */
                int new_fd;
                clen   = sizeof(clientname);
                /* AICI conexiunea noua este acceptata si se obtine socketul dedicat clientului. */
                new_fd = accept(sock,
                                (struct sockaddr *)&clientname, &clen);
                if (new_fd < 0) {
                    fprintf(stderr, "[inet] accept(): %s\n",
                            strerror(errno));
                    pthread_exit(NULL);
                }
                FD_SET(new_fd, &active_fd_set);

                if (g_cfg.log_level >= 1)
                    fprintf(stderr, "[inet] New connection on fd %d from %s\n",
                            new_fd, inet_ntoa(clientname.sin_addr));

            } else {
                /* Date de la un client deja conectat */
                /* AICI serverul se uita in header fara sa consume mesajul, pentru a afla operatia ceruta. */
                msgHeaderType h = peekMsgHeader(i);
                int clientID    = h.clientID;

                if (clientID < 0) {
                    fprintf(stderr,
                            "[inet] Protocol error: negative clientID on fd %d."
                            " Closing.\n", i);
                    close(i);
                    FD_CLR(i, &active_fd_set);
                    continue;
                }

                /*  client nou (clientID == 0) */
                /* AICI se detecteaza handshake-ul initial: client nou, fara ID asignat inca. */
                if (clientID == 0) {
                    int         newID;
                    msgIntType  m;

                    /* AICI serverul genereaza ID-ul unic pentru clientul nou conectat. */
                    newID = create_client_id();

                    if (g_cfg.log_level >= 1)
                        fprintf(stderr,
                                "[inet] New client on fd %d → assigned ID %d\n",
                                i, newID);

                    if (readSingleInt(i, &m) < 0) {
                        close(i);
                        FD_CLR(i, &active_fd_set);
                        continue;
                    }

                    /*  ID-ul alocat */
                    if (writeSingleInt(i, h, newID) < 0) {
                        close(i);
                        FD_CLR(i, &active_fd_set);
                    }
                    continue;
                }
                int operation = h.opID;

                if (operation < 0) {
                    fprintf(stderr,
                            "[inet] Protocol error: bad opID on fd %d."
                            " Closing.\n", i);
                    close(i);
                    FD_CLR(i, &active_fd_set);
                    continue;
                }

                switch (operation) {

                    /* AICI se trateaza cererea de tip ECHO: serverul citeste un string si il trimite inapoi neschimbat. */
                    case OPR_ECHO: {
                        msgStringType str;
                        if (readSingleString(i, &str) < 0) {
                            close(i); FD_CLR(i, &active_fd_set); break;
                        }
                        if (g_cfg.log_level >= 2)
                            fprintf(stderr, "[inet] ECHO: %s\n", str.msg);

                        if (writeSingleString(i, h, str.msg) < 0) {
                            close(i); FD_CLR(i, &active_fd_set);
                        }
                        free(str.msg);
                        break;
                    }

                    /* AICI se trateaza cererea de concatenare a doua stringuri. */
                    case OPR_CONC: {
                        /* Primeste doua siruri si trimite inapoi sirurile concatenate */
                        msgStringType s1, s2;
                        char         *result;
                        size_t        rlen;

                        if (readSingleString(i, &s1) < 0) {
                            close(i); FD_CLR(i, &active_fd_set); break;
                        }
                        if (readSingleString(i, &s2) < 0) {
                            free(s1.msg);
                            close(i); FD_CLR(i, &active_fd_set); break;
                        }

                        rlen   = strlen(s1.msg) + strlen(s2.msg) + 2;
                        result = (char *)malloc(rlen);
                        if (result == NULL) {
                            free(s1.msg); free(s2.msg);
                            close(i); FD_CLR(i, &active_fd_set); break;
                        }
                        snprintf(result, rlen, "%s %s", s1.msg, s2.msg);

                        if (g_cfg.log_level >= 2)
                            fprintf(stderr, "[inet] CONC: %s\n", result);

                        if (writeSingleString(i, h, result) < 0) {
                            close(i); FD_CLR(i, &active_fd_set);
                        }
                        free(s1.msg); free(s2.msg); free(result);
                        break;
                    }

                    /* AICI se trateaza negarea unui numar intreg. */
                    case OPR_NEG: {
                        msgIntType m;
                        if (readSingleInt(i, &m) < 0) {
                            close(i); FD_CLR(i, &active_fd_set); break;
                        }
                        m.msg = -m.msg;
                        if (g_cfg.log_level >= 2)
                            fprintf(stderr, "[inet] NEG result: %d\n", m.msg);
                        if (writeSingleInt(i, h, m.msg) < 0) {
                            close(i); FD_CLR(i, &active_fd_set);
                        }
                        break;
                    }

                    /* AICI se trateaza adunarea a doua numere intregi. */
                    case OPR_ADD: {
                        msgIntType m1, m2;
                        if (readMultiInt(i, &m1, &m2) < 0) {
                            close(i); FD_CLR(i, &active_fd_set); break;
                        }
                        int sum = m1.msg + m2.msg;
                        if (g_cfg.log_level >= 2)
                            fprintf(stderr, "[inet] ADD: %d + %d = %d\n",
                                    m1.msg, m2.msg, sum);
                        if (writeSingleInt(i, h, sum) < 0) {
                            close(i); FD_CLR(i, &active_fd_set);
                        }
                        break;
                    }

                    /* AICI se trateaza cererea complexa OPR_SOLVE pentru rezolvarea sistemului liniar Ax=b. */
                    case OPR_SOLVE: {
                        /* Primeste sistemul liniar A x = b de la client, il rezolva prin LAPACKE, apoi trimite inapoi vectorul x.
                         */
                        int     n   = 0;
                        double *A   = NULL;
                        double *b   = NULL;

                        if (readSolveRequest(i, &n, &A, &b) < 0) {
                            fprintf(stderr,
                                    "[inet] OPR_SOLVE: failed to read"
                                    " request on fd %d\n", i);
                            close(i); FD_CLR(i, &active_fd_set);
                            break;
                        }

                        if (g_cfg.log_level >= 1)
                            fprintf(stderr,
                                    "[inet] OPR_SOLVE: n=%d,"
                                    " workers=%d\n",
                                    n, g_cfg.solver_workers);

                        /* AICI serverul apeleaza modulul numeric care decide daca rezolva serial sau paralel. */
                        solver_result_t res =
                            solver_solve(n, A, b, g_cfg.solver_workers);

                        free(A); free(b);

                        /* AICI serverul trimite inapoi catre client statusul si vectorul solutie. */
                        if (writeSolveResponse(i, h,
                                               res.status, n, res.x) < 0) {
                            fprintf(stderr,
                                    "[inet] OPR_SOLVE: failed to send"
                                    " response on fd %d\n", i);
                            close(i); FD_CLR(i, &active_fd_set);
                        }

                        if (res.x) free(res.x);

                        if (g_cfg.log_level >= 1)
                            fprintf(stderr,
                                    "[inet] OPR_SOLVE: done (status=%d)\n",
                                    res.status);
                        break;
                    }

                    /* AICI conexiunea este inchisa explicit de client sau de server pentru operatie necunoscuta. */
                    case OPR_BYE:
                    default:
                        if (g_cfg.log_level >= 1)
                            fprintf(stderr,
                                    "[inet] BYE/unknown op on fd %d. Closing.\n",
                                    i);
                        close(i);
                        FD_CLR(i, &active_fd_set);
                        break;
                }
            }
        }
    }

    pthread_exit(NULL);
}
